"""Compare repository inputs to the packaged C++ build and test workload.

Unknown paths count as inputs. Only documentation and independently executed
CI policy are excluded; workflow job bodies and the platform matrix still count.
"""
import base64
from functools import lru_cache
import hashlib
import json

import yaml

from github_artifacts import api


POLICY_JOBS = {'changes', 'reusable-packages', 'check-ci-policy'}
POLICY_FILES = {
    '.github/workflows/full_validation.yml',
    'ci/full_validation.py', 'ci/reuse_ci.py', 'ci/validation_inputs.py',
    'README.md', 'CONVENTIONS.md', 'AGENTS.md', 'omnirefl-banner.png',
}


def workload_file(path):
    return not (path in POLICY_FILES or path.startswith(('ci/tests/', 'site/'))
                or (path.startswith('docs/') and path.endswith('.md')))


@lru_cache(maxsize=None)
def blob(sha):
    response = api('git/blobs/' + sha)
    if response['encoding'] != 'base64':
        raise ValueError('Unsupported Git blob encoding')
    return base64.b64decode(response['content']).decode()


def member(node, name):
    return next(value for key, value in node.value if key.value == name)


@lru_cache(maxsize=None)
def snapshot(commit):
    tree = api('git/trees/' + commit + '?recursive=1')
    if tree['truncated']:
        print('Input comparison unavailable: Git tree is truncated')
        return None
    entries = {entry['path']: entry for entry in tree['tree'] if entry['type'] != 'tree'}
    workflow = '.github/workflows/ci.yml'
    try:
        # Keep YAML scalar spelling/types and alias contents. Removing policy
        # jobs must not remove shared values referenced by a workload job.
        document = yaml.compose(blob(entries[workflow]['sha']), Loader=yaml.SafeLoader)
        jobs = member(document, 'jobs')
        jobs.value = [(key, value) for key, value in jobs.value if key.value not in POLICY_JOBS]
        matrix = yaml.safe_load(member(member(document, 'env'), 'CI_MATRIX').value)
        targets = ['extras'] + [row['target'] for runtime in matrix.values()
                               for row in runtime['build']['include']]
        workload = yaml.serialize(document)
    except (KeyError, StopIteration, TypeError, AttributeError, yaml.YAMLError) as error:
        print('Input comparison unavailable: invalid CI workload: ' + str(error))
        return None
    files = [(path, entry['mode'], entry['sha']) for path, entry in sorted(entries.items())
             if path != workflow and workload_file(path)]
    encoded = json.dumps([files, entries[workflow]['mode'], workload]).encode()
    return hashlib.sha256(encoded).hexdigest(), targets
