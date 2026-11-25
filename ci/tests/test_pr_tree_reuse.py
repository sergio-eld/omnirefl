"""Reuse a merged PR's full run only when GitHub proves identical source trees.

Exercise the real selection/download CLIs through the documented GitHub REST
boundary, sharing the artifact service fixture from test_package_reuse.
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

from test_package_reuse import GITHUB_CLI


class MergedPRReuse(unittest.TestCase):
    def test_merged_source_and_artifacts(self):
        for case in ('identical tree', 'different tree', 'amended head',
                     'unrelated merge head', 'advanced tested base',
                     'failed full gate', 'smoke only',
                     'expired packages', 'missing symbols', 'expired proof',
                     'failed rerun after green full', 'another attempt green',
                     'GitHub cleared merged PR metadata',
                     'successful rerun without package validation',
                     'successful rerun rebuilt packages',
                     'failed rerun left packages intact'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                environment = dict(os.environ, GIT_AUTHOR_NAME='CI fixture',
                                   GIT_AUTHOR_EMAIL='ci@example.test',
                                   GIT_COMMITTER_NAME='CI fixture',
                                   GIT_COMMITTER_EMAIL='ci@example.test')
                subprocess.run(['git', 'init', '-q', directory], check=True)

                def git(*args, **kwargs):
                    return subprocess.check_output(['git', '-C', directory, *args],
                                                   env=environment, text=True,
                                                   **kwargs).strip()

                # Real immutable trees/commits: a PR synthetic merge and its
                # squash merge have identical files but different SHAs/parents.
                empty_tree = git('mktree', input='')
                base = git('commit-tree', empty_tree, input='base')
                (root / 'source').write_text('int main() {}\n')
                git('add', 'source')
                tree = git('write-tree')
                head = git('commit-tree', tree, '-p', base, input='feature')
                tested_base = (git('commit-tree', empty_tree, '-p', base,
                                   input='unrelated base')
                               if case == 'advanced tested base' else base)
                tested_head = (git('commit-tree', tree, '-p', base, input='other head')
                               if case == 'unrelated merge head' else head)
                tested = git('commit-tree', tree, '-p', tested_base, '-p', tested_head,
                             input='PR')
                merged = git('commit-tree', empty_tree if case == 'different tree'
                             else tree, '-p', base, input='squash')
                commits = {sha: json.loads(git('show', '-s',
                           '--format={"sha":"%H","tree":{"sha":"%T"}}', sha))
                           for sha in (merged, tested)}
                commits[tested]['parents'] = [{'sha': tested_base}, {'sha': tested_head}]
                commits[merged]['parents'] = [{'sha': base}]
                artifacts = []

                def artifact(name, file, content, expired=False):
                    number = len(artifacts) + 1
                    path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(path, 'w') as archive:
                        archive.writestr(file, content)
                    artifacts.append({
                        'id': number, 'name': name, 'expired': expired,
                        'created_at': '2026-11-25T12:00:20Z' if name.startswith(
                            'validated-commit-') else '2026-11-25T12:00:10Z',
                        'digest': 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest(),
                        'workflow_run': {'id': 7, 'head_sha': head},
                    })

                artifact('packages-extras-' + tested[:12], 'package.tar.gz', tested,
                         expired=case == 'expired packages')
                artifact('packages-cosmo-universal-' + tested[:12],
                         'binary.tar.gz', tested)
                if case != 'missing symbols':
                    artifact('symbols-cosmo-universal-' + tested[:12],
                             'omnirefl.debug', 'paired symbols')
                artifact('validated-commit-' + tested, 'validated-commit.json',
                         json.dumps({'commit': tested, 'run_id': '7'}))
                if case != 'smoke only':
                    artifact('full-suite-' + tested + '-attempt-1', 'full-suite.json',
                             json.dumps({'suite': 'full', 'commit': tested,
                                         'run_id': '7', 'run_attempt': '1'}),
                             expired=case == 'expired proof')
                run = {
                    'id': 7, 'head_sha': head, 'status': 'completed',
                    'conclusion': 'failure' if case in (
                        'failed rerun after green full', 'failed rerun left packages intact')
                                  else 'success',
                    'run_attempt': 2 if case in ('failed rerun after green full',
                        'another attempt green', 'successful rerun without package validation',
                        'successful rerun rebuilt packages',
                        'failed rerun left packages intact') else 1,
                    'event': 'pull_request', 'display_title': 'CI / feature / automatic',
                    'head_repository': {'full_name': 'example/project'},
                    'pull_requests': [{'number': 1, 'base': {'sha': base},
                                      'head': {'sha': head}}],
                    'path': '.github/workflows/ci.yml',
                }
                if case == 'GitHub cleared merged PR metadata':
                    run['pull_requests'] = []
                if case in ('failed rerun after green full',
                            'successful rerun without package validation'):
                    artifacts[1]['created_at'] = '2026-11-25T12:00:30Z'
                if case == 'successful rerun rebuilt packages':
                    artifacts[1]['created_at'] = '2026-11-25T12:00:30Z'
                    artifacts[2]['created_at'] = '2026-11-25T12:00:30Z'
                    for entry in artifacts:
                        if entry['name'].startswith('validated-commit-'):
                            entry['created_at'] = '2026-11-25T12:00:40Z'
                (root / 'api.json').write_text(json.dumps({
                    'runs': [run], 'artifacts': artifacts, 'commits': commits,
                    'jobs': {'7': [{'name': 'Full test suite', 'conclusion': 'success'}],
                             '7:1': [{'name': 'Full test suite',
                                      'conclusion': 'failure' if case in (
                                          'failed full gate', 'another attempt green')
                                                    else 'success'}]},
                    'merged_pulls': {merged: [{
                        'number': 1, 'merged_at': '2026-11-25T12:00:00Z',
                        'merge_commit_sha': merged,
                        'base': {'ref': 'master', 'repo': {'full_name': 'example/project'}},
                        'head': {'sha': 'e' * 40 if case == 'amended head' else head,
                                 'repo': {'full_name': 'example/project'}},
                    }]},
                }))
                environment.update(
                    GITHUB_REPOSITORY='example/project', GITHUB_SHA=merged,
                    GITHUB_RUN_ID='100', GITHUB_OUTPUT=str(root / 'output'),
                    GITHUB_FIXTURE=directory,
                    PATH=directory + os.pathsep + os.environ['PATH'])

                def invoke(script, *args):
                    return subprocess.run(
                        [sys.executable, str(Path(__file__).resolve().parents[1] / script),
                         *args], cwd=directory, env=environment,
                        capture_output=True, text=True)

                result = invoke('reuse_ci.py', 'find', '--require-symbols',
                                '--targets', 'extras', 'cosmo-universal')
                self.assertEqual(0, result.returncode, result.stderr)
                output = dict(line.split('=', 1) for line
                              in (root / 'output').read_text().splitlines())
                valid = case in ('identical tree', 'expired packages', 'missing symbols',
                                 'failed rerun after green full',
                                 'GitHub cleared merged PR metadata', 'advanced tested base',
                                 'successful rerun without package validation',
                                 'successful rerun rebuilt packages',
                                 'failed rerun left packages intact')
                packages = case in ('identical tree', 'advanced tested base',
                                    'GitHub cleared merged PR metadata',
                                    'successful rerun rebuilt packages',
                                    'failed rerun left packages intact')
                self.assertEqual('7' if packages else '', output['run_id'])
                self.assertEqual(str(valid).lower(), output['full_suite'])
                if packages or not valid or case in (
                        'failed rerun after green full',
                        'successful rerun without package validation'):
                    result = invoke('reuse_ci.py', 'download', '--run-id', '7',
                                    '--target', 'cosmo-universal',
                                    '--packages-dir', str(root / 'packages'),
                                    '--symbols-dir', str(root / 'symbols'))
                    if packages:
                        self.assertEqual(0, result.returncode, result.stderr)
                        self.assertEqual(tested, (root / 'packages/binary.tar.gz').read_text())
                        self.assertEqual('paired symbols',
                                         (root / 'symbols/omnirefl.debug').read_text())
                    else:
                        self.assertNotEqual(0, result.returncode)
                        self.assertFalse((root / 'packages').exists())
                (root / 'output').unlink()
                result = invoke('full_validation.py')
                self.assertEqual(0, result.returncode, result.stderr)
                output = dict(line.split('=', 1) for line
                              in (root / 'output').read_text().splitlines())
                self.assertEqual(str(not valid).lower(), output['full_suite'])
                if valid:
                    self.assertEqual('7', output['reused_full_run_id'])
                    self.assertEqual(tested, output['reused_full_commit'])


if __name__ == '__main__':
    unittest.main()
