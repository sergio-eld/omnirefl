# TODO: [low] Source rewrites during instrumentation

Investigated 2026-11-13 in the assistant-owned Alpine container, using Clang 22
for instrumentation and GCC 15.2 for compilation. Rewriting a source file while
Omnirefl parses it can crash the parser. This is very low priority.

Observed results:

- Twelve instrumentation runs with the source unchanged passed, including
  concurrent C++11/C++20 pairs. Both serialization suites subsequently passed.
- Repeatedly truncating and rewriting a disposable source copy during parsing
  caused SIGBUS in two of three runs; the unchanged-copy baseline passed.
- The original build reported invalid UTF-8 in Clang's synthetic `<built-in>`
  buffer, followed by SIGSEGV. That exact sequence was not reproduced. The build
  overlapped with an edit, but the connection remains a hypothesis.
- `pipeline::to_parsed_ast` currently creates ASTUnit with
  `UserFilesAreVolatile=false`. Investigate Clang's source-buffer handling if
  this is revisited; no tool change was made based on the current evidence.

Reproduction requires the existing configured `/build`, its compilation
database, and the serialization tests in `/src`. The script only rewrites its
`/tmp` fixture; it leaves the repository source unchanged. Run from WSL:

```sh
docker exec -i omnirefl-codex-alpine python3 - <<'PY'
import pathlib
import shlex
import subprocess
import threading
import time

source = '/src/extras/extensions/serialization/tests/to_record.cpp'
fixture = pathlib.Path('/tmp/to-record-file-race.cpp')
contents = pathlib.Path(source).read_bytes()
command = shlex.split(subprocess.check_output([
    '/build/ccdb_query', '/build/compile_commands.json', source,
    '/cpp_11.extensions.serialization.dir/'], text=True))
command = [str(fixture) if arg == source else arg for arg in command]
args = ['/build/omnirefl', '--resource-dir',
        '/usr/local/llvm/install-linux/lib/clang/22', '--log-level', 'info',
        '-o', '/tmp/to-record-file-race.omnirefl.hpp', '-c', str(fixture),
        '--', *command]
for trial in range(4):
    fixture.write_bytes(contents)
    done = threading.Event()
    def rewrite():
        time.sleep(0.2)
        while not done.is_set():
            fixture.write_bytes(contents)
            time.sleep(0.003)
    writer = threading.Thread(target=rewrite) if trial else None
    if writer:
        writer.start()
    try:
        result = subprocess.run(args, cwd='/build', capture_output=True,
                                timeout=30)
    finally:
        done.set()
        if writer:
            writer.join()
    pathlib.Path(f'/tmp/to-record-file-race-{trial}.log').write_bytes(
        result.stdout + result.stderr)
    print(f'trial={trial} edits={bool(writer)} exit={result.returncode}', flush=True)
PY
```

Trial 0 is the unchanged baseline. Negative exit code `-7` denotes SIGBUS on
this Linux environment; the race is nondeterministic. Logs are saved inside
the container as `/tmp/to-record-file-race-<trial>.log`.
