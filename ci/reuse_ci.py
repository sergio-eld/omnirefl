#!/usr/bin/env python3
"""Reuse successful CI packages; full-suite proofs are independent.

Exact commits qualify directly. A merged PR also qualifies when a synthetic
checkout of its final head passed validation and equals the merged source tree.
Artifacts retain SHA-256 verification and their original binary provenance.
Used by both CI and release preflight.
"""

import argparse
import base64
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


def full_suite_passed(run_id, available, commit=COMMIT, artifacts_created_at=None,
                      build_commit=None):
    prefix = 'full-suite-' + commit + '-attempt-'
    for name, artifact in available.items():
        if not name.startswith(prefix):
            continue
        with archive(artifact) as content:
            proof = json.loads(content.read('full-suite.json'))
        # Reused coverage proves the tree, not uploads made in this run.
        if artifacts_created_at is not None and (
                'source_run_id' in proof or 'source_commit' in proof):
            continue
        if build_commit is not None and proof.get('build_commit') != build_commit:
            continue
        attempt = str(proof.get('run_attempt', ''))
        if (proof.get('suite') == 'full' and proof.get('commit') == commit
                and str(proof.get('run_id')) == str(run_id)
                and attempt.isdecimal() and int(attempt) > 0
                and name == prefix + attempt):
            jobs = pages('actions/runs/' + str(run_id) + '/attempts/' + attempt
                         + '/jobs', 'jobs')
            if any(job['name'].split(' / ')[-1] == 'Full test suite'
                   and job['conclusion'] == 'success'
                   and (artifacts_created_at is None
                        or (job.get('completed_at') or '') > artifacts_created_at)
                   for job in jobs):
                return True
    return False


def commit_proof(run_id, available, commit):
    marker = available.get('validated-commit-' + commit)
    if not marker:
        return None
    with archive(marker) as content:
        proof = json.loads(content.read('validated-commit.json'))
    if proof.get('commit') == commit and str(proof.get('run_id')) == str(run_id):
        return proof
    return None


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
        if not commit_proof(run['id'], available, commit):
            return False
    return True


def merged_pr_validations(commit, run_id=None):
    """Yield tested final PR checkouts whose tree was actually merged."""
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
                        or not commit_proof(run['id'], available, tested)):
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


def merged_pr_full_validations(commit, run_id=None):
    """Yield full-suite proofs for the merged tree, independently of packages."""
    for run, tested, available in merged_pr_validations(commit, run_id):
        if full_suite_passed(run['id'], available, tested):
            yield run, tested, available


def reusable_packages(run, full_suite, available, commit, targets):
    # The tested-commit marker is uploaded after package builds and checks.
    # Reject replacements uploaded after that gate, even if a later docs-only
    # rerun turns the overall run green. Untouched earlier packages still qualify.
    # Check even on attempt 1: a rerun may start after fetching run metadata.
    # A failed or unverifiable full-suite claim cannot stand in for green smoke.
    if not full_suite and any(name.startswith('full-suite-' + commit + '-attempt-')
                              for name in available):
        return False
    marker = available.get('validated-commit-' + commit)
    cutoff = (marker or {}).get('created_at')
    proof = commit_proof(run['id'], available, commit)
    if not cutoff or not proof:
        return False
    names = [artifact_name('packages', t, commit) for t in targets]
    names += [artifact_name('symbols', t, commit) for t in targets if t != 'extras']
    if not all(available[name].get('created_at')
               and available[name]['created_at'] < cutoff
               for name in names if name in available):
        return False
    newest = max((available[name]['created_at'] for name in names
                  if name in available), default='')
    # A later attempt can refresh the marker and replace uploads. Bind the
    # bytes to the successful attempt, including stale green run metadata.
    if full_suite and full_suite_passed(
            run['id'], available, commit, artifacts_created_at=newest):
        return True
    # Current markers are published after package, test, and benchmark gates.
    # Verify that exact attempt; later docs-only success validates no new bytes.
    if 'suite' in proof or 'run_attempt' in proof:
        attempt = str(proof.get('run_attempt', ''))
        if (proof.get('suite') not in ('smoke', 'full')
                or not attempt.isdecimal() or int(attempt) < 1):
            return False
        jobs = pages('actions/runs/' + str(run['id']) + '/attempts/' + attempt
                     + '/jobs', 'jobs')
        return any(job['name'].split(' / ')[-1] == 'Required CI'
                   and job['conclusion'] == 'success'
                   and cutoff <= (job.get('completed_at') or '') for job in jobs)
    # Legacy markers lack an attempt. Only accept uploads from within a green
    # attempt's interval, not replacements left by a later failed attempt.
    if (successful_run(run) and run.get('run_started_at')
            and run['run_started_at'] <= newest < (run.get('updated_at') or '')):
        return True
    # A failed rerun does not invalidate an earlier green smoke attempt.
    for attempt in range(int(run.get('run_attempt', 1)) - 1, 0, -1):
        previous = api('actions/runs/' + str(run['id']) + '/attempts/' + str(attempt))
        if (previous['head_sha'] == run['head_sha'] and successful_run(previous)
                and previous.get('run_started_at')
                and previous['run_started_at'] <= newest < (previous.get('updated_at') or '')):
            return True
    return False


