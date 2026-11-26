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
    sha = url.path.rsplit('/', 1)[1]
    print(json.dumps(data.get('commits', {}).get(sha, {
        'sha': sha, 'parents': [{'sha': p} for p in data.get('parents', [])]})))
    sys.exit()
if '/commits/' in url.path and url.path.endswith('/pulls'):
    print(json.dumps(data.get('merged_pulls', {}).get(url.path.split('/')[-2], [])))
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
    result = next(r for r in data['runs'] if str(r['id']) == path[1])
elif len(path) == 4 and path[0] == 'runs' and path[2] == 'attempts':
    result = data['attempts'][path[3]]
elif len(path) == 3 and path[0] == 'runs' and path[2] == 'artifacts':
    result = {'artifacts': data['artifacts']}
elif path[0] == 'runs' and path[-1] == 'jobs':
    key = path[1] + ':' + path[3] if len(path) == 5 else path[1]
    result = {'jobs': data.get('jobs', {}).get(key, data.get('jobs', {}).get(path[1], [
        {'name': 'Build and validate release / Full test suite',
         'conclusion': 'success', 'completed_at': '2026-11-25T12:00:25Z'}]))}
elif len(path) == 3 and path[0] == 'artifacts' and path[2] == 'zip':
    sys.stdout.buffer.write((root / (path[1] + '.zip')).read_bytes())
    sys.exit()
else:
    raise RuntimeError(sys.argv[2])
print(json.dumps(result))
'''


class PackageReuse(unittest.TestCase):
    def test_smoke_packages_survive_failed_rerun(self):
        commit = 'a' * 40
        for case, earlier_result, replaced, reusable in [
            ('untouched green smoke artifacts', 'success', False, True),
            ('replacement after tested gate', 'success', True, False),
            ('no green attempt', 'failure', False, False),
        ]:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                artifacts = []
                for name, filename, content, timestamp in [
                    ('packages-cosmo-universal-' + commit[:12], 'binary.tar.gz',
                     'package bytes', '30' if replaced else '10'),
                    ('symbols-cosmo-universal-' + commit[:12], 'omnirefl.debug',
                     'matching symbols', '10'),
                    ('validated-commit-' + commit, 'validated-commit.json',
                     json.dumps({'commit': commit, 'run_id': '7'}), '20'),
                ]:
                    number = len(artifacts) + 1
                    archive_path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(archive_path, 'w') as archive:
                        archive.writestr(filename, content)
                    artifacts.append({
                        'id': number, 'name': name, 'expired': False,
                        'created_at': '2026-11-25T12:00:' + timestamp + 'Z',
                        'digest': 'sha256:' + hashlib.sha256(
                            archive_path.read_bytes()).hexdigest(),
                        'workflow_run': {'id': 7, 'head_sha': commit},
                    })
                run = {'id': 7, 'head_sha': commit, 'status': 'completed',
                       'conclusion': 'failure', 'run_attempt': 2,
                       'event': 'push', 'path': '.github/workflows/ci.yml',
                       'updated_at': '2026-11-25T12:00:50Z',
                       'run_started_at': '2026-11-25T12:00:26Z'}
                (root / 'api.json').write_text(json.dumps({
                    'runs': [run], 'artifacts': artifacts,
                    'attempts': {'1': {**run, 'run_attempt': 1,
                                       'conclusion': earlier_result,
                                       'updated_at': '2026-11-25T12:00:25Z',
                                       'run_started_at': '2026-11-25T12:00:00Z'}},
                }))
                environment = dict(
                    os.environ, GITHUB_REPOSITORY='example/project', GITHUB_SHA=commit,
                    GITHUB_OUTPUT=str(root / 'output'), GITHUB_FIXTURE=directory,
                    PATH=directory + os.pathsep + os.environ['PATH'])
                command = [sys.executable, str(Path(__file__).resolve().parents[1]
                                               / 'reuse_ci.py')]
                result = subprocess.run(command + [
                    'find', '--targets', 'cosmo-universal', '--require-symbols'],
                    env=environment, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                output = dict(line.split('=', 1) for line
                              in (root / 'output').read_text().splitlines())
                self.assertEqual('7' if reusable else '', output['run_id'])
                self.assertEqual('false', output['full_suite'])
                result = subprocess.run(command + [
                    'download', '--run-id', '7', '--target', 'cosmo-universal',
                    '--packages-dir', str(root / 'packages'),
                    '--symbols-dir', str(root / 'symbols')],
                    env=environment, capture_output=True, text=True)
                if reusable:
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertEqual('package bytes',
                                     (root / 'packages/binary.tar.gz').read_text())
                    self.assertEqual('matching symbols',
                                     (root / 'symbols/omnirefl.debug').read_text())
                else:
                    self.assertNotEqual(0, result.returncode)
                    self.assertFalse((root / 'packages').exists())

    def test_required_master_ci_is_not_skipped_when_initially_absent(self):
        # The run appears after the first query, then fails validation.
        # gh run watch --exit-status: https://cli.github.com/manual/gh_run_watch
        service = '''\
#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
if sys.argv[1:3] == ['run', 'watch']:
    sys.exit(1)
path = sys.argv[2]
if '/actions/workflows/ci.yml/runs?' in path:
    marker = Path(os.environ['GITHUB_FIXTURE'])
    visible = marker.exists()
    marker.touch()
    result = {'workflow_runs': [{'id': 7}] if visible else []}
elif '/actions/runs?' in path:
    result = {'workflow_runs': []}
else:
    result = {'artifacts': []}
