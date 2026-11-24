#!/usr/bin/env python3
"""Download GitHub Actions artifacts with digest and extraction checks."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from urllib.parse import urlencode
import zipfile


def api(path):
    repository = os.environ['GITHUB_REPOSITORY']
    for attempt in range(3):
        try:
            response = subprocess.check_output(
                ['gh', 'api', 'repos/' + repository + '/' + path])
        except subprocess.CalledProcessError:
            if attempt == 2:
                raise
            print('::warning::GitHub metadata request failed; retrying ' + path,
                  file=sys.stderr)
            time.sleep(2 ** (attempt + 1))
            continue
        return json.loads(response)


def pages(path, key):
    page = 1
    while page:
        separator = '&' if '?' in path else '?'
        entries = api(path + separator + urlencode({'per_page': 100, 'page': page}))[key]
        yield from entries
        page = page + 1 if len(entries) == 100 else 0


def artifacts(run_id):
    return list(pages('actions/runs/' + str(run_id) + '/artifacts', 'artifacts'))


def named_artifact(available, name):
    matches = [artifact for artifact in available if artifact['name'] == name]
    if len(matches) != 1:
        raise RuntimeError('Expected one artifact ' + name + ', found ' + str(len(matches)))
    return matches[0]


@contextmanager
def archive(artifact):
    expected = artifact.get('digest') or ''
    if artifact['expired'] or not expected.startswith('sha256:'):
        raise RuntimeError('Artifact is expired or lacks a SHA-256 digest: ' + artifact['name'])
    path = 'repos/' + os.environ['GITHUB_REPOSITORY'] + '/actions/artifacts/'
    for attempt in range(3):
        # A fresh file discards partial responses before a retry.
        with tempfile.TemporaryFile() as data:
            try:
                subprocess.run(['gh', 'api', path + str(artifact['id']) + '/zip'],
                               stdout=data, check=True)
            except subprocess.CalledProcessError:
                if attempt == 2:
                    raise
                print('::warning::Artifact download failed; retrying ' + artifact['name'],
                      file=sys.stderr)
                time.sleep(2 ** (attempt + 1))
                continue
            data.seek(0)
            digest = hashlib.sha256()
            for block in iter(lambda: data.read(1024 * 1024), b''):
                digest.update(block)
            if 'sha256:' + digest.hexdigest() != expected:
                raise RuntimeError('Artifact digest mismatch: ' + artifact['name'])
            data.seek(0)
            with zipfile.ZipFile(data) as content:
                yield content
            return


def extract(artifact, destination):
    destination = Path(destination).resolve()
    with archive(artifact) as content:
        for entry in content.infolist():
            path = (destination / entry.filename).resolve()
            if path != destination and destination not in path.parents:
                raise RuntimeError('Unsafe artifact entry: ' + entry.filename)
        destination.mkdir(parents=True, exist_ok=True)
        content.extractall(destination)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--name', required=True)
    parser.add_argument('--destination', required=True)
    arguments = parser.parse_args()
    extract(named_artifact(artifacts(arguments.run_id), arguments.name),
            arguments.destination)
