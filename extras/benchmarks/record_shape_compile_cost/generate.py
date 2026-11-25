"""Generate identical public read/write calls for flat records of increasing width.

Used only by the standalone non-CI comparison project. Each object instantiates
one mapping direction, separating reader and writer costs from tool execution.
"""
from pathlib import Path
import sys

output = Path(sys.argv[1])
for width in (20, 80, 160):
    fields = ''.join('  int m{}{{}};\n'.format(i) for i in range(width))
    record = ('#include <omnirefl/serialization/ryml.hpp>\n\n'
              'struct record {\n' + fields + '};\n\n')
    calls = {
        'read': '''bool read(const std::string &document, record *into) {
  auto result = omni::ryml::deserialize(omni::type_t<record>{}, document);
  if (!result.value)
    return false;
  *into = std::move(*result.value);
  return true;
}
''',
        'write': '''std::string write(const record &value) {
  return omni::ryml::as_json(value);
}
''',
    }
    for direction, call in calls.items():
        path = output / 'flat_{}_{}.cpp'.format(width, direction)
        text = record + call
        if not path.exists() or path.read_text() != text:
            path.write_text(text)
