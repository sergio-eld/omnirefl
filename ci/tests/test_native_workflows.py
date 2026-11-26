"""Run release download shell against real ZIPs via the Actions REST fixture."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest
import zipfile

from test_package_reuse import GITHUB_CLI


ROOT = Path(__file__).resolve().parents[2]


class NativeWorkflows(unittest.TestCase):
    def test_release_publishes_the_fully_validated_set(self):
        workflow = (ROOT / '.github/workflows/release.yml').read_text()
        step = re.search(r'      - name: Download Release Artifacts\n(.*?)'
                         r'(?=      - name:)', workflow, re.DOTALL).group(1)
        script = textwrap.dedent(step.split('        run: |\n', 1)[1])
        for validation, expected in [('success', 'current full bytes'),
                                     ('skipped', 'pinned full bytes')]:
            with self.subTest(validation=validation), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'ci').symlink_to(ROOT / 'ci', target_is_directory=True)
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                commit = 'a' * 40
                artifacts = []
                for name, filename, contents in [
                    ('release-inputs-' + commit, 'packages/extras/bytes', 'pinned full bytes'),
                    ('packages-extras-' + commit[:12], 'bytes', 'current full bytes'),
                ]:
                    number = len(artifacts) + 1
                    archive_path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(archive_path, 'w') as archive:
                        archive.writestr(filename, contents)
                    artifacts.append({'id': number, 'name': name, 'expired': False,
                        'digest': 'sha256:' + hashlib.sha256(archive_path.read_bytes()).hexdigest()})
                (root / 'api.json').write_text(json.dumps({'runs': [], 'artifacts': artifacts}))
                environment = dict(os.environ, GITHUB_SHA=commit, GITHUB_RUN_ID='100',
                    GITHUB_REPOSITORY='example/project', GITHUB_FIXTURE=directory,
                    REUSE_ARTIFACTS='true', VALIDATION_RESULT=validation,
                    RELEASE_PACKAGE_TARGETS='extras', RELEASE_SYMBOL_TARGETS='',
                    PATH=directory + os.pathsep + os.environ['PATH'])
                result = subprocess.run(['bash', '-e', '-c', script], cwd=directory,
                    env=environment, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                files = list((root / 'artifacts').rglob('bytes'))
                self.assertEqual(1, len(files))
                self.assertEqual(expected, files[0].read_text())
