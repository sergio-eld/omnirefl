#!/usr/bin/env python3
"""Select full validation by PR label, commit cadence, or scheduled idle time."""

import argparse
from datetime import datetime, timezone
import os
import subprocess
from urllib.parse import urlencode

from github_artifacts import api, pages


def select_pr(number):
    # Reruns retain the original event; labels may have changed since then.
    labels = api('pulls/' + str(number))['labels']
    full = any(label['name'] == 'ci:full' for label in labels)
    print('Suite: full (ci:full)' if full else 'Suite: smoke')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('full_suite=' + str(full).lower(), file=output)


def select(idle):
    from reuse_ci import merged_pr_full_validations

    commit = os.environ['GITHUB_SHA']
    history = subprocess.check_output(
        ['git', 'rev-list', '--first-parent', '--max-count=5', commit],
        text=True).splitlines()
    validated_distance = 5
    running = False
    reused = None
    for distance, revision in enumerate(history):
        validated = False
        runs = pages('actions/runs?' + urlencode({'head_sha': revision}),
                     'workflow_runs')
        for run in runs:
            path = run['path'].split('@')[0]
            if (str(run['id']) == os.environ['GITHUB_RUN_ID']
                    or run['event'] == 'pull_request'
                    or run['head_sha'] != revision
                    or path not in ('.github/workflows/ci.yml',
                                    '.github/workflows/release.yml',
                                    '.github/workflows/full_validation.yml')):
                continue
            if run['status'] != 'completed':
                active_full = (path == '.github/workflows/release.yml'
                               or run['display_title'].endswith('/ full'))
                if not active_full and (path == '.github/workflows/full_validation.yml'
                                        or run['display_title'].endswith('/ automatic')):
                    # Automatic runs choose their suite after the run is named.
                    jobs = pages('actions/runs/' + str(run['id']) + '/jobs', 'jobs')
                    active_full = any(job['name'].endswith(' / full') for job in jobs)
                running = running or active_full
                continue
            # Job results outlive artifact retention. PR head commits alone
            # cannot stand in for their tested merge commits.
            jobs = pages('actions/runs/' + str(run['id']) + '/jobs?filter=all',
                         'jobs')
            validated = validated or any(
                job['name'].split(' / ')[-1] == 'Full test suite'
                and job['conclusion'] == 'success' for job in jobs)
        if not validated:
            source = next(merged_pr_full_validations(revision), None)
            if source:
                validated = True
                if distance == 0:
                    reused = source
                label = 'Full reuse' if distance == 0 else 'Full baseline'
                print(f'{label}: run {source[0]["id"]} '
                      f'({source[1][:12]} -> {revision[:12]})')
        if validated:
            validated_distance = distance
            break

    if idle:
        timestamp = int(subprocess.check_output(
            ['git', 'show', '-s', '--format=%ct', commit], text=True))
        idle_days = (datetime.now(timezone.utc).timestamp() - timestamp) / 86400
    cadence_due = (validated_distance > 0
                   and (idle_days >= 3 if idle else validated_distance >= 5))
    due = cadence_due and not running
    selection = ('Run: full' if due else 'Run: none') if idle else (
        'Suite: full' if due else 'Suite: smoke')
    distance = (f'since full: {validated_distance}/5 commits' if validated_distance < 5
                else 'since green full: 5+/5 commits' if len(history) == 5
                else 'green full: none')
    if idle:
        distance += f'; idle: {int(idle_days * 24)}/72h'
    if running and cadence_due:
        distance += '; full run active'
    print(selection + '; ' + distance)
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('full_suite=' + str(due).lower(), file=output)
        if reused:
            print('reused_full_run_id=' + str(reused[0]['id']), file=output)
            print('reused_full_commit=' + reused[1], file=output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--idle', action='store_true',
                      help='Use the three-day idle condition instead of commit cadence')
    mode.add_argument('--pull-request', type=int, metavar='NUMBER',
                      help='Select from the current ci:full label on a PR')
    arguments = parser.parse_args()
    if arguments.pull_request is not None:
        select_pr(arguments.pull_request)
    else:
        select(arguments.idle)