def artifact_name(kind, target, commit=COMMIT):
    return kind + '-' + target + '-' + commit[:12]


def wait_active():
    # Order attempts, not original run creation: reruns can start after newer
    # full runs. A deterministic tie-break avoids selectors waiting in a cycle.
    def started(run):
        attempt = api('actions/runs/' + str(run['id']) + '/attempts/'
                      + str(run['run_attempt']))
        return attempt['run_started_at'], int(run['id'])

    current = started(api('actions/runs/' + os.environ['GITHUB_RUN_ID']))
    for run in pages('actions/runs?' + urlencode({'head_sha': COMMIT}), 'workflow_runs'):
        if (str(run['id']) != os.environ['GITHUB_RUN_ID']
                and run['status'] != 'completed' and run['event'] != 'pull_request'
                and run['path'].split('@')[0] in (
                    '.github/workflows/ci.yml', '.github/workflows/release.yml',
                    '.github/workflows/full_validation.yml')
                and started(run) < current):
            subprocess.run(['gh', 'run', 'watch', str(run['id']),
                            '--interval', '30', '--exit-status'], check=True)


def wait_for_runs(args):
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

    if args.wait_active:
        wait_active()


def find(args):
    wait_for_runs(args)
    package_run = ''
    package_commit = ''
    build_commit = ''
    manifest = {}
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
                       () if args.native else merged_pr_validations(COMMIT))
    for run, source_commit, available in candidates:
        if run['path'].split('@')[0] not in (
                '.github/workflows/ci.yml', '.github/workflows/release.yml',
                '.github/workflows/full_validation.yml'):
            continue
        if available is None:
            available = available_artifacts(run['id'])
            if not matches_commit(run, available, source_commit):
                continue
        proof = commit_proof(run['id'], available, source_commit) or {}
        origin = proof.get('build_commit', '')
        if args.native and (source_commit != COMMIT or origin != COMMIT
                            or run['event'] == 'pull_request'):
            continue
        passed = full_suite_passed(run['id'], available, source_commit)
        if not args.native:
            full_suite = full_suite or passed
        if not reusable_packages(run, passed, available, source_commit, args.targets):
            continue
        required = [artifact_name('packages', t, source_commit) for t in args.targets]
        if args.require_symbols:
            required += [artifact_name('symbols', t, source_commit)
                         for t in args.targets if t != 'extras']
        if all(name in available for name in required):
            native_full = args.native and full_suite_passed(
                run['id'], available, source_commit,
                artifacts_created_at=max(available[name]['created_at'] for name in required),
                build_commit=COMMIT)
            if not package_run or native_full:
                package_run = str(run['id'])
                package_commit = source_commit
                build_commit = origin
                manifest = {name: {key: available[name][key] for key in ('id', 'digest')}
                            for name in required}
                if args.native:
                    full_suite = native_full
        if package_run and full_suite:
            break

    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('run_id=' + package_run, file=output)
        print('artifact_commit=' + package_commit, file=output)
        if build_commit:
            print('build_commit=' + build_commit, file=output)
        if package_run and (args.native or build_commit):
            encoded = base64.urlsafe_b64encode(json.dumps(manifest).encode()).decode()
            print('artifact_manifest=' + encoded, file=output)
        print('full_suite=' + str(full_suite).lower(), file=output)
    print(json.dumps({'commit': COMMIT, 'package_run': package_run,
                      'artifact_commit': package_commit, 'full_suite': full_suite}))


