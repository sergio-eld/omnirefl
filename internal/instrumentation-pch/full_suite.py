"""Time the complete Linux package suite with controlled PCH/cache settings."""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--extras', type=pathlib.Path, required=True,
                    help='Extracted shipped extras archive, not an untracked checkout')
parser.add_argument('--package', type=pathlib.Path, required=True)
parser.add_argument('--root', type=pathlib.Path, required=True)
parser.add_argument('--dependencies', type=pathlib.Path)
parser.add_argument('--jobs', type=int, default=8)
parser.add_argument('--phase-timeout', type=int, default=1800)
args = parser.parse_args()
root = args.root.resolve()
if any((root / mode).exists() for mode in ('pch-baseline', 'cached', 'ci-baseline', 'targeted')):
    parser.error('Use a fresh --root directory for clean build comparisons')
source = root / 'source with space'
shutil.copytree(args.extras.resolve(), source)
common = ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_BENCHMARKS=OFF',
          '-Domnirefl_ROOT=' + str(args.package.resolve())]
if args.dependencies:
    for name in ('googletest', 'tl_expected', 'tl_optional', 'mpark_variant',
                 'string_view_lite', 'rapidyaml'):
        common += ['-DFETCHCONTENT_SOURCE_DIR_' + name.upper() + '=' +
                   str(args.dependencies.resolve() / (name + '-src'))]
policy = pathlib.Path(__file__).with_name('suite_pch_policy.cmake').resolve()
environment = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL=str(args.jobs))
results = []
for mode in ('pch-baseline', 'cached', 'ci-baseline', 'targeted'):
    build = root / mode
    record = {'mode': mode, 'parallel': args.jobs}
    configure = ['cmake', '-S', str(source), '-B', str(build), *common,
                 '-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=' +
                 ('' if mode == 'ci-baseline' else str(policy)),
                 '-DOMNIREFL_PCH_CACHE_DIR=' +
                 (str(root / 'cache') if mode in ('cached', 'targeted') else ''),
                 '-DPCH_RESEARCH_MIN_SOURCES=' + ('2' if mode == 'targeted' else '1')]
    commands = [('configure', configure),
                ('build', ['cmake', '--build', str(build), '--parallel', str(args.jobs)]),
                ('test', ['ctest', '--test-dir', str(build), '--no-tests=error',
                          '--timeout', '600', '--output-on-failure',
                          '--output-junit', str(root / (mode + '-tests.xml'))])]
    for phase, command in commands:
        started = time.monotonic()
        log_path = root / (mode + '-' + phase + '.log')
        with log_path.open('w') as log:
            run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                 env=environment, timeout=args.phase_timeout)
        record[phase + '_seconds'] = time.monotonic() - started
        record[phase + '_exit'] = run.returncode
        print(json.dumps(record), flush=True)
        (root / 'results.json').write_text(json.dumps([*results, record], indent=2))
        if run.returncode:
            print(log_path.read_text()[-6000:])
            raise SystemExit(run.returncode)
        if phase == 'test':
            summary = re.search(r'(\d+)% tests passed, (\d+) tests failed out of (\d+)',
                                log_path.read_text())
            if summary:
                record['tests'] = int(summary[3])
                record['failed_tests'] = int(summary[2])
    results.append(record)
(root / 'results.json').write_text(json.dumps(results, indent=2))
