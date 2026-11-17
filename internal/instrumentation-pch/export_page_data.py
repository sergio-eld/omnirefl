"""Export matched initial-build .cpp timings for the research page."""
import argparse
import json
import pathlib
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--logs', type=pathlib.Path, required=True)
parser.add_argument('--ninja', type=pathlib.Path, required=True)
parser.add_argument('--output', type=pathlib.Path, required=True)
parser.add_argument('--modes', nargs='+',
                    default=['master', 'prototype-disabled', 'targeted-current',
                             'ci-baseline', 'baseline', 'cached', 'targeted'])
args = parser.parse_args()
modes = tuple(args.modes)
source_marker = '/source with space/'


def instrumentation(path):
    result = {}
    text = path.read_text()
    chunks = re.split(r'(?m)^\[\d+/\d+\] ', text)
    for chunk in chunks:
        match = re.match(r'omnirefl: generating reflection for ([^:]+): (.+)\n', chunk)
        if not match:
            continue
        target, source = match.groups()
        if source_marker not in source:
            continue
        phases = dict((m[1], int(m[2]) / 1e6) for m in re.finditer(
            r'(?m)^([^:\n]+): .*\(([0-9]+) microseconds\)$', chunk))
        if 'parsed ast' not in phases:
            raise ValueError('Missing instrumentation timings: ' + source)
        key = (target, source.split(source_marker, 1)[1])
        if key in result:
            raise ValueError('Duplicate initial-build invocation: ' + str(key))
        result[key] = sum(phases.values())
    return result


def compilation(directory):
    # Ninja appends later CTest-driven rebuilds. Completion times are ordered
    # within one invocation; a clock reset marks the next invocation.
    durations = {}
    previous_end = -1
    for line in (directory / '.ninja_log').read_text().splitlines():
        if line.startswith('#'):
            continue
        columns = line.split('\t')
        if len(columns) != 5:
            continue
        start, end = map(int, columns[:2])
        if end < previous_end:
            break
        previous_end = end
        durations[columns[3]] = (end - start) / 1000
    result = {}
    for entry in json.loads((directory / 'compile_commands.json').read_text()):
        source = entry['file']
        if source_marker not in source or not source.endswith(('.cpp', '.cc', '.cxx', '.C')):
            continue
        output = str(pathlib.PurePosixPath(entry['output']).relative_to(entry['directory']))
        if output not in durations:
            continue
        target = re.search(r'(?:^|/)CMakeFiles/([^/]+)\.dir/', output)[1]
        key = (target, source.split(source_marker, 1)[1])
        if key in result:
            raise ValueError('Duplicate object mapping: ' + str(key))
        result[key] = durations[output]
    return result


parsed = {mode: instrumentation(args.logs / (mode + '-build.log')) for mode in modes}
compiled = {mode: compilation(args.ninja / mode) for mode in modes}
assert all(set(compiled[mode]) == set(compiled[modes[0]]) for mode in modes)
assert all(set(parsed[mode]) == set(parsed[modes[0]]) for mode in modes)
sources = sorted({source for _, source in compiled[modes[0]]})
files = []
for source in sources:
    contexts = sorted(key for key in compiled[modes[0]] if key[1] == source)
    policies = {}
    for mode in modes:
        instrument = sum(parsed[mode].get(key, 0) for key in contexts)
        compile_time = sum(compiled[mode][key] for key in contexts)
        policies[mode] = dict(instrumentation=instrument, compilation=compile_time,
                              combined=instrument + compile_time)
    files.append(dict(source=source, contexts=[key[0] for key in contexts], policies=policies))
summary = json.loads(pathlib.Path(__file__).with_name('results.json').read_text())
output = dict(fullSuite=summary['full_package_suite'], compilerProfiles=summary['profiles'],
              disabledPathProbe=summary.get('disabled_path_probe'),
              files=files, coverage=dict(files=len(files),
                  compiledContexts=len(compiled[modes[0]]),
                  instrumentedContexts=sum(key in parsed[modes[0]] for key in compiled[modes[0]]),
                  allInstrumentationCalls=len(parsed[modes[0]])))
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(output, indent=2) + '\n')
print(json.dumps(output['coverage']))
