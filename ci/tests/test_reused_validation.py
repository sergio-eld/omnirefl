"""Run the workflow's aggregate gates with GitHub job-result contexts.

https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#needs-context
"""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest


WORKFLOW = Path(__file__).resolve().parents[2] / '.github/workflows/ci.yml'


def run_step(name, environment, directory=None):
    step = re.search(
        r'^      - name: ' + re.escape(name) + r'\n(.*?)'
        r'(?=^      - name:|^  [a-z][a-z0-9-]*:|\Z)',
        WORKFLOW.read_text(), re.DOTALL | re.MULTILINE).group(1)
    script = textwrap.dedent(step.split('        run: |\n', 1)[1])
    return subprocess.run(['bash', '-e', '-c', script],
                          env={**os.environ, **environment}, cwd=directory,
                          capture_output=True, text=True, timeout=10)


class ReusedValidation(unittest.TestCase):
    def test_package_reuse_decision(self):
        for name, event, full, force, rebuild, source, expected in [
            ('automatic master', 'push', False, False, False, '7', True),
            ('full cadence', 'push', True, False, False, '7', False),
            ('manual run', 'workflow_dispatch', False, False, False, '7', False),
            ('explicit build', 'push', False, True, False, '7', False),
            ('rejected release packages', 'push', False, False, True, '7', False),
            ('PR rerun', 'pull_request', False, False, False, '7', False),
            ('no verified source', 'push', False, False, False, '', False),
        ]:
            with self.subTest(case=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result = run_step('Report package reuse', {
                    'GITHUB_EVENT_NAME': event, 'GITHUB_REF': 'refs/heads/master',
                    'GITHUB_OUTPUT': str(root / 'outputs'),
                    'GITHUB_STEP_SUMMARY': str(root / 'summary'),
                    'GITHUB_SERVER_URL': 'https://github.com',
                    'GITHUB_REPOSITORY': 'example/project',
                    'FULL_SUITE': str(full).lower(), 'FORCE_BUILD': str(force).lower(),
                    'REBUILD_PACKAGES': str(rebuild).lower(), 'SOURCE_RUN': source,
                    'LOOKUP_PACKAGES': 'true'}, directory)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual('reuse_checks=' + str(expected).lower() + '\n',
                                 (root / 'outputs').read_text())

    def test_required_ci_requires_every_selected_result(self):
        for name, reused, failure, allowed in [
            ('verified smoke', True, None, True),
            ('unverified skipped tests', False, None, False),
            ('package restore failed', True, 'build-package-cosmo', False),
            ('test failed', True, 'test-package-windows-ucrt', False),
            ('source lookup failed', True, 'reusable-packages', False),
            ('policy failed', True, 'check-ci-policy', False),
            ('matrix failed', True, 'validate-ci-matrix', False),
            ('benchmark failed', True, 'benchmark-cosmo', False),
        ]:
            with self.subTest(case=name):
                jobs = {job: {'result': 'success', 'outputs': {}} for job in (
                    'changes', 'validate-ci-matrix', 'reusable-packages',
                    'check-ci-policy', 'build-package-extras', 'build-package-cosmo',
                    'build-package-musl', 'build-package-ucrt')}
                jobs['changes']['outputs'] = {
                    'build_sources': 'true', 'check_ci_policy': 'true'}
                jobs['reusable-packages']['outputs']['reuse_checks'] = str(reused).lower()
                for job in ('test-package-linux-cosmo', 'test-package-linux-musl',
                            'test-package-macos', 'test-package-windows-cosmo',
                            'test-package-windows-ucrt', 'benchmark-cosmo',
                            'benchmark-musl', 'check-site'):
                    jobs[job] = {'result': 'skipped', 'outputs': {}}
                if failure:
                    jobs[failure]['result'] = 'failure'
                result = run_step('Require successful package and test jobs', {
                    'JOB_RESULTS': json.dumps(jobs), 'GITHUB_EVENT_NAME': 'push'})
                self.assertEqual(0 if allowed else 1, result.returncode,
                                 result.stdout + result.stderr)

    def test_full_gate_needs_green_full_coverage(self):
        for name, reused, benchmarks, failure, allowed in [
            ('reused full', True, 'skipped', None, True),
            ('fresh full', False, 'success', None, True),
            ('smoke is not full', False, 'skipped', None, False),
            ('package validation failed', True, 'skipped', 'required-ci', False),
            ('selection failed', True, 'skipped', 'changes', False),
            ('benchmark failed', True, 'failure', None, False),
        ]:
            with self.subTest(case=name):
                jobs = {
                    'changes': {'result': 'success', 'outputs': {
                        'build_sources': 'true',
                        'reused_full_run_id': '7' if reused else ''}},
                    'required-ci': {'result': 'success'},
                    'benchmark-cosmo': {'result': benchmarks},
                    'benchmark-musl': {'result': benchmarks},
                }
                if failure:
                    jobs[failure]['result'] = 'failure'
                result = run_step('Require complete validation', {
                    'JOB_RESULTS': json.dumps(jobs)})
                self.assertEqual(0 if allowed else 1, result.returncode,
                                 result.stdout + result.stderr)

    def test_commit_proof_retains_reused_source(self):
        for reused in (False, True):
            with self.subTest(reused=reused), tempfile.TemporaryDirectory() as directory:
                result = run_step('Record the tested commit', {
                    'GITHUB_SHA': 'a' * 40, 'GITHUB_RUN_ID': '10',
                    'GITHUB_RUN_ATTEMPT': '2', 'OMNI_TEST_SUITE': 'smoke',
                    'REUSED_CHECKS': str(reused).lower(),
                    'SOURCE_RUN': '7', 'SOURCE_COMMIT': 'b' * 40}, directory)
                self.assertEqual(0, result.returncode, result.stderr)
                expected = {'commit': 'a' * 40, 'run_id': '10',
                            'run_attempt': '2', 'suite': 'smoke', 'build_commit': ''}
                if reused:
                    expected.update(source_run_id='7', source_commit='b' * 40)
                self.assertEqual(expected, json.loads(
                    (Path(directory) / 'validated-commit.json').read_text()))


if __name__ == '__main__':
    unittest.main()
