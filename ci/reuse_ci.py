#!/usr/bin/env python3
"""Reuse successful exact-commit CI packages; full-suite proofs are independent.

Only immutable commit IDs qualify. Downloaded artifacts retain their GitHub
SHA-256 verification; a full-suite marker additionally binds the tested commit
and producing run. Used by both CI and release preflight.
"""

import argparse
from collections import Counter
import json
import os
import subprocess
from urllib.parse import urlencode

from github_artifacts import api, archive, artifacts, extract, pages

REPOSITORY = os.environ['GITHUB_REPOSITORY']
COMMIT = os.environ['GITHUB_SHA']


def successful_run(run):
    return run['status'] == 'completed' and run['conclusion'] == 'success'


def available_artifacts(run_id):
    entries = artifacts(run_id)
    counts = Counter(artifact['name'] for artifact in entries)
    return {a['name']: a for a in entries
            if counts[a['name']] == 1 and not a['expired']
            and (a.get('digest') or '').startswith('sha256:')}


def full_suite_passed(run_id, available):
    prefix = 'full-suite-' + COMMIT + '-attempt-'
    for name, artifact in available.items():
        if not name.startswith(prefix):
            continue
        with archive(artifact) as content:
            proof = json.loads(content.read('full-suite.json'))
        if (proof.get('suite') == 'full' and proof.get('commit') == COMMIT
                and str(proof.get('run_id')) == str(run_id)
                and name == prefix + str(proof.get('run_attempt'))):
            jobs = pages('actions/runs/' + str(run_id) + '/jobs?filter=all', 'jobs')
            return any(job['name'].split(' / ')[-1] == 'Full test suite'
                       and job['conclusion'] == 'success' for job in jobs)
    return False


def matches_commit(run, available):
    if run['head_sha'] != COMMIT:
        # A PR can author its own artifacts. Bind both proof kinds to GitHub's
        # recorded base/head parents before trusting a merge-commit claim.
        if (run.get('event') != 'pull_request'
                or (run.get('head_repository') or {}).get('full_name') != REPOSITORY):
            return False
        merged = api('git/commits/' + COMMIT)
        parents = [parent['sha'] for parent in merged['parents']]
        if merged['sha'] != COMMIT or len(parents) != 2 or not any(
                parents == [pr['base']['sha'], run['head_sha']]
                and pr['head']['sha'] == run['head_sha']
                for pr in run.get('pull_requests', [])):
            return False
        marker = available.get('validated-commit-' + COMMIT)
        if not marker:
            return False
        with archive(marker) as content:
            proof = json.loads(content.read('validated-commit.json'))
        if (proof.get('commit') != COMMIT
                or str(proof.get('run_id')) != str(run['id'])):
            return False
    return True


def reusable_run(run, full_suite):
    if successful_run(run) or full_suite:
        # A release may fail in publishing after its full validation passed.
        return True
    # A flaky rerun does not invalidate an earlier green attempt of this commit.
    for attempt in range(int(run.get('run_attempt', 1)) - 1, 0, -1):
        previous = api('actions/runs/' + str(run['id']) + '/attempts/' + str(attempt))
        if previous['head_sha'] == run['head_sha'] and successful_run(previous):
            return True
    return False


def artifact_name(kind, target):
    return kind + '-' + target + '-' + COMMIT[:12]


def find(args):
    if args.wait_master:
        # Preserve the release gate on any active commit CI before reuse.
        runs = api('actions/workflows/ci.yml/runs?' + urlencode({
            'head_sha': COMMIT, 'branch': 'master', 'event': 'push', 'per_page': 1,
        }))['workflow_runs']
        if runs:
            subprocess.run(['gh', 'run', 'watch', str(runs[0]['id']),
                            '--interval', '30', '--exit-status'], check=True)

    package_run = ''
    full_suite = False
    runs = {run['id']: run for run in pages(
        'actions/runs?' + urlencode({'head_sha': COMMIT}), 'workflow_runs')}
    # The repository artifact index can find tested PR merge commits even
    # though the workflow-run index only exposes their head commits.
    markers = pages('actions/artifacts?' + urlencode({
        'name': 'validated-commit-' + COMMIT,
    }), 'artifacts')
    for marker in markers:
        if marker['expired']:
            continue
        run_id = marker['workflow_run']['id']
        if run_id not in runs:
            runs[run_id] = api('actions/runs/' + str(run_id))
    for run in runs.values():
        if run['path'].split('@')[0] not in (
                '.github/workflows/ci.yml', '.github/workflows/release.yml'):
            continue
        available = available_artifacts(run['id'])
        if not matches_commit(run, available):
            continue
        passed = full_suite_passed(run['id'], available)
        if not reusable_run(run, passed):
            continue
        full_suite = full_suite or passed
        required = [artifact_name('packages', t) for t in args.targets]
        if args.require_symbols:
            required += [artifact_name('symbols', t) for t in args.targets if t != 'extras']
        if not package_run and all(name in available for name in required):
            package_run = str(run['id'])
        if package_run and full_suite:
            break

    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('run_id=' + package_run, file=output)
        print('full_suite=' + str(full_suite).lower(), file=output)
    print(json.dumps({'commit': COMMIT, 'package_run': package_run,
                      'full_suite': full_suite}))


def download(args):
    run = api('actions/runs/' + args.run_id)
    available = available_artifacts(args.run_id)
    if (not matches_commit(run, available)
            or not reusable_run(run, full_suite_passed(args.run_id, available))):
        raise RuntimeError('Package source is not successful CI for ' + COMMIT)
    extract(available[artifact_name('packages', args.target)], args.packages_dir)
    if args.symbols_dir:
        extract(available[artifact_name('symbols', args.target)], args.symbols_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('find')
    command.add_argument('--targets', nargs='+', required=True)
    command.add_argument('--require-symbols', action='store_true')
    command.add_argument('--wait-master', action='store_true')
    command.set_defaults(invoke=find)
    command = commands.add_parser('download')
    command.add_argument('--run-id', required=True)
    command.add_argument('--target', required=True)
    command.add_argument('--packages-dir', required=True)
    command.add_argument('--symbols-dir')
    command.set_defaults(invoke=download)
    arguments = parser.parse_args()
    arguments.invoke(arguments)
