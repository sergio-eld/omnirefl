"""Input-equivalent reuse through GitHub's Git and Actions REST contracts.

Real Git trees and artifact ZIPs model an orchestration-only follow-up to a
native full run. Source/test changes and untrusted coverage must run again.
https://docs.github.com/en/rest/git/trees#get-a-tree
https://docs.github.com/en/rest/commits/commits#compare-two-commits
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

from test_package_reuse import GITHUB_CLI


class InputReuse(unittest.TestCase):
    def test_full_inputs_and_native_release_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / 'repo'
            repo.mkdir()

            def git(*args):
                return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()

            git('init', '-q')
            git('config', 'user.email', 'test@example.invalid')
            git('config', 'user.name', 'Test')
            workflow = '''env:
  CI_MATRIX: |
    cosmo:
      build:
        include: [{target: cosmo-universal}]
jobs:
  changes:
    steps: [{run: echo policy}]
  build:
    steps: [{run: echo build}]
'''
            for path, text in {'.github/workflows/ci.yml': workflow,
                    '.github/workflows/full_validation.yml': 'old schedule',
                    'include/api.hpp': '// source',
                    'extras/guide.cpp': '// guide', 'docs/ci.md': 'old docs'}.items():
                file = repo / path
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(text)
            git('add', '.')
            git('commit', '-qm', 'native baseline')
            source = git('rev-parse', 'HEAD')
            (repo / 'docs/ci.md').write_text('new docs')
            (repo / '.github/workflows/full_validation.yml').write_text('new schedule')
            (repo / '.github/workflows/ci.yml').write_text(workflow.replace('echo policy', 'echo new policy'))
            git('commit', '-qam', 'orchestration')
            target = git('rev-parse', 'HEAD')
            trees, blobs = {}, {}

            def snapshot(commit):
                entries = []
                for line in git('ls-tree', '-r', commit).splitlines():
                    info, path = line.split('\t')
                    mode, kind, sha = info.split()
                    entries.append(dict(path=path, mode=mode, type=kind, sha=sha))
                    if kind == 'blob':
                        content = subprocess.check_output(['git', '-C', str(repo), 'cat-file', 'blob', sha])
                        blobs[sha] = dict(encoding='base64', content=base64.b64encode(content).decode())
                trees[commit] = dict(truncated=False, tree=entries)

            snapshot(source)
            snapshot(target)
            artifacts = []
            proof = dict(commit=source, build_commit=source, suite='full', run_id='7', run_attempt='1')
            for name, filename, content, second in [
                ('packages-extras-' + source[:12], 'extras.tar.gz', 'extras', '10'),
                ('packages-cosmo-universal-' + source[:12], 'package.tar.gz', 'binary', '10'),
                ('symbols-cosmo-universal-' + source[:12], 'tool.debug', 'symbols', '10'),
                ('validated-commit-' + source, 'validated-commit.json', json.dumps(proof), '20'),
                ('full-suite-' + source + '-attempt-1', 'full-suite.json', json.dumps(proof), '24'),
            ]:
                number = len(artifacts) + 1
                path = root / (str(number) + '.zip')
                with zipfile.ZipFile(path, 'w') as archive:
                    archive.writestr(filename, content)
                artifacts.append(dict(id=number, name=name, expired=False,
                    created_at='2026-11-25T12:00:' + second + 'Z',
                    digest='sha256:' + hashlib.sha256(path.read_bytes()).hexdigest(),
                    workflow_run=dict(id=7, head_sha=source)))
            service = GITHUB_CLI.replace("path = url.path.split('/actions/')[1].split('/')", '''if '/git/trees/' in url.path:
    if url.path.rsplit('/', 1)[1] == data.get('unavailable_source'):
        sys.exit(1)
    print(json.dumps(data['trees'][url.path.rsplit('/', 1)[1]]))
    sys.exit()
if '/git/blobs/' in url.path:
    print(json.dumps(data['blobs'][url.path.rsplit('/', 1)[1]]))
    sys.exit()
if '/git/ref/heads/master' in url.path:
    print(json.dumps({'object': {'sha': data['master']}}))
    sys.exit()
if '/compare/' in url.path:
    print(json.dumps({'status': data.get('ancestry', 'ahead')}))
    sys.exit()
path = url.path.split('/actions/')[1].split('/')''')
            (root / 'gh').write_text(service)
            (root / 'gh').chmod(0o755)
            run = dict(id=7, head_sha=source, status='completed', conclusion='success',
                       event='push', path='.github/workflows/ci.yml', run_attempt=1)
            data = dict(runs=[run], artifacts=artifacts, trees=trees, blobs=blobs, master=target)
            environment = dict(os.environ, GITHUB_REPOSITORY='example/project', GITHUB_SHA=target,
                GITHUB_OUTPUT=str(root / 'output'), GITHUB_FIXTURE=directory,
                PATH=directory + os.pathsep + os.environ['PATH'])
            command = [sys.executable, str(Path(__file__).resolve().parents[1] / 'reuse_ci.py')]

            def select():
                (root / 'api.json').write_text(json.dumps(data))
                (root / 'output').write_text('')
                result = subprocess.run(command + ['full-inputs'], env=environment,
                                        capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                return dict(line.split('=', 1) for line in (root / 'output').read_text().splitlines())

            output = select()
            self.assertEqual('7', output['reused_full_run_id'])
            self.assertEqual(source, output['reused_full_commit'])
            self.assertEqual(source, output['reused_full_build_commit'])
            result = subprocess.run(command + ['find', '--native', '--targets', 'extras'],
                                    env=environment, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertIn('run_id=\n', (root / 'output').read_text())

            for path in ('include/api.hpp', 'extras/guide.cpp', '.github/workflows/ci.yml'):
                with self.subTest(changed=path):
                    file = repo / path
                    original = file.read_text()
                    file.write_text(original.replace('echo build', 'echo different build')
                                    if path.endswith('ci.yml') else original + '\n// changed')
                    git('commit', '-qam', 'workload change')
                    changed = git('rev-parse', 'HEAD')
                    snapshot(changed)
                    environment['GITHUB_SHA'] = changed
                    self.assertEqual('', select()['reused_full_run_id'])
                    file.write_text(original)
                    git('commit', '-qam', 'restore')
            environment['GITHUB_SHA'] = target
            for case in ('PR source', 'not on master', 'truncated tree', 'failed full gate', 'unavailable source'):
                with self.subTest(case=case):
                    data['unavailable_source'] = source if case == 'unavailable source' else ''
                    data['runs'][0]['event'] = 'pull_request' if case == 'PR source' else 'push'
                    data['ancestry'] = 'diverged' if case == 'not on master' else 'ahead'
                    trees[target]['truncated'] = case == 'truncated tree'
                    data['jobs'] = {'7:1': [dict(name='Full test suite',
                        conclusion='failure' if case == 'failed full gate' else 'success',
                        completed_at='2026-11-25T12:00:26Z')]}
                    self.assertEqual('', select()['reused_full_run_id'])
