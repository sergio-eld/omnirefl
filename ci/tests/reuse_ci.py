"""Check the reuse CLI through GitHub's external run/artifact protocol.

PR run metadata names the head commit, while checkout tests a merge commit.
The fixtures model the REST Actions API and use real ZIPs and SHA-256 digests:
https://docs.github.com/en/rest/actions/artifacts
https://docs.github.com/en/rest/actions/workflow-runs
"""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest
import zipfile


# Stand in for the unavailable authenticated GitHub service, not project code.
GITHUB_CLI = '''\
#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
from urllib.parse import parse_qs, urlsplit

root = Path(os.environ['GITHUB_FIXTURE'])
data = json.loads((root / 'api.json').read_text())
url = urlsplit(sys.argv[2])
if '/git/commits/' in url.path:
    print(json.dumps({'sha': url.path.rsplit('/', 1)[1],
                      'parents': [{'sha': p} for p in data['parents']]}))
    sys.exit()
path = url.path.split('/actions/')[1].split('/')
query = parse_qs(url.query)
if path == ['runs']:
    result = {'workflow_runs': [r for r in data['runs']
              if r['head_sha'] == query.get('head_sha', [r['head_sha']])[0]]}
elif path == ['artifacts']:
    result = {'artifacts': [a for a in data['artifacts']
              if a['name'] == query.get('name', [a['name']])[0]]}
elif len(path) == 2 and path[0] == 'runs':
    result = data['runs'][0]
elif len(path) == 3 and path[0] == 'runs' and path[2] == 'artifacts':
    result = {'artifacts': data['artifacts']}
elif len(path) == 3 and path[0] == 'runs' and path[2] == 'jobs':
    result = {'jobs': [{'name': 'Build and validate release / Full test suite',
                       'conclusion': 'success'}]}
elif len(path) == 3 and path[0] == 'artifacts' and path[2] == 'zip':
    sys.stdout.buffer.write((root / (path[1] + '.zip')).read_bytes())
    sys.exit()
else:
    raise RuntimeError(sys.argv[2])
print(json.dumps(result))
'''


class PackageReuse(unittest.TestCase):
    def test_exact_tested_commit(self):
        commit = 'a' * 40
        head = 'b' * 40
        base = 'd' * 40
        for name, head, requested, full, conclusion, parents, expected in [
            ('PR merge commit', head, commit, False, 'success', [base, head],
             {'run_id': '7', 'full_suite': 'false'}),
            ('publishing failed after full validation', commit, commit, True,
             'failure', [base, head], {'run_id': '7', 'full_suite': 'true'}),
            ('amended commit', head, 'c' * 40, False, 'success', [base, head],
             {'run_id': '', 'full_suite': 'false'}),
            ('ordinary push', commit, commit, False, 'success', [base, head],
             {'run_id': '7', 'full_suite': 'false'}),
            ('unrelated PR smoke claim', head, commit, False, 'success',
             [base, 'e' * 40], {'run_id': '', 'full_suite': 'false'}),
            ('unrelated PR full claim', head, commit, True, 'success',
             [base, 'e' * 40], {'run_id': '', 'full_suite': 'false'}),
        ]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'gh').write_text(textwrap.dedent(GITHUB_CLI))
                (root / 'gh').chmod(0o755)
                artifacts = []

                def artifact(name, file, content):
                    number = len(artifacts) + 1
                    path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(path, 'w') as archive:
                        archive.writestr(file, content)
                    artifacts.append({
                        'id': number, 'name': name, 'expired': False,
                        'digest': 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest(),
                        'workflow_run': {'id': 7, 'head_sha': head},
                    })

                artifact('packages-extras-' + commit[:12],
                         'package.tar.gz', 'package contents')
                artifact('validated-commit-' + commit, 'validated-commit.json',
                         json.dumps({'commit': commit, 'run_id': '7'}))
                if full:
                    artifact('full-suite-' + commit + '-attempt-1', 'full-suite.json',
                             json.dumps({'suite': 'full', 'commit': commit,
                                         'run_id': '7', 'run_attempt': '1'}))
                run = {
                    'id': 7, 'head_sha': head, 'status': 'completed',
                    'conclusion': conclusion, 'run_attempt': 1,
                    'event': 'pull_request' if head != commit else 'push',
                    'head_repository': {'full_name': 'example/project'},
                    'pull_requests': [{'base': {'sha': base}, 'head': {'sha': head}}],
                    'path': '.github/workflows/release.yml' if full
                            else '.github/workflows/ci.yml',
                }
                (root / 'api.json').write_text(json.dumps({
                    'runs': [run], 'artifacts': artifacts, 'parents': parents,
                }))
                environment = dict(
                    os.environ, GITHUB_REPOSITORY='example/project', GITHUB_SHA=requested,
                    GITHUB_RUN_ID='8', GITHUB_OUTPUT=str(root / 'output'),
                    GITHUB_FIXTURE=directory,
                    PATH=directory + os.pathsep + os.environ['PATH'],
                )
                command = [sys.executable, str(Path(__file__).resolve().parents[1]
                                               / 'reuse_ci.py')]
                result = subprocess.run(command + ['find', '--targets', 'extras'],
                                        env=environment, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                output = dict(line.split('=', 1)
                              for line in (root / 'output').read_text().splitlines())
                self.assertEqual(expected, output)
                if output['run_id']:
                    result = subprocess.run(command + [
                        'download', '--run-id', '7', '--target', 'extras',
                        '--packages-dir', str(root / 'package'),
                    ], env=environment, capture_output=True, text=True)
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertEqual('package contents',
                                     (root / 'package/package.tar.gz').read_text())


if __name__ == '__main__':
    unittest.main()
