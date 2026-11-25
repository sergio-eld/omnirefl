"""Exercise PR suite selection against current GitHub labels on reruns.

External response format: https://docs.github.com/en/rest/pulls/pulls#get-a-pull-request
"""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


GITHUB_CLI = '''\
#!/usr/bin/env python3
import os
from pathlib import Path
import sys
if sys.argv[1:] != ['api', 'repos/example/project/pulls/134']:
    raise RuntimeError('Unexpected GitHub endpoint')
print(Path(os.environ['GITHUB_FIXTURE']).read_text())
'''


class PrFullValidation(unittest.TestCase):
    def test_current_labels(self):
        cases = [
            ('unlabeled PR', [], [], False),
            ('full label added after original run', [], ['ci:full'], True),
            ('full label removed after original run', ['ci:full'], [], False),
            ('unrelated label', [], ['documentation'], False),
        ]
        for name, original, current, expected in cases:
            with self.subTest(case=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                event = {'pull_request': {'number': 134, 'labels': [
                    {'name': label} for label in original]}}
                (root / 'event.json').write_text(json.dumps(event))
                (root / 'pull.json').write_text(json.dumps({
                    'labels': [{'name': label} for label in current]}))
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                output = root / 'output'
                environment = dict(
                    os.environ, GITHUB_REPOSITORY='example/project',
                    GITHUB_EVENT_NAME='pull_request',
                    GITHUB_EVENT_PATH=str(root / 'event.json'),
                    GITHUB_OUTPUT=str(output),
                    GITHUB_FIXTURE=str(root / 'pull.json'),
                    PATH=directory + os.pathsep + os.environ['PATH'])
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).resolve().parents[1]
                                         / 'full_validation.py'),
                     '--pull-request', '134'],
                    env=environment, capture_output=True, text=True, timeout=30)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual('full_suite=' + str(expected).lower() + '\n',
                                 output.read_text())


if __name__ == '__main__':
    unittest.main()
