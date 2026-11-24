"""Reuse the record-shape fixtures and add executable correctness checks."""
from pathlib import Path
import re
import subprocess
import sys

output = Path(sys.argv[1])
subprocess.run([sys.executable, str(Path(__file__).with_name('generate.py')), str(output / 'original')], check=True)
for case in sys.argv[2:]:
    count = int(re.search(r'_(\d+)_(?:read|write)$', case).group(1))
    base_depth = re.match(r'bases(\d+)_', case)
    depth = int(base_depth.group(1)) if base_depth else 1
    source = (output / 'original' / f'{case}.cpp').read_text()
    if case.endswith('_read'):
        driver = '''
#include <iostream>
int main(int argc, char **) {
  std::string document{"{"};
  for (int i = 0; i < COUNT; ++i) {
    if (i) document += ',';
    document += "\\\"m" + std::to_string(i) + "\\\":" + std::to_string(i + argc);
  }
  document += '}';
  root value{};
  if (!run(document, &value)) return 1;
'''.replace('COUNT', str(count))
        for i in range(count):
            driver += f'  if (value.m{i} != {i} + argc) {{ std::cerr << "Incorrect m{i}\\n"; return 1; }}\n'
        driver += '}\n'
    else:
        driver = '\n#include <iostream>\nint main(int argc, char **) {\n  root value{};\n'
        driver += ''.join(f'  value.m{i} = {i} + argc;\n' for i in range(count))
        driver += '''  const auto parsed = omni::ryml::core::parse(run(value));
  if (!parsed) return 1;
  const auto object = parsed->crootref();
  if (object.num_children() != COUNT) return 1;
  for (int i = 0; i < COUNT; ++i) {
    const int chunk = COUNT / DEPTH;
    const int index = (DEPTH - 1 - i / chunk) * chunk + i % chunk;
    const auto key = "m" + std::to_string(index);
    const auto expected = std::to_string(index + argc);
    const auto child = object.child(i);
    if (child.key() != c4::to_csubstr(key) || child.val() != c4::to_csubstr(expected)) {
      std::cerr << "Incorrect field or order at " << key << '\\n';
      return 1;
    }
  }
}
'''.replace('COUNT', str(count)).replace('DEPTH', str(depth))
    for layout in 'ABC':
        (output / f'{case}.{layout}.cpp').write_text(source + driver)
