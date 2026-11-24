#!/usr/bin/env python3
"""Select periodic full validation from master history and prior full runs."""

from datetime import datetime, timezone
import os
import subprocess
from urllib.parse import urlencode

from github_artifacts import pages


def select():
    commit = os.environ['GITHUB_SHA']
    history = subprocess.check_output(
        ['git', 'rev-list', '--first-parent', '--max-count=5', commit],
        text=True).splitlines()
    validated_distance = 5
    running = False
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
                # Run metadata omits inputs; ci.yml exposes its profile in run-name.
                running = running or path != '.github/workflows/ci.yml' or (
                    run['display_title'].endswith('/ full'))
                continue
            # Job results outlive artifact retention. PR head commits cannot
            # stand in for their tested merge commits, so PR runs are excluded.
            jobs = pages('actions/runs/' + str(run['id']) + '/jobs?filter=all',
                         'jobs')
            validated = validated or any(
                job['name'].split(' / ')[-1] == 'Full test suite'
                and job['conclusion'] == 'success' for job in jobs)
        if validated:
            validated_distance = distance
            break

    timestamp = int(subprocess.check_output(
        ['git', 'show', '-s', '--format=%ct', commit], text=True))
    idle_days = (datetime.now(timezone.utc).timestamp() - timestamp) / 86400
    due = (not running and validated_distance > 0
           and (validated_distance >= 5 or idle_days >= 3))
    if running:
        reason = 'A full validation run is already active.'
    elif validated_distance == 0:
        reason = 'This commit already has a green full test run.'
    elif validated_distance >= 5:
        reason = 'No green full run covers the latest five first-parent commits.'
    elif idle_days >= 3:
        reason = 'Master has unvalidated commits and has been idle for at least three days.'
    else:
        reason = 'Fewer than five unvalidated commits; master has not been idle for three days.'
    print('Full test suite selected:', due, '-', reason)
    print('Recent full validation distance:', validated_distance,
          '; master idle days:', round(idle_days, 2),
          '; full validation active:', running)
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        print('full_suite=' + str(due).lower(), file=output)


if __name__ == '__main__':
    select()
