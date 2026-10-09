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
import hashlib, io, json, os, pathlib, shutil, sys, tarfile
name=pathlib.Path(sys.argv[0]).name
a=sys.argv[1:]
mode=os.environ.get('MOCK_MODE', '')
state=pathlib.Path(os.environ['MOCK_STATE'])
config_bytes=json.dumps({'architecture':'amd64','os':'linux','rootfs':{'type':'layers','diff_ids':[]}}).encode()
config_digest='sha256:'+hashlib.sha256(config_bytes).hexdigest()
with (state / 'calls').open('a') as f: f.write(json.dumps([name]+a)+'\n')
if name == 'git':
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
    elif a[:2] == ['image','inspect']: print('sha256:'+'e'*64)
    elif a[:2] == ['image','save']:
        with tarfile.open(a[a.index('--output')+1], 'w') as archive:
            manifest=json.dumps([{'Config':'config.json','RepoTags':[],'Layers':[]}]).encode()
            for filename,data in [('config.json',config_bytes),('manifest.json',manifest)]:
                entry=tarfile.TarInfo(filename);entry.size=len(data);archive.addfile(entry,io.BytesIO(data))
    elif a[:2] == ['buildx','build']: pass
    elif a[:3] == ['buildx','imagetools','inspect']:
        image=a[3]
        if '--raw' in a:
            print(json.dumps({'config': {'digest':'sha256:'+'c'*64 if mode == 'config-mismatch' else config_digest}}))
        elif '--format' in a and a[a.index('--format')+1] == '{{.Manifest.Digest}}':
            print('Name: '+image+'\nDigest: sha256:'+'b'*64)
        elif mode == 'registry-invalid-digest': print(json.dumps({'digest':'invalid'}))
        elif ':candidate-' in image: print(json.dumps({'digest':'sha256:'+'b'*64}))
        elif mode == 'tag-conflict': print(json.dumps({'digest':'sha256:'+'f'*64}))
        elif mode == 'registry-fail': print('authentication failed', file=sys.stderr); sys.exit(1)
        elif mode == 'registry-not-found': print('registry endpoint not found', file=sys.stderr); sys.exit(1)
        elif (state/'published.json').exists() and image in json.loads((state/'published.json').read_text()):
            print(json.dumps({'digest':json.loads((state/'published.json').read_text())[image]}))
        else: print('manifest unknown', file=sys.stderr); sys.exit(1)
    elif a[:3] == ['buildx','imagetools','create']:
        tag=a[a.index('--tag')+1]
        if mode == 'partial-publish' and tag.endswith('linux-cuda12'): sys.exit(1)
        p=state/'published.json'
        data=json.loads(p.read_text()) if p.exists() else {}
        data[tag]=a[-1].split('@')[1]
        p.write_text(json.dumps(data))
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

    def prepare(self, mode=''):
        project = self.build_workspace()
        r = self.run_bash('bash deploy/build-docker/build.sh --prepare', mode, project)
        receipt = project / ('deploy/build-docker/out/release-1.30.0-' + '1' * 40) / 'receipt.json'
        return project, r, receipt

    def test_all_tests_finish_before_candidate_upload(self):
        _, r, receipt = self.prepare()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(len(json.loads(receipt.read_text())['images']), 3)
        calls = self.calls()
        tests = [i for i,c in enumerate(calls) if c[:2] == ['docker','run']]
        uploads = [i for i,c in enumerate(calls) if '--push' in c]
        self.assertEqual(len(tests), 3)
        self.assertLess(max(tests), min(uploads))
        self.assertTrue(all(any(':candidate-' in arg for arg in calls[i]) for i in uploads))
        self.assertNotIn('--gpus', calls[tests[0]])
        self.assertIn('--gpus', calls[tests[1]])
        self.assertEqual(len([c for c in calls if c[:3] == ['docker','image','save']]), 3)

    def test_invalid_registry_digest_prevents_receipt(self):
        _, r, receipt = self.prepare('registry-invalid-digest')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(receipt.exists())

    def test_test_failure_prevents_all_uploads(self):
        _, r, receipt = self.prepare('test-fail')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(receipt.exists())
        self.assertFalse(any('--push' in c for c in self.calls()))

    def test_untested_candidate_cannot_get_receipt(self):
        _, r, receipt = self.prepare('config-mismatch')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(receipt.exists())

    def test_missing_inference_output_prevents_uploads(self):
        _, r, receipt = self.prepare('missing-output')
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(receipt.exists())
        self.assertFalse(any('--push' in c for c in self.calls()))

    def test_publish_tag_conflict_and_auth_failure_do_not_write(self):
        project, r, receipt = self.prepare()
        self.assertEqual(r.returncode, 0, r.stderr)
        for mode in ('tag-conflict','registry-fail','registry-not-found'):
            r = self.run_bash(f'bash deploy/build-docker/build.sh --publish="{receipt}"', mode, project)
            self.assertNotEqual(r.returncode, 0)
        self.assertFalse(any(c[:4] == ['docker','buildx','imagetools','create'] for c in self.calls()))

    def test_partial_publish_can_resume_same_digests(self):
        project, r, receipt = self.prepare()
        self.assertEqual(r.returncode, 0, r.stderr)
        r = self.run_bash(f'bash deploy/build-docker/build.sh --publish="{receipt}"', 'partial-publish', project)
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(len(json.loads((self.root/'published.json').read_text())), 1)
        r = self.run_bash(f'bash deploy/build-docker/build.sh --publish="{receipt}"', cwd=project)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(len(json.loads((self.root/'published.json').read_text())), 3)

    def test_publish_mismatched_tree_cannot_write(self):
        project, r, receipt = self.prepare()
        data=json.loads(receipt.read_text())
        data['tree']='3'*40
        receipt.write_text(json.dumps(data))
        r = self.run_bash(f'bash deploy/build-docker/build.sh --publish="{receipt}"', cwd=project)
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(any(c[:4] == ['docker','buildx','imagetools','create'] for c in self.calls()))

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
