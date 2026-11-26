"""Exercise release steps against GitHub's external draft-release contract.

https://docs.github.com/en/rest/releases/releases
The fixture replaces authenticated GitHub, leaving the workflow shell intact.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[2]
GITHUB_CLI = '''\
#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

path = Path(os.environ['RELEASE_FIXTURE'])
release = json.loads(path.read_text())
args = sys.argv[1:]
if args[:2] == ['release', 'create']:
    if release is not None:
        sys.exit('release already exists')
    path.write_text(json.dumps({'id': 8, 'tag_name': args[2], 'draft': True,
                               'assets': []}))
elif args[0] == 'api':
    if '--method' in args and args[args.index('--method') + 1] == 'DELETE':
        path.write_text('null')
    elif any(arg.endswith('/releases') for arg in args):
        if release is not None:
            print(str(release['id']) + '\\t' + str(release['draft']).lower())
else:
    sys.exit('unsupported GitHub CLI operation')
'''


def run_step(name, environment):
    workflow = (ROOT / '.github/workflows/release.yml').read_text()
    step = re.search(r'^      - name: ' + re.escape(name) + r'\n(.*?)'
                     r'(?=^      - name:|^  [a-z]|\Z)', workflow,
                     re.DOTALL | re.MULTILINE).group(1)
    script = textwrap.dedent(step.split('        run: |\n', 1)[1])
    return subprocess.run(['bash', '-e', '-c', script], cwd=ROOT,
                          env=environment, capture_output=True, text=True,
                          timeout=10)


class DraftRelease(unittest.TestCase):
    def test_retag_refreshes_draft_but_preserves_published_release(self):
        for draft in (True, False):
            with self.subTest(draft=draft), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                old = {'id': 7, 'tag_name': 'v0.0.8', 'draft': draft,
                       'assets': ['old-commit.tar.gz']}
                state = root / 'release.json'
                state.write_text(json.dumps(old))
                (root / 'gh').write_text(GITHUB_CLI)
                (root / 'gh').chmod(0o755)
                environment = dict(os.environ, GH_TOKEN='fixture',
                    GITHUB_REPOSITORY='example/project', GITHUB_REF_NAME='v0.0.8',
                    GITHUB_OUTPUT=str(root / 'outputs'), RELEASE_FIXTURE=str(state),
                    PATH=directory + os.pathsep + os.environ['PATH'])
                preflight = run_step('Validate Release Target', environment)
                self.assertEqual(0 if draft else 1, preflight.returncode,
                                 preflight.stdout + preflight.stderr)
                self.assertEqual(None if draft else old, json.loads(state.read_text()))
                if draft:
                    result = run_step('Create Draft Prerelease', environment)
                    self.assertEqual(0, result.returncode, result.stderr)
                    self.assertEqual({'id': 8, 'tag_name': 'v0.0.8', 'draft': True,
                                      'assets': []}, json.loads(state.read_text()))

    def test_publication_during_validation_blocks_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            published = {'id': 7, 'tag_name': 'v0.0.8', 'draft': False,
                         'assets': ['published.tar.gz']}
            state = root / 'release.json'
            state.write_text(json.dumps(published))
            (root / 'gh').write_text(GITHUB_CLI)
            (root / 'gh').chmod(0o755)
            result = run_step('Create Draft Prerelease', dict(os.environ,
                GITHUB_REPOSITORY='example/project', GITHUB_REF_NAME='v0.0.8',
                RELEASE_FIXTURE=str(state), PATH=directory + os.pathsep + os.environ['PATH']))
            self.assertNotEqual(0, result.returncode)
            self.assertEqual(published, json.loads(state.read_text()))
