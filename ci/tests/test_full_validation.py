"""Exercise full-suite cadence through real Git history and the GitHub API CLI.

The external fixtures follow Actions run/job responses:
https://docs.github.com/en/rest/actions/workflow-runs
https://docs.github.com/en/rest/actions/workflow-jobs
"""

from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Optional
import unittest


GITHUB_CLI = '''\
#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
from urllib.parse import parse_qs, urlsplit
data = json.loads(Path(os.environ['GITHUB_FIXTURE']).read_text())
url = urlsplit(sys.argv[2])
if '/commits/' in url.path and url.path.endswith('/pulls'):
    print('[]')
    sys.exit()
path = url.path.split('/actions/')[1].split('/')
if path == ['runs']:
    revision = parse_qs(url.query)['head_sha'][0]
    result = {'workflow_runs': [r for r in data['runs'] if r['head_sha'] == revision]}
elif len(path) == 3 and path[0] == 'runs' and path[2] == 'jobs':
    result = {'jobs': data['jobs'].get(path[1], [])}
else:
    raise RuntimeError(url.path)
print(json.dumps(result))
'''


@dataclass(frozen=True)
class Case:
    name: str
    # First-parent commits after the last successful full run; None seeds it.
    distance: Optional[int]
    idle: timedelta
    expected: bool
    active: bool = False
    failed: bool = False
    pr_claim: bool = False
    automatic: bool = False
    checking: bool = False


class FullValidation(unittest.TestCase):
    def test_master_cadence(self):
        cases = [
            Case('validated idle head', 0, timedelta(days=4), False),
            Case('four new commits', 4, timedelta(), False),
            Case('five new commits', 5, timedelta(), True),
            Case('push ignores idle time', 1, timedelta(days=4), False),
            Case('active full run', 5, timedelta(), False, active=True),
            Case('active automatic full run', 5, timedelta(), False,
                 active=True, automatic=True),
            Case('active idle check is not full validation', 5, timedelta(), True,
                 active=True, checking=True),
            Case('failed full run does not reset', 5, timedelta(), True, failed=True),
            Case('PR head is not its tested merge', 5, timedelta(), True, pr_claim=True),
            Case('initial baseline', None, timedelta(), True),
        ]
        for case in cases:
            with self.subTest(case=case.name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                subprocess.run(['git', 'init', '-q', directory], check=True)
                environment = dict(os.environ, GIT_AUTHOR_NAME='CI fixture',
                                   GIT_AUTHOR_EMAIL='ci@example.test',
                                   GIT_COMMITTER_NAME='CI fixture',
                                   GIT_COMMITTER_EMAIL='ci@example.test')
                latest = datetime.now(timezone.utc) - case.idle
                for index in range(6):
                    date = latest - timedelta(hours=5 - index)
                    environment.update(GIT_AUTHOR_DATE=date.isoformat(),
                                       GIT_COMMITTER_DATE=date.isoformat())
                    subprocess.run(['git', '-C', directory,
                                    '-c', 'commit.gpgsign=false',
                                    '-c', 'core.hooksPath=/dev/null',
                                    'commit', '--allow-empty', '-qm', str(index)],
                                   env=environment, check=True)
                history = subprocess.check_output(
                    ['git', '-C', directory, 'rev-list', '--first-parent', 'HEAD'],
                    text=True).splitlines()
                runs = [{
                    'id': 100, 'head_sha': history[0], 'status': 'in_progress',
                    'event': 'push', 'path': '.github/workflows/full_validation.yml',
                    'display_title': 'Periodic full validation',
                }]
                jobs = {}
                if case.distance is not None:
                    runs.append({
                        'id': 7, 'head_sha': history[case.distance],
                        'status': 'completed', 'event': 'push',
                        'path': '.github/workflows/ci.yml',
                        'display_title': 'CI / master / full',
                    })
                    jobs['7'] = [{'name': 'Full test suite', 'conclusion': 'success'}]
                if case.active or case.failed or case.pr_claim:
                    runs.append({
                        'id': 8, 'head_sha': history[0],
                        'status': 'in_progress' if case.active else 'completed',
                        'event': 'pull_request' if case.pr_claim else 'push',
                        'path': '.github/workflows/full_validation.yml' if case.checking
                                else '.github/workflows/ci.yml',
                        'display_title': 'Periodic full validation' if case.checking
                                         else 'CI / master / automatic' if case.automatic
                                         else 'CI / master / full',
                    })
                    jobs['8'] = [{'name': 'Validate and prepare CI matrix / full'
                                  if case.automatic else 'Select periodic full validation'
                                  if case.checking else 'Full test suite',
                                  'conclusion': 'failure' if case.failed else 'success'}]
                (root / 'api.json').write_text(json.dumps({'runs': runs, 'jobs': jobs}))
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                environment.update(
                    GITHUB_REPOSITORY='example/project', GITHUB_SHA=history[0],
                    GITHUB_RUN_ID='100', GITHUB_OUTPUT=str(root / 'output'),
                    GITHUB_FIXTURE=str(root / 'api.json'),
                    PATH=directory + os.pathsep + os.environ['PATH'])
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).resolve().parents[1]
                                         / 'full_validation.py')],
                    cwd=directory, env=environment, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual('full_suite=' + str(case.expected).lower() + '\n',
                                 (root / 'output').read_text())


if __name__ == '__main__':
    unittest.main()
