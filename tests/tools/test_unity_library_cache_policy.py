"""Keep all production Unity Library saves and restores on one explicit boundary."""
from pathlib import Path

import yaml


def main():
    root = Path(__file__).resolve().parents[2]
    entries = ('PackageCache/', 'DataStore/', 'ArtifactDB', 'SourceAssetDB',
               'ShaderCache/', 'ShaderCache.db')
    configurations = 0
    for name in ('ci-engine.yml', 'ci-engine-android.yml', 'ci-ios.yml'):
        workflow = yaml.safe_load((root / '.github/workflows' / name).read_text())
        for job_name, job in workflow['jobs'].items():
            operations = {}
            for step in job.get('steps', []):
                settings = step.get('with', {})
                key = settings.get('key', '')
                if not key.startswith('UnityLibrary-'):
                    continue
                assert key.startswith('UnityLibrary-allowlist-v1-'), (name, job_name, key)
                paths = settings['path'].splitlines()
                library = paths[0].split('/Library/')[0] + '/Library/'
                assert paths == [f'{library}{entry}' for entry in entries], (name, job_name, paths)
                assert all(line.startswith('UnityLibrary-allowlist-v1-')
                           for line in settings.get('restore-keys', '').splitlines())
                action = step['uses'].split('@')[0]
                assert action in ('actions/cache/restore', 'actions/cache/save'), action
                operations.setdefault(key, {})[action] = paths
            for key, actions in operations.items():
                assert set(actions) == {'actions/cache/restore', 'actions/cache/save'}, (job_name, key)
                assert actions['actions/cache/restore'] == actions['actions/cache/save']
                configurations += 1
    assert configurations == 8, configurations
    print(f'Unity Library allowlist verified for {configurations} production cache configurations')


if __name__ == '__main__':
    main()
