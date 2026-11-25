#!/usr/bin/env python3
"""Opt-in keep-going regression using an installed Omnirefl package.

Run inside a disposable toolchain container (not part of the full test suite):
  python3 check.py --package /path/to/package --root /tmp/keep-going --generator Ninja
  python3 check.py --package /path/to/package --root /tmp/keep-going-make \
      --generator 'Unix Makefiles'

Uses real reflection and compilation. Failed sources are retried by ordinary
builds; successful reflection remains cached. Explicit .omni.force requests force
all sources. Ninja can repeat reflection in the first ordinary build after
.omni.force to refresh its build and dependency records.
"""

import argparse
from pathlib import Path
import os
import re
import shutil
import subprocess


def build(root, targets, flags):
    result = subprocess.run(
        ['cmake', '--build', str(root), '--target', *targets, '--', *flags],
        env={**os.environ, 'CMAKE_BUILD_PARALLEL_LEVEL': '1'},
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return result.returncode, result.stdout


def source(name, invalid=False, extra=False):
    return (f'#error invalid_{name}\n' if invalid else '') + f'''#include <omnirefl/reflected_scope.hpp>
#include <tuple>
namespace {{
struct model_{name} {{
  int value;
''' + ('#include "extra_fields.hpp"\n' if extra else '') + f'''}};
struct field_count {{
  template <typename Meta>
  int operator()(omni::record_meta_t<Meta> m) const {{
    return std::tuple_size<decltype(m.accessible_data_members())>::value;
  }}
}};
}}
int {name}() {{
  return omni::reflected_call(field_count{{}}, omni::type_t<model_{name}>{{}});
}}
'''


def headers(root):
    return {path: path.stat().st_mtime_ns
            for path in root.glob('omni_*/*.omnirefl.hpp')}


def main(args):
    root = args.root.resolve()
    if root.exists():
        raise RuntimeError(f'Refusing to replace existing test directory: {root}')
    inputs = root / 'source'
    inputs.mkdir(parents=True)
    (inputs / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(omnirefl_keep_going LANGUAGES CXX)
find_package(omnirefl CONFIG REQUIRED)
add_executable(example main.cpp alpha.cpp beta.cpp gamma.cpp valid.cpp)
set_target_properties(example PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
omni_reflected_target(example LOG_LEVEL warning)
add_executable(other other_main.cpp delta.cpp)
set_target_properties(other PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
omni_reflected_target(other LOG_LEVEL warning)
''')
    (inputs / 'main.cpp').write_text('''int alpha();
int beta();
int gamma();
int valid();
int main() { return alpha() + beta() + gamma() + valid() == 4 ? 0 : 1; }
''')
    (inputs / 'other_main.cpp').write_text('''int delta();
int main() { return delta() == 1 ? 0 : 1; }
''')
    for name in ('alpha', 'beta', 'gamma', 'delta', 'valid'):
        (inputs / (name + '.cpp')).write_text(source(name, invalid=name != 'valid'))
    output = root / 'build'
    subprocess.run(['cmake', '-S', str(inputs), '-B', str(output),
                    '-G', args.generator, '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                    '-DCMAKE_PREFIX_PATH=' + str(args.package.resolve())], check=True)
    flags = ['-j1', '-k0'] if args.generator == 'Ninja' else ['-j1', '-k']

    status, log = build(output, ['example.omni', 'other.omni'], flags)
    (root / 'failed.log').write_text(log)
    if status == 0:
        raise RuntimeError('Invalid sources unexpectedly built successfully')
    errors = set(re.findall(r'error: invalid_(\w+)', log))
    if errors != {'alpha', 'beta', 'gamma', 'delta'}:
        raise RuntimeError(f'Keep-going did not report every invalid source: {errors}\n{log}')
    successful = {path: timestamp for path, timestamp in headers(output).items()
                  if path.name.startswith(('main_', 'valid_', 'other_main_'))}
    if len(successful) != 3:
        raise RuntimeError(f'Expected all three valid-source outputs: {successful}\n{log}')
    for path in successful:
        if 'Placeholder generated' in path.read_text():
            raise RuntimeError(f'Valid source did not finish reflection: {path}')

    (inputs / 'alpha.cpp').write_text(source('alpha'))
    status, log = build(output, ['example', 'other'], flags)
    (root / 'partly-fixed.log').write_text(log)
    if status == 0:
        raise RuntimeError('Remaining invalid sources unexpectedly built successfully')
    errors = set(re.findall(r'error: invalid_(\w+)', log))
    if errors != {'beta', 'gamma', 'delta'}:
        raise RuntimeError(f'Partial repair did not report remaining failures: {errors}\n{log}')
    for path, timestamp in successful.items():
        if path.stat().st_mtime_ns != timestamp:
            raise RuntimeError(f'Partial repair regenerated valid reflection: {path}\n{log}')
    for name in ('beta', 'gamma', 'delta'):
        (inputs / (name + '.cpp')).write_text(source(name))
    status, log = build(output, ['example', 'other'], flags)
    (root / 'fixed.log').write_text(log)
    if status:
        raise RuntimeError(f'Fixed sources failed to build:\n{log}')
    for path, timestamp in successful.items():
        if path.stat().st_mtime_ns != timestamp:
            raise RuntimeError(f'Rebuilding fixed sources regenerated valid reflection: {path}\n{log}')
    subprocess.run([str(output / 'example')], check=True)
    subprocess.run([str(output / 'other')], check=True)

    (inputs / 'extra_fields.hpp').write_text('  int added;\n')
    (inputs / 'valid.cpp').write_text(source('valid', extra=True))
    (inputs / 'main.cpp').write_text((inputs / 'main.cpp').read_text().replace('== 4', '== 5'))
    before_force = headers(output)
    status, log = build(output, ['example.omni.force'], flags)
    (root / 'forced.log').write_text(log)
    if status:
        raise RuntimeError(f'Forced regeneration failed:\n{log}')
    forced = {path: timestamp for path, timestamp in headers(output).items()
              if path.parent.name == 'omni_example'}
    for path, timestamp in forced.items():
        if timestamp == before_force[path]:
            raise RuntimeError(f'Explicit .omni.force did not force regeneration: {path}\n{log}')
    status, log = build(output, ['example'], flags)
    (root / 'after-force.log').write_text(log)
    if status:
        raise RuntimeError(f'Build after forced regeneration failed:\n{log}')
    after_force = headers(output)
    subprocess.run([str(output / 'example')], check=True)

    (inputs / 'extra_fields.hpp').write_text('  int added;\n  int another;\n')
    (inputs / 'main.cpp').write_text((inputs / 'main.cpp').read_text().replace('== 5', '== 6'))
    status, log = build(output, ['example'], flags)
    (root / 'header-change.log').write_text(log)
    if status:
        raise RuntimeError(f'Build after new header changed failed:\n{log}')
    subprocess.run([str(output / 'example')], check=True)
    for path, timestamp in after_force.items():
        if not path.name.startswith(('main_', 'valid_')):
            if path.stat().st_mtime_ns != timestamp:
                raise RuntimeError(f'Header change regenerated unrelated reflection: {path}\n{log}')
    print(f'{args.generator}: keep-going, repair caching, forced regeneration, '
          'and header dependencies passed')
    shutil.rmtree(root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--generator', choices=('Ninja', 'Unix Makefiles'),
                        default='Ninja')
    main(parser.parse_args())
