"""Verify cache sanitization preserves compiled assets and refuses local Editor state."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from sanitize_unity_library_cache import sanitize


def main():
    with tempfile.TemporaryDirectory() as temp:
        workspace = Path(temp)
        project = workspace / 'project'
        (project / 'ProjectSettings').mkdir(parents=True)
        library = project / 'Library'
        library.mkdir()
        (library / 'burst.pid').write_text('2234')
        (library / 'ilpp.pid').write_text('2347')
        compiled = library / 'compiled-cache.bin'
        compiled.write_bytes(b'keep compiled assets')
        env = dict(os.environ)
        env.pop('GITHUB_ACTIONS', None)
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('sanitize_unity_library_cache.py')),
                                 '--project', str(project)], env=env, capture_output=True, text=True)
        assert result.returncode != 0
        assert (library / 'burst.pid').read_text() == '2234'
        assert sanitize(project, workspace) == ['burst.pid', 'ilpp.pid']
        assert compiled.read_bytes() == b'keep compiled assets'
        assert sanitize(project, workspace) == []
        try:
            sanitize(project, project / 'nested-workspace')
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted a project outside the CI workspace')
    print('Unity compiler PID cache sanitization checks passed')


if __name__ == '__main__':
    main()
