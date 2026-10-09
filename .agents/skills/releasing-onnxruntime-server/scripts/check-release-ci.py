#!/usr/bin/env python3
"""Read-only CI gate for a specific PR head. Nonzero while pending or failed."""
import json
import subprocess
import sys


def api(endpoint):
    return json.loads(subprocess.check_output(["gh", "api", endpoint], text=True))


def pages(endpoint, key):
    page = 1
    result = []
    while True:
        batch = api(f"{endpoint}&per_page=100&page={page}")[key]
        result.extend(batch)
        if len(batch) < 100:
            return result
        page += 1


def check(repo, number, expected):
    pr = api(f"repos/{repo}/pulls/{number}")
    if pr["state"] != "open" or pr["head"]["sha"] != expected:
        raise ValueError("PR is not open at the expected head")
    if pr["base"]["ref"] != "main" or pr.get("draft"):
        raise ValueError("release PR must target main and be ready for review")
    runs = pages(f"repos/{repo}/actions/runs?head_sha={expected}", "workflow_runs")
    for workflow in ("CMake on Linux", "CMake on Windows", "CMake on MacOS", "CodeQL"):
        matching = [r for r in runs if r["name"] == workflow and r["head_sha"] == expected
                    and r["event"] == "pull_request" and any(p["number"] == number for p in r["pull_requests"])]
        if not matching:
            raise ValueError(f"missing PR workflow for {expected}: {workflow}")
        latest = max(matching, key=lambda r: (r["id"], r.get("run_attempt", 1)))
        if latest["status"] != "completed" or latest["conclusion"] != "success":
            raise ValueError(f"{workflow}: {latest['status']}/{latest['conclusion']}")
    # PR checks can belong to the synthetic merge SHA; inspect both surfaces.
    for sha in dict.fromkeys((expected, pr["merge_commit_sha"])):
        if not sha:
            raise ValueError("PR merge commit is not ready")
        checks = pages(f"repos/{repo}/commits/{sha}/check-runs?filter=latest", "check_runs")
        for c in checks:
            if c["status"] != "completed" or c["conclusion"] != "success":
                raise ValueError(f"check {c['name']}: {c['status']}/{c['conclusion']}")
        statuses = api(f"repos/{repo}/commits/{sha}/status")
        if statuses["statuses"] and statuses["state"] != "success":
            raise ValueError(f"commit statuses on {sha}: {statuses['state']}")
    current = api(f"repos/{repo}/pulls/{number}")
    main = api(f"repos/{repo}/git/ref/heads/main")["object"]["sha"]
    if current["head"]["sha"] != expected or current["base"]["sha"] != pr["base"]["sha"] or main != pr["base"]["sha"] or current["merge_commit_sha"] != pr["merge_commit_sha"]:
        raise ValueError("PR head, merge or main moved during CI inspection")
    if current.get("mergeable") is not True or current.get("mergeable_state") != "clean":
        raise ValueError(f"merge gate: {current.get('mergeable_state')}")
    print(json.dumps({"head": expected, "base": main, "merge": current["merge_commit_sha"]}))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: check-release-ci.py <owner/repo> <pr-number> <expected-head-sha>")
    try:
        check(sys.argv[1], int(sys.argv[2]), sys.argv[3])
    except (ValueError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
