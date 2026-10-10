"""Exercise retry boundaries and memory parsing without Docker, Unity, or GitHub writes."""
from pathlib import Path
import tempfile
import json
import os
from unittest.mock import patch
import retry_unity_startup_kill as retry
from monitor_unity_build_memory import cgroup_directory, counters, read_memory
from retry_unity_startup_kill import candidate, eligible, startup_kill, TARGET


def main():
    observed = '\n'.join([
        '2026-10-10T10:24:00Z CodeReloadManager initialized',
        '2026-10-10T10:24:00Z Begin MonoManager ReloadAssembly',
        "2026-10-10T10:24:00Z Registering precompiled unity dll's ...",
        '2026-10-10T10:24:01Z Killed',
        '2026-10-10T10:24:01Z Build failed, with exit code 137',
    ])
    assert startup_kill(observed)
    assert not startup_kill(observed.replace('137', '1'))
    assert not startup_kill(observed.replace('Begin MonoManager ReloadAssembly', 'Compiling shader'))
    assert not startup_kill(observed.replace("Registering precompiled unity dll's ...", 'Begin MonoManager ReloadAssembly\nRegistering precompiled dlls'))
    assert not startup_kill(f'{observed}\nerror CS7036: missing argument')
    assert not startup_kill(f'{observed}\n[IMM_URP_SMOKE] FAIL rendering')
    target = dict(id=123, name=TARGET, conclusion='failure')
    assert candidate([target]) == target
    assert candidate([target, dict(name='Validation Evidence', conclusion='failure')]) == target
    assert candidate([target, dict(name='Engine / Unity macOS Metal Composition', conclusion='failure')]) is None
    assert candidate([dict(target, conclusion='success')]) is None
    run = dict(status='completed', conclusion='failure', run_attempt=1, head_branch='main',
               head_repository=dict(full_name='owner/repo'), head_sha='abc', path='.github/workflows/ci-validation.yml')
    assert eligible(run, 'owner/repo', 'abc')
    assert not eligible(dict(run, run_attempt=2), 'owner/repo', 'abc')
    assert not eligible(run, 'owner/repo', 'newer')
    assert not eligible(dict(run, head_repository=dict(full_name='fork/repo')), 'owner/repo', 'abc')
    assert not eligible(dict(run, status='in_progress'), 'owner/repo', 'abc')
    assert not eligible(dict(run, head_branch='feature/test'), 'owner/repo', 'abc')
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        group = root / 'docker' / 'test'
        group.mkdir(parents=True)
        (group / 'memory.current').write_text('123')
        (group / 'memory.peak').write_text('456')
        (group / 'memory.max').write_text('max')
        (group / 'memory.events').write_text('oom 2\noom_kill 1\n')
        assert cgroup_directory('0::/docker/test', root) == group.resolve()
        assert cgroup_directory('0::/../../outside', root) is None
        assert read_memory(group)['memory.peak'] == '456'
        assert read_memory(group)['memory.events']['oom_kill'] == 1
        assert counters('MemAvailable: 123\nSwapFree: 456')['MemAvailable:'] == 123
    with tempfile.TemporaryDirectory() as temp:
        event = Path(temp) / 'event.json'
        summary = Path(temp) / 'summary.md'
        event.write_text(json.dumps(dict(workflow_run=dict(id=99))))
        calls = []
        def fake_api(path, method=None):
            calls.append((path, method))
            if path.endswith('/actions/runs/99'):
                return json.dumps(run)
            if path.endswith('/commits/main'):
                return json.dumps(dict(sha='abc'))
            if '/attempts/1/jobs?' in path:
                return json.dumps(dict(jobs=[target]))
            if path.endswith('/logs'):
                return observed
            if path.endswith('/actions/jobs/123/rerun') and method == 'POST':
                return '{}'
            raise AssertionError(f'Unexpected GitHub operation: {path} {method}')
        with patch.dict(os.environ, GITHUB_EVENT_PATH=str(event), GITHUB_REPOSITORY='owner/repo', GITHUB_STEP_SUMMARY=str(summary)), patch.object(retry, 'api', fake_api):
            retry.main()
            assert [(path, method) for path, method in calls if method] == [('repos/owner/repo/actions/jobs/123/rerun', 'POST')]
            calls.clear()
            with patch.object(retry, 'startup_kill', return_value=False):
                retry.main()
            assert not any(method for _, method in calls)
            calls.clear()
            run['run_attempt'] = 2
            retry.main()
            assert not any(method for _, method in calls)
    print('Unity infrastructure diagnostics and bounded retry checks passed')


if __name__ == '__main__':
    main()
