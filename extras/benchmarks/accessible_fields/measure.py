"""Compiler launcher: record wall time, CPU time and Linux peak RSS per invocation."""
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

stats, label = Path(sys.argv[1]), sys.argv[2]
started = time.perf_counter()
result = subprocess.run(sys.argv[3:])
usage = resource.getrusage(resource.RUSAGE_CHILDREN)
stats.mkdir(parents=True, exist_ok=True)
(stats / f'{label}-{time.time_ns()}.json').write_text(json.dumps({
    'target': label,
    'seconds': time.perf_counter() - started,
    'user_seconds': usage.ru_utime,
    'system_seconds': usage.ru_stime,
    'peak_rss_kib': usage.ru_maxrss,
    'exit': result.returncode,
}))
sys.exit(result.returncode)