def full_inputs(args):
    """Reuse original full coverage, without relabeling or restaging binaries."""
    from validation_inputs import snapshot

    wait_for_runs(args)
    current = snapshot(COMMIT)
    reused = {}
    if current:
        fingerprint, targets = current
        master = api('git/ref/heads/master')['object']['sha']
        seen = set()
        for marker in pages('actions/artifacts', 'artifacts'):
            if marker['expired'] or not marker['name'].startswith('full-suite-'):
                continue
            run_id = marker['workflow_run']['id']
            if run_id in seen:
                continue
            seen.add(run_id)
            try:
                run = api('actions/runs/' + str(run_id))
                source = run['head_sha']
                if (run['status'] != 'completed' or run.get('event') == 'pull_request'
                        or run['path'].split('@')[0] not in (
                            '.github/workflows/ci.yml', '.github/workflows/release.yml',
                            '.github/workflows/full_validation.yml')):
                    continue
                # Only native coverage on master's history is an original source.
                # Never promote a derived proof or an arbitrary branch's artifacts.
                if source != master and api('compare/' + source + '...' + master)[
                        'status'] not in ('ahead', 'identical'):
                    continue
                previous = snapshot(source)
                if not previous or previous[0] != fingerprint:
                    continue
                available = available_artifacts(run_id)
                proof = commit_proof(run_id, available, source) or {}
                if proof.get('build_commit') != source:
                    continue
                names = [artifact_name('packages', target, source) for target in targets]
                names += [artifact_name('symbols', target, source)
                          for target in targets if target != 'extras']
                if not all(name in available and available[name].get('created_at') for name in names):
                    continue
                if (full_suite_passed(run_id, available, source, build_commit=source,
                        artifacts_created_at=max(available[name]['created_at'] for name in names))
                        and reusable_packages(run, True, available, source, targets)):
                    reused = dict(reused_full_run_id=str(run_id), reused_full_commit=source,
                                  reused_full_build_commit=source)
                    break
            except subprocess.CalledProcessError:
                # Historical runs/commits can disappear. Unavailable metadata
                # cannot grant coverage; fall back to normal package work.
                print('::warning::Full source unavailable: run ' + str(run_id))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        for key in ('reused_full_run_id', 'reused_full_commit', 'reused_full_build_commit'):
            print(key + '=' + reused.get(key, ''), file=output)
    print(json.dumps({'commit': COMMIT, **reused}))


def download(args):
    run = api('actions/runs/' + args.run_id)
    available = available_artifacts(args.run_id)
    source_commit = COMMIT
    if not matches_commit(run, available):
        merged_source = next(merged_pr_validations(COMMIT, args.run_id), None)
        if not merged_source:
            raise RuntimeError('Package source is not successful CI for ' + COMMIT)
        run, source_commit, available = merged_source
    proof = commit_proof(args.run_id, available, source_commit) or {}
    if args.native and (source_commit != COMMIT or proof.get('build_commit') != COMMIT
                        or run['event'] == 'pull_request'):
        raise RuntimeError('Package source was not built from ' + COMMIT)
    passed = full_suite_passed(args.run_id, available, source_commit)
    if not reusable_packages(run, passed, available, source_commit, [args.target]):
        raise RuntimeError('Package source is not successful CI for ' + COMMIT)
    if args.native and not args.manifest:
        raise RuntimeError('Native download requires its selected artifact manifest')
    if args.manifest:
        manifest = json.loads(base64.urlsafe_b64decode(args.manifest))
        names = [artifact_name('packages', args.target, source_commit)]
        if args.symbols_dir:
            names.append(artifact_name('symbols', args.target, source_commit))
        for name in names:
            selected = {key: available.get(name, {}).get(key) for key in ('id', 'digest')}
            if selected != manifest.get(name):
                raise RuntimeError('Selected artifact was replaced: ' + name)
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
    command.add_argument('--native', action='store_true',
                         help='Require original builds from this exact commit')
    command.add_argument('--wait-active', action='store_true')
    command.add_argument('--wait-master', nargs='?', const='if-present',
                         choices=('if-present', 'required'))
    command.set_defaults(invoke=find)
    command = commands.add_parser('full-inputs',
                                  help='Find native full coverage with identical workload inputs')
    command.add_argument('--wait-active', action='store_true')
    command.add_argument('--wait-master', nargs='?', const='if-present',
                         choices=('if-present', 'required'))
    command.set_defaults(invoke=full_inputs)
    command = commands.add_parser('download')
    command.add_argument('--native', action='store_true')
    command.add_argument('--run-id', required=True)
    command.add_argument('--target', required=True)
    command.add_argument('--manifest')
    command.add_argument('--packages-dir', required=True)
    command.add_argument('--symbols-dir')
    command.set_defaults(invoke=download)
    arguments = parser.parse_args()
    arguments.invoke(arguments)
