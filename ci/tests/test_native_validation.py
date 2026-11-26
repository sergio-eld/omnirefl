"""Native release policy through the documented Actions REST artifact boundary.

Use real marker ZIPs/digests and the external service fixture documented in
 test_package_reuse; no project collaborator is replaced.
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
from test_reused_validation import run_step


class NativeValidation(unittest.TestCase):
    def test_full_restage_preserves_original_build(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run_step('Record the tested commit', {
                'GITHUB_SHA': 'a' * 40, 'GITHUB_RUN_ID': '10',
                'GITHUB_RUN_ATTEMPT': '1', 'OMNI_TEST_SUITE': 'full',
                'REUSED_CHECKS': 'false', 'SOURCE_RUN': '7',
                'SOURCE_COMMIT': 'a' * 40, 'SOURCE_BUILD_COMMIT': 'b' * 40}, directory)
            self.assertEqual(0, result.returncode, result.stderr)
            proof = json.loads((Path(directory) / 'validated-commit.json').read_text())
            self.assertEqual('b' * 40, proof.get('build_commit'))

    def test_full_proof_retains_original_build(self):
        for source, origin in [('', 'a' * 40), ('7', 'b' * 40), ('7', '')]:
            with self.subTest(source=source, origin=origin), tempfile.TemporaryDirectory() as directory:
                result = run_step('Record successful full-suite validation', {
                    'GITHUB_SHA': 'a' * 40, 'GITHUB_RUN_ID': '10',
                    'GITHUB_RUN_ATTEMPT': '2', 'SOURCE_RUN': source,
                    'SOURCE_BUILD_COMMIT': origin, 'REUSED_FULL_RUN': '',
                    'REUSED_FULL_COMMIT': ''}, directory)
                self.assertEqual(0, result.returncode, result.stderr)
                proof = json.loads((Path(directory) / 'full-suite.json').read_text())
                self.assertEqual(origin, proof['build_commit'])

    def test_native_packages_and_full_coverage(self):
        commit = 'a' * 40
        for case, origin, full, derived, missing, expected, covered in [
            ('restaged synthetic merge', 'b' * 40, True, False, False, '', False),
            ('unknown legacy origin', None, True, False, False, '', False),
            ('exact smoke', commit, False, False, False, '7', False),
            ('exact full', commit, True, False, False, '7', True),
            ('active exact full', commit, True, False, False, '7', True),
            ('old rerun waits active full', commit, True, False, False, '7', True),
            ('derived tree coverage', commit, True, True, False, '7', False),
            ('missing package', commit, True, False, True, '', False),
            ('failed full job', commit, True, False, False, '', False),
        ]:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                artifacts = []

                def artifact(name, filename, content, second):
                    number = len(artifacts) + 1
                    path = root / (str(number) + '.zip')
                    with zipfile.ZipFile(path, 'w') as archive:
                        archive.writestr(filename, content)
                    artifacts.append({'id': number, 'name': name, 'expired': False,
                        'created_at': '2026-11-25T12:00:' + second + 'Z',
                        'digest': 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest(),
                        'workflow_run': {'id': 7, 'head_sha': commit}})

                proof = {'commit': commit, 'run_id': '7', 'run_attempt': '1',
                         'suite': 'full' if full else 'smoke'}
                if origin is not None:
                    proof['build_commit'] = origin
                if not missing:
                    artifact('packages-extras-' + commit[:12], 'package.tar.gz',
                             'native bytes', '10')
                artifact('validated-commit-' + commit, 'validated-commit.json',
                         json.dumps(proof), '20')
                if full:
                    full_proof = {**proof, 'suite': 'full'}
                    if derived:
                        full_proof.update(source_run_id='8', source_commit='b' * 40)
                    artifact('full-suite-' + commit + '-attempt-1', 'full-suite.json',
                             json.dumps(full_proof), '24')
                run = {'id': 7, 'head_sha': commit, 'status': 'completed',
                       'conclusion': 'success', 'event': 'push',
                       'path': '.github/workflows/ci.yml', 'run_attempt': 1}
                (root / 'api.json').write_text(json.dumps({
                    'runs': [run], 'artifacts': artifacts, 'jobs': {'7:1': [
                        {'name': 'Required CI', 'conclusion': 'success',
                         'completed_at': '2026-11-25T12:00:25Z'},
                        {'name': 'Full test suite', 'conclusion': 'failure' if case ==
                            'failed full job' else 'success',
                         'completed_at': '2026-11-25T12:00:26Z'}]}}))
                if case in ('active exact full', 'old rerun waits active full'):
                    completed = json.loads((root / 'api.json').read_text())
                    current = {**run, 'id': 100, 'status': 'in_progress',
                        'created_at': '2026-11-25T12:00:00Z' if case ==
                            'old rerun waits active full' else '2026-11-25T12:00:40Z'}
                    current['run_attempt'] = 2 if case == 'old rerun waits active full' else 1
                    completed['attempts'] = {
                        '100:' + str(current['run_attempt']): {'run_started_at': '2026-11-25T12:00:40Z'},
                        '7:1': {'run_started_at': '2026-11-25T12:00:10Z'}}
                    completed['runs'].append(current)
                    (root / 'completed.json').write_text(json.dumps(completed))
                    active = {**completed, 'runs': [{**run, 'status': 'in_progress',
                        'created_at': '2026-11-25T12:00:10Z'}, current], 'artifacts': []}
                    (root / 'api.json').write_text(json.dumps(active))
                    service = GITHUB_CLI.replace(
                        "data = json.loads((root / 'api.json').read_text())",
                        "if sys.argv[1:3] == ['run', 'watch']:\n"
                        "    (root / 'api.json').write_text((root / 'completed.json').read_text())\n"
                        "    sys.exit()\n"
                        "data = json.loads((root / 'api.json').read_text())")
                    service = service.replace(
                        "result = data['attempts'][path[3]]",
                        "result = data['attempts'][path[1] + ':' + path[3]]")
                    service = service.replace(
                        "{'artifacts': data['artifacts']}",
                        "{'artifacts': [a for a in data['artifacts'] if str(a['workflow_run']['id']) == path[1]]}")
                    (root / 'gh').write_text(service)
                environment = dict(os.environ, GITHUB_REPOSITORY='example/project',
                    GITHUB_SHA=commit, GITHUB_RUN_ID='100',
                    GITHUB_OUTPUT=str(root / 'output'), GITHUB_FIXTURE=directory,
                    PATH=directory + os.pathsep + os.environ['PATH'])
                command = [sys.executable, str(Path(__file__).resolve().parents[1]
                           / 'reuse_ci.py')]
                wait = ['--wait-active'] if case in (
                    'active exact full', 'old rerun waits active full') else []
                result = subprocess.run(command + ['find', '--native', *wait, '--targets', 'extras'],
                    env=environment, capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                output = dict(line.split('=', 1) for line
                              in (root / 'output').read_text().splitlines())
                self.assertEqual(expected, output['run_id'])
                self.assertEqual(str(covered).lower(), output['full_suite'])
                if expected:
                    self.assertEqual(commit, output['build_commit'])
                    self.assertTrue(output.get('artifact_manifest'), 'Native selection must pin its bytes')
                result = subprocess.run(command + ['download', '--native', '--manifest', output.get('artifact_manifest', ''), '--run-id', '7',
                    '--target', 'extras', '--packages-dir', str(root / 'packages')],
                    env=environment, capture_output=True, text=True)
                self.assertEqual(0 if expected else 1, result.returncode, result.stderr)
                if expected:
                    self.assertEqual('native bytes', (root / 'packages/package.tar.gz').read_text())
                    replacement = dict(artifacts[0], id=99)
                    replacement_path = root / '99.zip'
                    with zipfile.ZipFile(replacement_path, 'w') as archive:
                        archive.writestr('package.tar.gz', 'replacement bytes')
                    replacement['digest'] = 'sha256:' + hashlib.sha256(
                        replacement_path.read_bytes()).hexdigest()
                    data = json.loads((root / 'api.json').read_text())
                    data['artifacts'][0] = replacement
                    (root / 'api.json').write_text(json.dumps(data))
                    result = subprocess.run(command + ['download', '--native', '--manifest',
                        output['artifact_manifest'], '--run-id', '7', '--target', 'extras',
                        '--packages-dir', str(root / 'replacement')], env=environment,
                        capture_output=True, text=True)
                    self.assertEqual(1, result.returncode)
                    self.assertIn('Selected artifact was replaced', result.stderr)
                    self.assertFalse((root / 'replacement').exists())
