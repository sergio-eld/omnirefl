import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

parser=argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--rounds', type=int, default=3)
args=parser.parse_args()
build=args.build
if not (build/'CMakeCache.txt').exists():
    raise SystemExit('Configured build required')
commands=json.loads((build/'compile_commands.json').read_text())
objects={}
for item in commands:
    source=Path(item['file']).name
    for layout in 'ABC':
        suffix=f'.{layout}.cpp'
        if source.endswith(suffix):
            case=source[:-len(suffix)]
            target=f'{layout}_{case}'
            objects[target]=build/'CMakeFiles'/f'{target}.dir'/'sources'/f'{case}.{layout}.cpp.o'
small=[t for t in objects if '_20_' in t]
wide=[t for t in objects if t not in small]
logs=build/'experiment-logs'
logs.mkdir(exist_ok=True)

def batch(targets,jobs,label):
    if not targets:
        return
    started=time.monotonic()
    with (logs/f'{label}.log').open('w') as out:
        result=subprocess.run(['cmake','--build',str(build),'--target',*targets,'--parallel',str(jobs)],stdout=out,stderr=subprocess.STDOUT)
    print(label,'exit=',result.returncode,'wall=',round(time.monotonic()-started,1),flush=True)
    if result.returncode:
        print((logs/f'{label}.log').read_text()[-6000:],flush=True)
        raise SystemExit(result.returncode)
    for target in targets:
        subprocess.run([str(build/target)],check=True)

batch(small,14,'warmup-small')
batch(wide,3,'warmup-wide')
if (build/'warmup-stats').exists():
    shutil.rmtree(build/'warmup-stats')
shutil.move(build/'stats',build/'warmup-stats')
for iteration in range(args.rounds):
    for targets,jobs,label in [(small,14,'small'),(wide,3,'wide')]:
        for target in targets:
            objects[target].unlink(missing_ok=True)
        batch(targets,jobs,f'round-{iteration+1}-{label}')
