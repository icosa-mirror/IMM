"""Discard compiler PID records restored from another CI runner before starting Unity."""
import argparse
import os
from pathlib import Path


def sanitize(project, workspace):
    project = project.resolve()
    workspace = workspace.resolve()
    library = project / 'Library'
    if not project.is_relative_to(workspace) or not library.resolve().is_relative_to(project):
        raise ValueError('Unity Library must be inside its project and CI workspace')
    if not (project / 'ProjectSettings').is_dir():
        raise ValueError('Expected a Unity project with ProjectSettings')
    removed = []
    for name in ('burst.pid', 'ilpp.pid'):
        path = library / name
        if path.is_symlink():
            raise ValueError(f'Refusing a symlinked compiler PID record: {name}')
        if path.exists():
            path.unlink()
            removed.append(name)
    return removed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True, type=Path)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise RuntimeError('Only run on a CI-owned project before launching its Editor')
    removed = sanitize(args.project, Path(os.environ['GITHUB_WORKSPACE']))
    print(f'[IMM_UNITY_CACHE] Removed restored compiler PID records: {", ".join(removed) or "none"}')


if __name__ == '__main__':
    main()
