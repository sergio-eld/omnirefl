#!/usr/bin/env python3
"""Compare backend compilation of one fixed CMake workload against two source refs.

Run inside the chosen Linux compiler container; see docs/performance-tracking.md.
The installed instrumentation tool, dependencies, workload, flags and build type
stay fixed. Only OMNI_PERF_SOURCE_DIR changes. No checkout is changed, and this
script is never invoked by CI.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tarfile
import time


def run(command, log_path, **options):
    with log_path.open('w') as log:
        process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                 **options)
    if process.returncode:
        print('Command failed ({}); full log: {}'.format(process.returncode,
                                                        log_path), file=sys.stderr)
        print('\n'.join(log_path.read_text().splitlines()[-20:]), file=sys.stderr)
        raise SystemExit(process.returncode)


def git(repository, *arguments):
    return subprocess.check_output(
        ['git', '-c', 'safe.directory=' + str(repository), '-C', str(repository),
         *arguments])


def snapshot(repository, ref, destination, excluded):
    destination.mkdir(parents=True)
    if ref == 'WORKTREE':
        files = git(repository, 'ls-files', '-coz', '--exclude-standard').split(b'\0')
        for name in set(files) - {b''}:
            relative = Path(os.fsdecode(name))
            source = repository / relative
            if source.is_file() and excluded not in source.parents:
                (destination / relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(str(source), str(destination / relative))
        digest = hashlib.sha256()
        for path in sorted(destination.rglob('*')):
            if path.is_file():
                digest.update(str(path.relative_to(destination)).encode())
                digest.update(path.read_bytes())
        return (git(repository, 'rev-parse', 'HEAD').decode().strip()
                + '+worktree:' + digest.hexdigest())
    archive = git(repository, 'archive', ref)
    with tarfile.open(fileobj=io.BytesIO(archive)) as content:
        options = {'filter': 'data'} if hasattr(tarfile, 'data_filter') else {}
        content.extractall(str(destination), **options)
    return git(repository, 'rev-parse', ref).decode().strip()


def launcher(arguments):
    # A separate process measures each compiler invocation, excluding reflection,
    # linking and unrelated targets. Linux reports ru_maxrss in KiB.
    import resource
    directory, label = Path(arguments[0]), arguments[1]
    command = arguments[2:]
    started = time.monotonic()
    process = subprocess.run(command)
    output = Path(command[command.index('-o') + 1]) if '-o' in command else None
    record = {
        'label': label, 'command': command, 'exit': process.returncode,
        'sample': int(os.environ.get('OMNI_PERF_SAMPLE', '0')),
        'seconds': time.monotonic() - started,
        'peak_rss_kib': resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
        'object': str(output) if output else None,
        'object_bytes': output.stat().st_size if output and output.exists() else None,
    }
    directory.mkdir(parents=True, exist_ok=True)
    name = hashlib.sha256(json.dumps(command).encode()).hexdigest()[:12]
    (directory / (label + '-' + name + '-' + str(os.getpid()) + '.json')) \
        .write_text(json.dumps(record, indent=2) + '\n')
    if process.returncode < 0:
        print('Compiler terminated by signal {}'.format(-process.returncode),
              file=sys.stderr)
        return 128 - process.returncode
    return process.returncode


def summary(output, targets, repetitions):
    records = [json.loads(p.read_text()) for p in (output / 'raw').glob('*.json')]
    groups = {}
    for record in records:
        if record['exit']:
            continue
        if not record['object']:
            continue
        target = next((p[:-4] for p in Path(record['object']).parts
                       if p.endswith('.dir')), None)
        if target not in targets or record.get('sample', -1) < 0:
            continue
        groups.setdefault((target, record['label']), []).append(record)
    for target in targets:
        for label in ('baseline', 'current'):
            group = groups.get((target, label), [])
            samples = sorted(r['sample'] for r in group)
            if samples != list(range(repetitions)):
                raise SystemExit('Incomplete compiler measurements for {} {}: '
                                 'expected one object per sample 0..{}, got {}. '
                                 'Inspect logs and raw records in {}'.format(
                                     target, label, repetitions - 1, samples, output))
    lines = ['| Target | Ref | Time, median (s) | Time CV | Peak RSS (MiB) | Object (KiB) | Time / baseline |',
             '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
    for (target, label), group in sorted(groups.items()):
        seconds = [r['seconds'] for r in group]
        cv = statistics.pstdev(seconds) / statistics.mean(seconds) * 100
        baseline = statistics.median(r['seconds'] for r in groups[target, 'baseline'])
        lines.append('| {} | {} | {:.3f} | {:.1f}% | {:.1f} | {:.1f} | {:.2f}x |'.format(
            target, label, statistics.median(seconds), cv,
            statistics.median(r['peak_rss_kib'] for r in group) / 1024,
            statistics.median(r['object_bytes'] for r in group) / 1024,
            statistics.median(seconds) / baseline))
    result = '\n'.join(lines) + '\n'
    (output / 'summary.md').write_text(result)
    print(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, help='Git ref or source directory')
    parser.add_argument('--current', default='WORKTREE', help='Git ref, directory, or WORKTREE')
    parser.add_argument('--output', required=True, type=Path, help='New result directory')
    parser.add_argument('--package', required=True, type=Path, help='Fixed installed tool prefix')
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--std', default='20')
    parser.add_argument('--flags', default='')
    parser.add_argument('--build-type', default='Release')
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--project', type=Path,
                        default=Path(__file__).resolve().parent / 'record_shape_compile_cost')
    parser.add_argument('--targets', nargs='+', default=['flat_20_read', 'flat_160_read',
                                                       'flat_20_write', 'flat_160_write'])
    parser.add_argument('--cmake-arg', action='append', default=[])
    args = parser.parse_args()
    if args.repetitions < 2:
        parser.error('Use at least two repetitions to expose measurement noise')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    repository = Path(__file__).resolve().parents[2]
    project = output / 'workload'
    shutil.copytree(str(args.project), str(project))
    shutil.copy2(__file__, str(output / 'compare.py'))
    metadata = vars(args).copy()
    import resource
    metadata.update(platform=os.uname(), compiler_version=subprocess.check_output(
        [args.compiler, '--version']).decode(),
        stack_limit_bytes=resource.getrlimit(resource.RLIMIT_STACK), refs={})
    builds = {}
    for label, ref in [('baseline', args.baseline), ('current', args.current)]:
        source = Path(ref)
        if source.is_dir():
            source = source.resolve()
            metadata['refs'][label] = str(source)
        else:
            source = output / 'sources' / label
            metadata['refs'][label] = snapshot(repository, ref, source, output)
        build = output / label
        builds[label] = build
        command = ['cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja',
                   '-DCMAKE_PREFIX_PATH=' + str(args.package.resolve()),
                   '-DCMAKE_CXX_COMPILER=' + args.compiler,
                   '-DCMAKE_CXX_STANDARD=' + args.std,
                   '-DCMAKE_CXX_FLAGS=' + args.flags,
                   '-DCMAKE_BUILD_TYPE=' + args.build_type,
                   '-DOMNI_PERF_SOURCE_DIR=' + str(source),
                   '-DCMAKE_CXX_COMPILER_LAUNCHER=' + ';'.join(
                       [sys.executable, str(output / 'compare.py'), '--launcher',
                        str(output / 'raw'), label]), *args.cmake_arg]
        print('Configuring ' + label, flush=True)
        run(command, output / (label + '-configure.log'))
    (output / 'environment.json').write_text(json.dumps(metadata, default=str, indent=2) + '\n')
    # Alternate A/B order; rebuilding only each measured object's output retains
    # the generated reflection header and dependencies between repetitions.
    for target in args.targets:
        for label, build in builds.items():
            print('Warming {} {}'.format(target, label), flush=True)
            run(['cmake', '--build', str(build), '--target', target,
                 '--parallel', '1'], output / (target + '-' + label + '-warmup.log'),
                env=dict(os.environ, OMNI_PERF_SAMPLE='-1'))
        for repetition in range(args.repetitions):
            for label in (['baseline', 'current'] if repetition % 2 == 0
                          else ['current', 'baseline']):
                build = builds[label]
                for path in build.glob('**/CMakeFiles/' + target + '.dir/**/*.o'):
                    path.unlink()
                print('Compiling {} {} {}/{}'.format(target, label, repetition + 1,
                                                     args.repetitions), flush=True)
                run(['cmake', '--build', str(build), '--target', target,
                     '--parallel', '1'],
                    output / ('{}-{}-{}.log'.format(target, label, repetition)),
                    env=dict(os.environ, OMNI_PERF_SAMPLE=str(repetition)))
    summary(output, args.targets, args.repetitions)


if __name__ == '__main__':
    sys.exit(launcher(sys.argv[2:]) if len(sys.argv) > 1 and sys.argv[1] == '--launcher'
             else main())
