#!/usr/bin/env python3
"""Reuse successful CI packages; full-suite proofs are independent.

Exact commits qualify directly. A merged PR also qualifies when a synthetic
checkout of its final head was fully tested and equals the merged source tree.
Artifacts retain SHA-256 verification and their original binary provenance.
Used by both CI and release preflight.
"""

import argparse
from collections import Counter
from itertools import chain
import json
import os
import subprocess
import time
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


def full_suite_passed(run_id, available, commit=COMMIT):
    prefix = 'full-suite-' + commit + '-attempt-'
    for name, artifact in available.items():
        if not name.startswith(prefix):
            continue
        with archive(artifact) as content:
            proof = json.loads(content.read('full-suite.json'))
        attempt = str(proof.get('run_attempt', ''))
        if (proof.get('suite') == 'full' and proof.get('commit') == commit
                and str(proof.get('run_id')) == str(run_id)
                and attempt.isdecimal() and int(attempt) > 0
                and name == prefix + attempt):
            jobs = pages('actions/runs/' + str(run_id) + '/attempts/' + attempt
                         + '/jobs', 'jobs')
            if any(job['name'].split(' / ')[-1] == 'Full test suite'
                   and job['conclusion'] == 'success' for job in jobs):
                return True
    return False


def has_commit_proof(run_id, available, commit):
    marker = available.get('validated-commit-' + commit)
    if not marker:
        return False
    with archive(marker) as content:
        proof = json.loads(content.read('validated-commit.json'))
    return (proof.get('commit') == commit
            and str(proof.get('run_id')) == str(run_id))


def matches_commit(run, available, commit=COMMIT):
    if run.get('event') == 'pull_request' and run['head_sha'] == commit:
        # The PR head recorded by Actions is not the merge checkout it tested.
        return False
    if run['head_sha'] != commit:
        # A PR can author its own artifacts. Bind both proof kinds to GitHub's
        # recorded base/head parents before trusting a merge-commit claim.
        if (run.get('event') != 'pull_request'
                or (run.get('head_repository') or {}).get('full_name') != REPOSITORY):
            return False
        merged = api('git/commits/' + commit)
        parents = [parent['sha'] for parent in merged['parents']]
        if merged['sha'] != commit or len(parents) != 2 or not any(
                parents == [pr['base']['sha'], run['head_sha']]
                and pr['head']['sha'] == run['head_sha']
                for pr in run.get('pull_requests', [])):
            return False
        if not has_commit_proof(run['id'], available, commit):
            return False
    return True


def merged_pr_full_validations(commit, run_id=None):
    """Yield full proofs for the final PR tree actually merged as commit."""
    pulls = api('commits/' + commit + '/pulls')
    for pr in pulls:
        if (not pr.get('merged_at') or pr.get('merge_commit_sha') != commit
                or pr['base']['ref'] != 'master'
                or (pr['base'].get('repo') or {}).get('full_name') != REPOSITORY
                or (pr['head'].get('repo') or {}).get('full_name') != REPOSITORY):
            continue
        runs = ([api('actions/runs/' + str(run_id))] if run_id else pages(
            'actions/runs?' + urlencode({'head_sha': pr['head']['sha'],
                                         'event': 'pull_request'}), 'workflow_runs'))
        for run in runs:
            if (run['event'] != 'pull_request'
                    or run['head_sha'] != pr['head']['sha']
                    or run['path'].split('@')[0] != '.github/workflows/ci.yml'
                    or (run.get('head_repository') or {}).get('full_name') != REPOSITORY):
                continue
            available = available_artifacts(run['id'])
            for name in available:
                if not name.startswith('validated-commit-'):
                    continue
                tested = name[len('validated-commit-'):]
                if (tested == run['head_sha']
                        or len(tested) != 40
                        or any(c not in '0123456789abcdef' for c in tested)
                        or not has_commit_proof(run['id'], available, tested)
                        or not full_suite_passed(run['id'], available, tested)):
                    continue
                # GitHub clears run.pull_requests after merging. Bind the
                # immutable synthetic checkout to the merged PR's final head;
                # tree equality covers any change of base or merge strategy.
                checkout = api('git/commits/' + tested)
                if (checkout['sha'] != tested or len(checkout['parents']) != 2
                        or checkout['parents'][1]['sha'] != pr['head']['sha']):
                    continue
                if checkout['tree']['sha'] == api(
                        'git/commits/' + commit)['tree']['sha']:
                    yield run, tested, available


