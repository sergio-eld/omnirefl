"""Exercise artifact downloads against transient GitHub service failures.

External response format: https://docs.github.com/en/rest/actions/artifacts
"""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


GITHUB_CLI = '''\
#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
from urllib.parse import urlsplit
root = Path(os.environ['GITHUB_FIXTURE'])
data = json.loads((root / 'api.json').read_text())
if urlsplit(sys.argv[2]).path.endswith('/zip'):
    attempts = root / 'attempts'
    count = int(attempts.read_text()) if attempts.exists() else 0
    attempts.write_text(str(count + 1))
    if count < data['failures']:
        sys.stdout.buffer.write(b'partial download')
        print('gh: HTTP 503', file=sys.stderr)
        sys.exit(1)
    sys.stdout.buffer.write((root / 'artifact.zip').read_bytes())
else:
    print(json.dumps({'artifacts': [data['artifact']]}))
'''


class ArtifactDownload(unittest.TestCase):
    def test_service_failures(self):
        for name, failures, expected in [
            ('recovers after transient failures', 2, 0),
            ('reports persistent failure', 3, 1),
        ]:
            with self.subTest(case=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / 'artifact.zip'
                with zipfile.ZipFile(archive, 'w') as content:
                    content.writestr('package.txt', 'verified package')
                (root / 'api.json').write_text(json.dumps({
                    'failures': failures,
                    'artifact': {
                        'id': 7, 'name': 'test-package', 'expired': False,
                        'digest': 'sha256:' + hashlib.sha256(archive.read_bytes()).hexdigest(),
                    },
                }))
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                environment = dict(
                    os.environ, GITHUB_REPOSITORY='example/project',
                    GITHUB_FIXTURE=directory,
                    PATH=directory + os.pathsep + os.environ['PATH'])
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).resolve().parents[1]
                                         / 'github_artifacts.py'),
                     '--run-id', '1', '--name', 'test-package',
                     '--destination', str(root / 'download')],
                    env=environment, capture_output=True, text=True, timeout=30)
                self.assertEqual(expected, result.returncode, result.stderr)
                if expected == 0:
                    self.assertEqual('verified package',
                                     (root / 'download/package.txt').read_text())
                else:
                    self.assertIn('HTTP 503', result.stderr)
                    self.assertFalse((root / 'download').exists())


if __name__ == '__main__':
    unittest.main()