print(json.dumps(result))
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'gh').write_text(service)
            (root / 'gh').chmod(0o755)
            environment = dict(
                os.environ, GITHUB_REPOSITORY='example/project', GITHUB_SHA='a' * 40,
                GITHUB_OUTPUT=str(root / 'output'),
                GITHUB_FIXTURE=str(root / 'master-run-visible'),
                PATH=directory + os.pathsep + os.environ['PATH'])
            result = subprocess.run(
                [sys.executable, str(Path(__file__).resolve().parents[1] / 'reuse_ci.py'),
                 'find', '--wait-master', 'required', '--targets', 'extras'],
                env=environment, capture_output=True, text=True, timeout=45)
            self.assertEqual(1, result.returncode, result.stderr)
            self.assertFalse((root / 'output').exists())

    def test_exact_tested_commit(self):
        commit = 'a' * 40
        head = 'b' * 40
        base = 'd' * 40
        for name, head, requested, full, conclusion, parents, expected, full_text in [
            ('PR merge commit', head, commit, False, 'success', [base, head],
             {'run_id': '7', 'full_suite': 'false'}, None),
            ('publishing failed after full validation', commit, commit, True,
             'failure', [base, head], {'run_id': '7', 'full_suite': 'true'}, None),
            ('amended commit', head, 'c' * 40, False, 'success', [base, head],
             {'run_id': '', 'full_suite': 'false'}, None),
            ('ordinary push', commit, commit, False, 'success', [base, head],
             {'run_id': '7', 'full_suite': 'false'}, None),
            ('unrelated PR smoke claim', head, commit, False, 'success',
             [base, 'e' * 40], {'run_id': '', 'full_suite': 'false'}, None),
            ('unrelated PR full claim', head, commit, True, 'success',
             [base, 'e' * 40], {'run_id': '', 'full_suite': 'false'}, None),
            ('unrelated PR malformed full claim', head, commit, True, 'success',
             [base, 'e' * 40], {'run_id': '', 'full_suite': 'false'}, '{malformed'),
            ('reused full does not validate uploads', commit, commit, True,
             'success', [base, head], {'run_id': '', 'full_suite': 'true'},
             json.dumps({'suite': 'full', 'commit': commit, 'run_id': '7',
                         'run_attempt': '3', 'source_run_id': '8',
                         'source_commit': head})),
        ]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                artifacts = []

                def artifact(name, file, content):
                    number = len(artifacts) + 1
                    path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(path, 'w') as archive:
                        archive.writestr(file, content)
                    artifacts.append({
                        'id': number, 'name': name, 'expired': False,
                        'created_at': '2026-11-25T12:00:20Z' if name.startswith(
                            'validated-commit-') else '2026-11-25T12:00:10Z',
                        'digest': 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest(),
                        'workflow_run': {'id': 7, 'head_sha': head},
                    })

                artifact('packages-extras-' + commit[:12],
                         'package.tar.gz', 'package contents')
                artifact('validated-commit-' + commit, 'validated-commit.json',
                         json.dumps({'commit': commit, 'run_id': '7'}))
                if full:
                    attempt = '3' if name == 'reused full does not validate uploads' else '1'
                    artifact('full-suite-' + commit + '-attempt-' + attempt, 'full-suite.json',
                             full_text if full_text is not None else json.dumps({
                                 'suite': 'full', 'commit': commit,
                                 'run_id': '7', 'run_attempt': '1'}))
                run = {
                    'id': 7, 'head_sha': head, 'status': 'completed',
                    'conclusion': conclusion, 'run_attempt': 1,
                    'updated_at': '2026-11-25T12:00:25Z',
                    'run_started_at': '2026-11-25T12:00:00Z',
                    'event': 'pull_request' if head != commit else 'push',
                    'head_repository': {'full_name': 'example/project'},
                    'pull_requests': [{'base': {'sha': base}, 'head': {'sha': head}}],
                    'path': '.github/workflows/release.yml' if full
                            else '.github/workflows/ci.yml',
                }
                attempts = {}
                jobs = {}
                if name == 'reused full does not validate uploads':
                    artifacts[0]['created_at'] = '2026-11-25T12:00:30Z'
                    artifacts[1]['created_at'] = '2026-11-25T12:00:40Z'
                    artifacts[2]['created_at'] = '2026-11-25T12:00:49Z'
                    run.update(run_attempt=3,
                               run_started_at='2026-11-25T12:00:45Z',
                               updated_at='2026-11-25T12:00:50Z')
                    attempts = {str(n): {**run, 'run_attempt': n,
                                         'conclusion': 'failure'} for n in (1, 2)}
                    jobs = {'7:3': [{'name': 'Full test suite', 'conclusion': 'success',
                                     'completed_at': '2026-11-25T12:00:50Z'}]}
                (root / 'api.json').write_text(json.dumps({
                    'runs': [run], 'artifacts': artifacts, 'parents': parents,
                    'attempts': attempts, 'jobs': jobs,
                }))
                environment = dict(
                    os.environ, GITHUB_REPOSITORY='example/project', GITHUB_SHA=requested,
                    GITHUB_OUTPUT=str(root / 'output'),
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
                self.assertEqual({**expected,
                                  'artifact_commit': commit if expected['run_id'] else ''},
                                 output)
                result = subprocess.run(command + [
                    'download', '--run-id', '7', '--target', 'extras',
                    '--packages-dir', str(root / 'package'),
                ], env=environment, capture_output=True, text=True)
                if output['run_id']:
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertEqual('package contents',
                                     (root / 'package/package.tar.gz').read_text())
                else:
                    self.assertNotEqual(0, result.returncode)
                    self.assertIn('Package source is not successful CI', result.stderr)
                    self.assertFalse((root / 'package').exists())


if __name__ == '__main__':
    unittest.main()