def reusable_packages(run, full_suite, available, commit, targets):
    # A run may fail in publishing after its full validation passed.
    if not successful_run(run) and not full_suite:
        return False
    # The tested-commit marker is uploaded after package builds and checks.
    # Reject replacements uploaded after that gate, even if a later docs-only
    # rerun turns the overall run green. Untouched earlier packages still qualify.
    # Check even on attempt 1: a rerun may start after fetching run metadata.
    marker = available.get('validated-commit-' + commit)
    cutoff = (marker or {}).get('created_at')
    if not cutoff or not has_commit_proof(run['id'], available, commit):
        return False
    names = [artifact_name('packages', t, commit) for t in targets]
    names += [artifact_name('symbols', t, commit) for t in targets if t != 'extras']
    return all(available[name].get('created_at')
               and available[name]['created_at'] < cutoff
               for name in names if name in available)


def artifact_name(kind, target, commit=COMMIT):
    return kind + '-' + target + '-' + commit[:12]


def find(args):
    if args.wait_master:
        # Preserve the release gate on any active commit CI before reuse.
        path = 'actions/workflows/ci.yml/runs?' + urlencode({
            'head_sha': COMMIT, 'branch': 'master', 'event': 'push', 'per_page': 1,
        })
        runs = []
        while not runs:
            runs = api(path)['workflow_runs']
            if runs:
                subprocess.run(['gh', 'run', 'watch', str(runs[0]['id']),
                                '--interval', '30', '--exit-status'], check=True)
            elif args.wait_master != 'required':
                break
            else:
                print('Waiting for master CI to appear for ' + COMMIT, flush=True)
                time.sleep(15)

    package_run = ''
    package_commit = ''
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
    candidates = chain(((run, COMMIT, None) for run in runs.values()),
                       merged_pr_full_validations(COMMIT))
    for run, source_commit, available in candidates:
        if run['path'].split('@')[0] not in (
                '.github/workflows/ci.yml', '.github/workflows/release.yml',
                '.github/workflows/full_validation.yml'):
            continue
        if available is None:
            available = available_artifacts(run['id'])
            if not matches_commit(run, available, source_commit):
                continue
            passed = full_suite_passed(run['id'], available, source_commit)
        else:
            passed = True  # Already verified by merged_pr_full_validations.
        full_suite = full_suite or passed
        if not reusable_packages(run, passed, available, source_commit, args.targets):
            continue
        required = [artifact_name('packages', t, source_commit) for t in args.targets]
        if args.require_symbols:
            required += [artifact_name('symbols', t, source_commit)
                         for t in args.targets if t != 'extras']
        if not package_run and all(name in available for name in required):
            package_run = str(run['id'])
            package_commit = source_commit
        if package_run and full_suite:
            break

    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('run_id=' + package_run, file=output)
        print('full_suite=' + str(full_suite).lower(), file=output)
    print(json.dumps({'commit': COMMIT, 'package_run': package_run,
                      'artifact_commit': package_commit, 'full_suite': full_suite}))


def download(args):
    run = api('actions/runs/' + args.run_id)
    available = available_artifacts(args.run_id)
    source_commit = COMMIT
    if not matches_commit(run, available):
        merged_source = next(merged_pr_full_validations(COMMIT, args.run_id), None)
        if not merged_source:
            raise RuntimeError('Package source is not successful CI for ' + COMMIT)
        run, source_commit, available = merged_source
        passed = True
    else:
        passed = full_suite_passed(args.run_id, available, source_commit)
    if not reusable_packages(run, passed, available, source_commit, [args.target]):
        raise RuntimeError('Package source is not successful CI for ' + COMMIT)
    print(json.dumps({'commit': COMMIT, 'source_run': args.run_id,
                      'artifact_commit': source_commit, 'target': args.target}))
    extract(available[artifact_name('packages', args.target, source_commit)],
            args.packages_dir)
    if args.symbols_dir:
        extract(available[artifact_name('symbols', args.target, source_commit)],
                args.symbols_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('find')
    command.add_argument('--targets', nargs='+', required=True)
    command.add_argument('--require-symbols', action='store_true')
    command.add_argument('--wait-master', nargs='?', const='if-present',
                         choices=('if-present', 'required'))
    command.set_defaults(invoke=find)
    command = commands.add_parser('download')
    command.add_argument('--run-id', required=True)
    command.add_argument('--target', required=True)
    command.add_argument('--packages-dir', required=True)
    command.add_argument('--symbols-dir')
    command.set_defaults(invoke=download)
    arguments = parser.parse_args()
    arguments.invoke(arguments)
