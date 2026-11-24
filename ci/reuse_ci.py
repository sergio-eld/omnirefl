#!/usr/bin/env python3
"""Reuse successful exact-commit CI packages; full-suite proofs are independent.

Only immutable commit IDs qualify. Downloaded artifacts retain their GitHub
SHA-256 verification; a full-suite marker additionally binds the tested commit
and producing run. Used by both CI and release preflight.
"""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
from urllib.parse import urlencode
import zipfile


REPOSITORY = os.environ['GITHUB_REPOSITORY']
COMMIT = os.environ['GITHUB_SHA']


def api(path, binary=False):
    data = subprocess.check_output(['gh', 'api', 'repos/' + REPOSITORY + '/' + path])
    return data if binary else json.loads(data)


def pages(path, key):
    page = 1
    while page:
        separator = '&' if '?' in path else '?'
        entries = api(path + separator + urlencode({'per_page': 100, 'page': page}))[key]
        yield from entries
        page = page + 1 if len(entries) == 100 else 0


def successful_run(run):
    return (run['head_sha'] == COMMIT
            and run['status'] == 'completed'
            and run['conclusion'] == 'success')


def artifacts(run_id):
    return {a['name']: a for a in pages('actions/runs/' + str(run_id) + '/artifacts',
                                      'artifacts')
            if not a['expired'] and (a.get('digest') or '').startswith('sha256:')}


def archive(artifact):
    data = api('actions/artifacts/' + str(artifact['id']) + '/zip', binary=True)
    digest = 'sha256:' + hashlib.sha256(data).hexdigest()
    if digest != artifact['digest']:
        raise RuntimeError('Artifact digest mismatch: ' + artifact['name'])
    return zipfile.ZipFile(io.BytesIO(data))


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


def reusable_run(run, full_suite):
    if run['head_sha'] != COMMIT:
        return False
    if successful_run(run) or full_suite:
        # A release may fail in publishing after its full validation passed.
        return True
    # A flaky rerun does not invalidate an earlier green attempt of this commit.
    for attempt in range(int(run.get('run_attempt', 1)) - 1, 0, -1):
        previous = api('actions/runs/' + str(run['id']) + '/attempts/' + str(attempt))
        if successful_run(previous):
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
    runs = pages('actions/runs?' + urlencode({'head_sha': COMMIT}),
                 'workflow_runs')
    for run in runs:
        if (run['head_sha'] != COMMIT or run['path'].split('@')[0] not in (
                '.github/workflows/ci.yml', '.github/workflows/release.yml')):
            continue
        available = artifacts(run['id'])
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


def extract(artifact, destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    with archive(artifact) as content:
        for entry in content.infolist():
            path = (destination / entry.filename).resolve()
            if destination.resolve() not in path.parents:
                raise RuntimeError('Unsafe artifact entry: ' + entry.filename)
        content.extractall(destination)


def download(args):
    run = api('actions/runs/' + args.run_id)
    available = artifacts(args.run_id)
    if not reusable_run(run, full_suite_passed(args.run_id, available)):
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
