import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SKILL = REPO / '.agents/skills/releasing-onnxruntime-server'
MOCK = r'''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys
name=pathlib.Path(sys.argv[0]).name
a=sys.argv[1:]
mode=os.environ.get('MOCK_MODE', '')
state=pathlib.Path(os.environ['MOCK_STATE'])
with (state / 'calls').open('a') as f: f.write(json.dumps([name]+a)+'\n')
if name == 'git':
    if mode == 'git-unavailable': sys.exit(1)
    if a[:2] == ['rev-parse', '--show-toplevel']: print(os.environ['MOCK_REPO'])
    elif a[:2] == ['rev-parse', 'HEAD']: print('1'*40)
    elif a[:2] == ['rev-parse', 'HEAD^{tree}']: print('2'*40)
    elif a[0] == 'status': pass
    else: sys.exit(1)
elif name == 'gh':
    endpoint=a[1]
    if '/releases' in endpoint:
        if endpoint.endswith('/releases'):
            if mode == 'notes-fail': sys.exit(1)
            print('v1.30.0\nv1.29.1')
        else: print('release notes')
    elif 'contents' in endpoint:
        if 'azure-pipelines?' in endpoint: print('c-api-noopenmp-packaging-pipelines.yml')
        elif 'azure-pipelines' in endpoint or 'Dockerfile.' in endpoint: print('CUDA=12.8 cuDNN=9.14; complete source')
        elif '--jq' in a: print('onnxruntime_cxx_api.h' if mode != 'headers-empty' else '')
        else:
            print('struct SessionOptionsImpl {\nSessionOptionsImpl& SetGraphOptimizationLevel(int level);\n};\n#define ORT_ENABLE_ALL 99')
elif name == 'curl':
    urls=[x for x in a if x.startswith('http')]
    url=urls[0]
    if '-o' in a:
        shutil.copyfile(os.environ['MOCK_ARCHIVE'], a[a.index('-o')+1])
    elif '/api/' in url:
        if url.endswith('/version'): print('1.30.0')
        elif '-fsSX' in a and 'POST' in a and url.endswith('/sessions/sample/2'):
            print(json.dumps({'option': {}} if mode == 'missing-output' else {'output': [0.5]}))
        else: print(json.dumps({'option': {'cuda': {'device_id': 0}}}))
    elif '/releases/tags/' in url:
        if mode == 'api-fail': sys.exit(22)
        if mode == 'json-fail': print('not json'); sys.exit(0)
        version=url.rsplit('/v',1)[1]
        tag='v1.99.0' if mode == 'wrong-tag' else 'v'+version
        names=[] if mode == 'no-asset' else ['onnxruntime-linux-x64-gpu_cuda12-'+version+'.tgz', 'onnxruntime-linux-x64-'+version+'.tgz']
        print(json.dumps({'tag_name': tag, 'assets': [{'name': n, 'browser_download_url': 'https://example.invalid/'+n} for n in names]}))
    elif '/token?' in url: print('{"token":"mock-token"}')
    elif '/manifests/' in url: print(json.dumps({'config': {'digest': 'sha256:'+'a'*64}}))
    elif '/blobs/' in url:
        print(json.dumps({'config': {'Env': ['CUDA_VERSION=12.8.1', 'NV_CUDNN_VERSION=9.14', 'NVIDIA_REQUIRE_CUDA=cuda>=12.8 brand=tesla,driver>=525,driver<580', 'NVIDIA_REQUIRE_ARCH=arch>=7.5']}}))
    else: sys.exit(1)
elif name == 'docker':
    if a[0] == 'run':
        if mode == 'test-fail' and 'linux-cuda13' in a[-1]: sys.exit(1)
        print('mock-container')
    elif a[0] == 'port': print('127.0.0.1:43111')
    elif a[:2] == ['buildx','build']:
        if mode == 'build-fail' and any('linux-cuda13' in arg for arg in a): sys.exit(1)
        if mode == 'push-fail' and '--push' in a and any('linux-cuda12' in arg for arg in a): sys.exit(1)
    elif a[0] in ('rm','cp','logs'): pass
    else: sys.exit(1)
'''


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='onnx-release-tests-')
        self.root = Path(self.temp.name)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.env = dict(os.environ, PATH=f'{self.bin}:{os.environ["PATH"]}', MOCK_STATE=str(self.root), MOCK_REPO=str(REPO))
        self.env.pop('GITHUB_TOKEN', None)
        for name in ('git', 'gh', 'curl', 'docker'):
            path = self.bin / name
            path.write_text(MOCK)
            path.chmod(0o755)

    def tearDown(self):
        self.temp.cleanup()

    def run_bash(self, command, mode='', cwd=REPO):
        env = dict(self.env, MOCK_MODE=mode)
        return subprocess.run(['bash', '-c', command], env=env, cwd=cwd, text=True, capture_output=True)

    def calls(self):
        return [json.loads(line) for line in (self.root / 'calls').read_text().splitlines()]

    def assets(self, mode=''):
        return self.run_bash(f'set -euo pipefail; source "{REPO}/deploy/build-docker/release-assets.sh"; ort_asset 1.29.1 onnxruntime-linux-x64-1.29.1.tgz', mode)

    def test_asset_download_is_version_pinned(self):
        result = self.assets()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('1.29.1.tgz', result.stdout)
        self.assertTrue(any('/releases/tags/v1.29.1' in arg for c in self.calls() for arg in c))
        self.assertFalse(any('/latest' in arg for c in self.calls() for arg in c))

    def test_asset_absence_is_distinct_from_api_failure(self):
        self.assertEqual(self.assets('no-asset').returncode, 3)
        for mode in ('api-fail', 'json-fail', 'wrong-tag'):
            self.assertNotIn(self.assets(mode).returncode, (0, 3), mode)

    def test_install_preserves_existing_destination(self):
        dest = self.root / 'runtime'
        dest.mkdir()
        (dest / 'keep').write_text('existing')
        r = self.run_bash(f'bash "{SKILL}/scripts/install-onnxruntime-gpu.sh" 1.30.0 "{dest}"')
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual((dest / 'keep').read_text(), 'existing')
        self.assertFalse(any(c[0] == 'curl' for c in self.calls()))

    def make_archive(self, version='1.30.0', complete=True):
        source = self.root / 'onnxruntime-linux-x64-gpu_cuda12-1.30.0'
        (source / 'include').mkdir(parents=True)
        (source / 'lib').mkdir()
        (source / 'VERSION_NUMBER').write_text(version)
        (source / 'include/onnxruntime_cxx_api.h').write_text('header')
        if complete:
            (source / 'lib/libonnxruntime.so').write_text('core')
            (source / 'lib/libonnxruntime_providers_cuda.so').write_text('cuda')
        archive = self.root / 'runtime.tgz'
        with tarfile.open(archive, 'w:gz') as tar:
            tar.add(source, arcname=source.name)
        self.env['MOCK_ARCHIVE'] = str(archive)

    def test_install_validates_and_installs_local_archive(self):
        self.make_archive()
        dest = self.root / 'runtime'
        r = self.run_bash(f'bash "{SKILL}/scripts/install-onnxruntime-gpu.sh" 1.30.0 "{dest}"')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((dest / 'VERSION_NUMBER').read_text(), '1.30.0')
        self.assertTrue((dest / 'lib/libonnxruntime_providers_cuda.so').exists())
        self.assertFalse(any(c[0] == 'sudo' for c in self.calls()))

    def test_install_rejects_wrong_archive_version(self):
        self.make_archive(version='1.99.0')
        dest = self.root / 'runtime'
        r = self.run_bash(f'bash "{SKILL}/scripts/install-onnxruntime-gpu.sh" 1.30.0 "{dest}"')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(dest.exists())

    def test_install_rejects_incomplete_archive(self):
        self.make_archive(complete=False)
        dest = self.root / 'runtime'
        r = self.run_bash(f'bash "{SKILL}/scripts/install-onnxruntime-gpu.sh" 1.30.0 "{dest}"')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(dest.exists())

    def test_surface_success(self):
        r = self.run_bash(f'bash "{SKILL}/scripts/ort-surface-diff.sh" 1.29.1 1.30.0 "{self.root}/surface"')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue((self.root / 'surface/notes-1.30.0.md').exists())
        self.assertEqual((self.root / 'surface/setters.diff').read_text(), '')

    def test_surface_listing_failure_never_reports_success(self):
        r = self.run_bash(f'bash "{SKILL}/scripts/ort-surface-diff.sh" 1.29.1 1.30.0 "{self.root}/surface"', 'notes-fail')
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('Complete review artifacts', r.stdout)

    def test_empty_header_list_fails(self):
        r = self.run_bash(f'bash "{SKILL}/scripts/ort-surface-diff.sh" 1.29.1 1.30.0 "{self.root}/surface"', 'headers-empty')
        self.assertNotEqual(r.returncode, 0)

    def test_cuda_keeps_full_constraints(self):
        r = self.run_bash(f'bash "{SKILL}/scripts/ort-cuda-base-images.sh" 1.30.0 12.8.1-cudnn-runtime-ubuntu24.04')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('cuda>=12.8 brand=tesla,driver>=525,driver<580', r.stdout)
        self.assertIn('NVIDIA_REQUIRE_ARCH=arch>=7.5', r.stdout)
        self.assertTrue(all('rel-' not in arg for c in self.calls() for arg in c))

    def build_workspace(self):
        project = self.root / 'project'
        scripts = project / 'deploy/build-docker'
        scripts.mkdir(parents=True)
        for file in ('build.sh', 'docker-image-test.sh'):
            shutil.copyfile(REPO / 'deploy/build-docker' / file, scripts / file)
        (scripts / 'VERSION').write_text('export VERSION=1.30.0\nexport ORT_VERSION=1.30.0\nexport IMAGE_PREFIX=kibaes/onnxruntime-server\n')
        return project

    def build(self, args='', mode=''):
        project = self.build_workspace()
        return self.run_bash(f'bash deploy/build-docker/build.sh {args}', mode, project)

    def test_release_uses_expected_tags_and_platforms_after_all_tests(self):
        r = self.build()
        self.assertEqual(r.returncode, 0, r.stderr)
        calls = self.calls()
        tests = [i for i, c in enumerate(calls) if c[:2] == ['docker', 'run']]
        uploads = [i for i, c in enumerate(calls) if '--push' in c]
        self.assertEqual(len(tests), 3)
        self.assertEqual(len(uploads), 3)
        self.assertLess(max(tests), min(uploads))
        for i, variant in zip(uploads, ('linux-cpu', 'linux-cuda12', 'linux-cuda13')):
            c = calls[i]
            self.assertEqual(c[c.index('-t') + 1], f'kibaes/onnxruntime-server:1.30.0-{variant}')
            expected = 'linux/amd64,linux/arm64' if variant == 'linux-cpu' else 'linux/amd64'
            self.assertEqual(c[c.index('--platform') + 1], expected)
        self.assertNotIn('--gpus', calls[tests[0]])
        self.assertIn('--gpus', calls[tests[1]])
        self.assertIn('--gpus', calls[tests[2]])

    def test_local_build_runs_tests_without_git_or_uploads(self):
        r = self.build('--local', 'git-unavailable')
        self.assertEqual(r.returncode, 0, r.stderr)
        calls = self.calls()
        self.assertEqual(len([c for c in calls if '--load' in c]), 3)
        self.assertEqual(len([c for c in calls if c[:2] == ['docker', 'run']]), 3)
        self.assertFalse(any('--push' in c or 'imagetools' in c for c in calls))
        self.assertFalse(any(c[0] == 'git' for c in calls))

    def test_dry_run_prints_actual_release_commands_without_running_them(self):
        r = self.build('--dry-run')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse((self.root / 'calls').exists())
        self.assertEqual(r.stdout.count('--push'), 3)
        self.assertIn('-t kibaes/onnxruntime-server:1.30.0-linux-cpu', r.stdout)
        self.assertNotIn('candidate-', r.stdout)

    def test_local_dry_run_has_no_push_commands(self):
        r = self.build('--local --dry-run')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('--push', r.stdout)
        self.assertFalse((self.root / 'calls').exists())

    def test_target_selection(self):
        project = self.build_workspace()
        for target, variants in (('cpu', ['linux-cpu']), ('cuda', ['linux-cuda12', 'linux-cuda13'])):
            with self.subTest(target=target):
                (self.root / 'calls').unlink(missing_ok=True)
                r = self.run_bash(f'bash deploy/build-docker/build.sh --target={target}', cwd=project)
                self.assertEqual(r.returncode, 0, r.stderr)
                uploads = [c for c in self.calls() if '--push' in c]
                self.assertEqual([c[c.index('-t') + 1] for c in uploads],
                                 [f'kibaes/onnxruntime-server:1.30.0-{v}' for v in variants])

    def test_local_failures_prevent_all_uploads(self):
        project = self.build_workspace()
        for mode in ('build-fail', 'test-fail', 'missing-output'):
            with self.subTest(mode=mode):
                (self.root / 'calls').unlink(missing_ok=True)
                r = self.run_bash('bash deploy/build-docker/build.sh', mode, project)
                self.assertNotEqual(r.returncode, 0)
                self.assertFalse(any('--push' in c for c in self.calls()))

    def test_push_failure_stops_remaining_uploads(self):
        r = self.build(mode='push-fail')
        self.assertNotEqual(r.returncode, 0)
        uploads = [c for c in self.calls() if '--push' in c]
        self.assertEqual(len(uploads), 2)
        self.assertTrue(uploads[-1][uploads[-1].index('-t') + 1].endswith('linux-cuda12'))

    def test_unknown_or_removed_options_do_not_run_commands(self):
        project = self.build_workspace()
        for arg in ('--target=typo', '--prepare', '--publish=receipt.json'):
            with self.subTest(arg=arg):
                r = self.run_bash(f'bash deploy/build-docker/build.sh {arg}', cwd=project)
                self.assertNotEqual(r.returncode, 0)
                self.assertFalse((self.root / 'calls').exists())

    def test_version_update_separates_server_and_runtime(self):
        project = self.root / 'project'
        files=['README.md','docs/docker.md','docs/swagger/openapi.yaml','deploy/build-docker/VERSION','deploy/build-docker/docker-compose.yaml','deploy/build-docker/README.md','src/test/test_lib_version.cpp']
        for file in files:
            p=project/file
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('version 1.30.0\nkeep 1x30x0\n')
        (project/'deploy/build-docker/VERSION').write_text('export VERSION=1.30.0\nexport ORT_VERSION=1.29.1\n')
        (project/'src/test/test_lib_version.cpp').write_text('EXPECT_EQ(onnxruntime_server::onnx::version(), "1.29.1");\n')
        shutil.copyfile(REPO/'deploy/update-version.sh', project/'deploy/update-version.sh')
        r=self.run_bash('bash deploy/update-version.sh 1.30.0 1.30.0a 1.30.0', cwd=project)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('keep 1x30x0', (project/'README.md').read_text())
        self.assertIn('export VERSION=1.30.0a', (project/'deploy/build-docker/VERSION').read_text())
        self.assertIn('export ORT_VERSION=1.30.0\n', (project/'deploy/build-docker/VERSION').read_text())
        self.assertIn('"1.30.0"', (project/'src/test/test_lib_version.cpp').read_text())


class CIGateTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('release_ci', SKILL/'scripts/check-release-ci.py')
        self.ci=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.ci)
        self.pr={'state':'open','head':{'sha':'head'},'base':{'ref':'main','sha':'base'},'merge_commit_sha':'merge','mergeable':True,'mergeable_state':'clean'}
        self.runs=[{'name':name,'head_sha':'head','event':'pull_request','pull_requests':[{'number':125}],'id':i,'run_attempt':1,'status':'completed','conclusion':'success'} for i,name in enumerate(('CMake on Linux','CMake on Windows','CMake on MacOS','CodeQL'))]
        self.checks=[{'name':'build','status':'completed','conclusion':'success'}]
        self.main='base'
        self.reads=0
        self.moved=False
        def api(endpoint):
            if '/pulls/' in endpoint:
                self.reads+=1
                p=copy.deepcopy(self.pr)
                if self.moved and self.reads>1: p['head']['sha']='new-head'
                return p
            if '/actions/runs?' in endpoint: return {'workflow_runs':self.runs}
            if '/check-runs?' in endpoint: return {'check_runs':self.checks}
            if '/status' in endpoint: return {'state':'success','statuses':[]}
            if '/git/ref/' in endpoint: return {'object':{'sha':self.main}}
            raise AssertionError(endpoint)
        self.ci.api=api

    def gate(self):
        with contextlib.redirect_stdout(io.StringIO()): self.ci.check('kibae/onnxruntime-server', 125, 'head')

    def test_complete_current_ci_passes(self): self.gate()
    def test_old_head_does_not_satisfy_gate(self):
        self.runs[0]['head_sha']='old-head'
        with self.assertRaises(ValueError): self.gate()
    def test_latest_failure_beats_old_success(self):
        r=copy.deepcopy(self.runs[0]); r.update(id=99,conclusion='failure'); self.runs.append(r)
        with self.assertRaises(ValueError): self.gate()
    def test_missing_required_workflow_fails(self):
        self.runs.pop()
        with self.assertRaises(ValueError): self.gate()
    def test_main_movement_fails(self):
        self.main='new-base'
        with self.assertRaises(ValueError): self.gate()
    def test_pr_head_movement_fails(self):
        self.moved=True
        with self.assertRaises(ValueError): self.gate()
    def test_merge_commit_movement_fails(self):
        original = self.ci.api
        def api(endpoint):
            result = original(endpoint)
            if '/pulls/' in endpoint and self.reads > 1:
                result['merge_commit_sha'] = 'new-merge'
            return result
        self.ci.api = api
        with self.assertRaises(ValueError): self.gate()
    def test_skipped_check_fails(self):
        self.checks[0]['conclusion']='skipped'
        with self.assertRaises(ValueError): self.gate()
    def test_check_pagination(self):
        self.ci.api=lambda endpoint: {'check_runs': list(range(100)) if endpoint.endswith('&page=1') else [100]}
        self.assertEqual(len(self.ci.pages('checks?filter=latest','check_runs')),101)


if __name__ == '__main__': unittest.main(verbosity=2)
