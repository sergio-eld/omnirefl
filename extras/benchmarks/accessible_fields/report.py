"""Collect repeated compiler measurements and executable/object sizes.

Usage: report.py <configured-build> <output.json>
The stripped copy and .text dump leave the original executable untouched.
"""
import collections
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

build = Path(sys.argv[1])
groups = collections.defaultdict(list)
for path in (build / 'stats').glob('*.json'):
    sample = json.loads(path.read_text())
    if sample['exit']:
        raise SystemExit(f'Failed compilation: {path}')
    groups[sample['target']].append(sample)

sizes = build / 'binary-sizes'
sizes.mkdir(exist_ok=True)
rows = []
for target, samples in sorted(groups.items()):
    binary = build / target
    stripped = sizes / target
    text = sizes / f'{target}.text'
    subprocess.run(['objcopy', '--strip-all', '--dump-section', f'.text={text}',
                    str(binary), str(stripped)], check=True)
    sections = {}
    for line in subprocess.check_output(['size', '-A', str(binary)], text=True).splitlines():
        cells = line.split()
        if len(cells) == 3 and cells[0].startswith('.') and cells[1].isdigit():
            sections[cells[0]] = int(cells[1])
    layout, case = target.split('_', 1)
    metadata = json.loads((build / f'{case}.C.hpp.json').read_text())
    wall = [s['seconds'] for s in samples]
    cpu = [s['user_seconds'] + s['system_seconds'] for s in samples]
    obj = build / 'CMakeFiles' / f'{target}.dir' / 'sources' / f'{case}.{layout}.cpp.o'
    rows.append({
        'case': case,
        'layout': layout,
        'samples': len(samples),
        'wall_seconds': statistics.median(wall),
        'wall_cv_percent': statistics.stdev(wall) / statistics.mean(wall) * 100 if len(wall) > 1 else None,
        'cpu_seconds': statistics.median(cpu),
        'cpu_cv_percent': statistics.stdev(cpu) / statistics.mean(cpu) * 100 if len(cpu) > 1 else None,
        'peak_rss_mib': statistics.median(s['peak_rss_kib'] for s in samples) / 1024,
        'header_bytes': metadata['header_bytes'][layout],
        'object_bytes': obj.stat().st_size,
        'binary_bytes': binary.stat().st_size,
        'stripped_bytes': stripped.stat().st_size,
        'text_bytes': sum(v for k, v in sections.items() if k.startswith('.text')),
        'rodata_bytes': sum(v for k, v in sections.items() if k.startswith('.rodata')),
        'debug_bytes': sum(v for k, v in sections.items() if k.startswith('.debug')),
        'text_sha256': hashlib.sha256(text.read_bytes()).hexdigest(),
    })
Path(sys.argv[2]).write_text(json.dumps(rows, indent=2))
print('case,layout,samples,wall_s,wall_cv,cpu_s,cpu_cv,rss_MiB,header_KiB,stripped_KiB,text_KiB,debug_KiB')
for row in sorted(rows, key=lambda r: (r['case'], r['layout'])):
    print(','.join(str(x) for x in [
        row['case'], row['layout'], row['samples'],
        round(row['wall_seconds'], 3), round(row['wall_cv_percent'] or 0, 2),
        round(row['cpu_seconds'], 3), round(row['cpu_cv_percent'] or 0, 2),
        round(row['peak_rss_mib'], 1), round(row['header_bytes'] / 1024, 1),
        round(row['stripped_bytes'] / 1024, 1), round(row['text_bytes'] / 1024, 1),
        round(row['debug_bytes'] / 1024, 1),
    ]))
