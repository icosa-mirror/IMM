"""Retry one current-branch Unity Vulkan startup kill; never retry source failures."""
import json
import os
from pathlib import Path
import re
import subprocess

TARGET = 'Engine / Unity Windows Vulkan Player Build'
DOWNSTREAM = {'Engine / Engine Evidence Report', 'Validation Evidence'}


def startup_kill(log):
    # Require the observed immediate kill during initial domain reload. A 137
    # elsewhere (including imports, compilation or the player) is not sufficient.
    if re.search(r'error CS\d+|Shader error|IMM_URP_SMOKE.*FAIL|BuildFailedException', log):
        return False
    lines = log.splitlines()
    for index, line in enumerate(lines):
        if re.search(r'\bKilled\s*$', line):
            before = '\n'.join(lines[max(0, index - 15):index])
            after = '\n'.join(lines[index:index + 8])
            if ('Begin MonoManager ReloadAssembly' in before and 'exit code 137' in after and
                    'CodeReloadManager initialized' in before and
                    before.count('Begin MonoManager ReloadAssembly') == 1):
                return True
    return False


def api(path, method=None):
    command = ['gh', 'api', path]
    if method:
        command += ['--method', method]
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


def eligible(run, repository, current_sha):
    return (run['status'] == 'completed' and run['conclusion'] == 'failure' and
            run['run_attempt'] == 1 and run['head_branch'] == 'main' and
            run['head_repository']['full_name'] == repository and run['head_sha'] == current_sha and
            run['path'] == '.github/workflows/ci-validation.yml')


def candidate(jobs):
    failed = [job for job in jobs if job['conclusion'] == 'failure']
    targets = [job for job in failed if job['name'] == TARGET]
    if len(targets) != 1 or any(job['name'] not in DOWNSTREAM | {TARGET} for job in failed):
        return None
    return targets[0]


def main():
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    repository = os.environ['GITHUB_REPOSITORY']
    run_id = event['workflow_run']['id']
    prefix = f'repos/{repository}'
    run = json.loads(api(f'{prefix}/actions/runs/{run_id}'))
    current_sha = json.loads(api(f'{prefix}/commits/main'))['sha']
    result = 'No automatic retry: run is obsolete, already retried, or outside the supported scope.'
    if eligible(run, repository, current_sha):
        jobs = []
        page = 1
        while True:
            batch = json.loads(api(f'{prefix}/actions/runs/{run_id}/attempts/1/jobs?per_page=100&page={page}'))['jobs']
            jobs.extend(batch)
            if len(batch) < 100:
                break
            page += 1
        job = candidate(jobs)
        if job and startup_kill(api(f'{prefix}/actions/jobs/{job["id"]}/logs')):
            # Recheck before mutation, in case another actor already restarted it.
            run = json.loads(api(f'{prefix}/actions/runs/{run_id}'))
            current_sha = json.loads(api(f'{prefix}/commits/main'))['sha']
            if eligible(run, repository, current_sha):
                api(f'{prefix}/actions/jobs/{job["id"]}/rerun', method='POST')
                result = (f'Requested the one permitted infrastructure retry for job {job["id"]} '
                          f'in run {run_id}. Successful independent jobs are retained. '
                          'The startup kill remains recorded in attempt 1; this is not proof of a memory fix.')
        else:
            result = 'No automatic retry: failure is not an isolated, confirmed Unity Vulkan startup kill.'
    print(result)
    with Path(os.environ['GITHUB_STEP_SUMMARY']).open('a') as summary:
        summary.write(f'{result}\n')


if __name__ == '__main__':
    main()
