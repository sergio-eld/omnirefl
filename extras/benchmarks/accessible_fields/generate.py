"""Write one translation unit per record shape.

Every unit declares a record with the same number of int leaf members and makes one Omnirefl
JSON call. Shapes differ only in how the members are arranged:

  flat      all members in one record
  nested    members split evenly over a chain of record-typed members (longer member paths)
  bases     members split evenly over a chain of base classes (no longer paths)
  methods   flat, plus public member functions that the mapping never uses
"""
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)

LEAVES = (20, 40, 80)
DEPTHS = (2, 4)
PRELUDE = '#include <string>\n\n#include <omnirefl/serialization/ryml.hpp>\n\n'
WRITE = 'std::string run(const root& value) { return omni::ryml::as_json(value); }\n'
READ = '''bool run(const std::string& json, root* out)
{
    auto result = omni::ryml::deserialize(omni::type_t<root>{}, json);
    if (!result.value)
        return false;
    *out = *result.value;
    return true;
}
'''


def members(first, count):
    return ''.join(f'    int m{index}{{}};\n' for index in range(first, first + count))


def flat(leaves, methods=0):
    functions = ''.join(
        f'    int method{index}(int value) const {{ return value + {index}; }}\n'
        for index in range(methods))
    return f'struct root\n{{\n{members(0, leaves)}{functions}}};\n\n'


def nested(leaves, depth):
    per_level = leaves // depth
    text = ''
    for level in range(depth - 1, -1, -1):
        name = 'root' if level == 0 else f'level{level}'
        child = f'    level{level + 1} child;\n' if level < depth - 1 else ''
        text += f'struct {name}\n{{\n{members(level * per_level, per_level)}{child}}};\n\n'
    return text


def bases(leaves, depth):
    per_level = leaves // depth
    text = ''
    for level in range(depth - 1, -1, -1):
        name = 'root' if level == 0 else f'level{level}'
        base = f': level{level + 1}' if level < depth - 1 else ''
        text += f'struct {name}{base}\n{{\n{members(level * per_level, per_level)}}};\n\n'
    return text


def settings(optionals=True, split=True, doubles=True, records=True):
    """A 30-member settings record shaped like a typical request type: three levels of
    inheritance, enums, optionals, strings, doubles and two small record members."""
    def opt(kind):
        return f'std::optional<{kind}>' if optionals else kind
    real = 'double' if doubles else 'int'
    resolution = 'resolution_data' if records else 'int'
    aspect = 'aspect_data' if records else 'int'
    rect = 'rect_data' if records else 'int'
    media = f'''    stream_index stream{{}};
    {opt('long long')} positionMs{{}};
    {opt(resolution)} resolution{{}};
    {opt(resolution)} resolutionWhenTranscoding{{}};
    std::string rotation;
    {aspect} aspectRatio{{}};
    {opt('bool')} dewarping{{}};
    {real} dewarpingXangle{{}};
    {real} dewarpingYangle{{}};
    {real} dewarpingFov{{}};
    int dewarpingPanofactor{{}};
    {opt(rect)} zoom{{}};
    bool panoramic{{}};
    {opt('std::string')} videoCodec{{}};
'''
    stream = f'''    format streamFormat{{}};
    quality streamQuality{{}};
    {opt('long long')} endPositionMs{{}};
    {opt('int')} dropLateFrames{{}};
    bool standFrameDuration{{}};
    bool realTimeOptimization{{}};
    bool audioOnly{{}};
    bool accurateSeek{{}};
    {opt('long long')} durationMs{{}};
    bool signature{{}};
    bool utcTimestamps{{}};
    bool continuousTimestamps{{}};
    bool download{{}};
'''
    head = '''#include <optional>

enum class stream_index { undefined, primary, secondary };
enum class format { webm, mpegts, mpjpeg, mp4, mkv };
enum class quality { lowest, low, normal, high, highest };
struct resolution_data { int width{}; int height{}; };
struct aspect_data { int width{}; int height{}; bool isAuto{}; };
struct rect_data { double x{}; double y{}; double width{}; double height{}; };

'''
    if split:
        return (head + f'struct media_settings\n{{\n{media}}};\n\n'
            f'struct stream_settings: media_settings\n{{\n{stream}}};\n\n'
            'struct root: stream_settings\n{\n    std::string id;\n};\n\n')
    return head + f'struct root\n{{\n{media}{stream}    std::string id;\n}};\n\n'


cases = [
    ('settings_28', settings()),
    ('settingsflat_28', settings(split=False)),
    ('settingsnoopt_28', settings(optionals=False)),
    ('settingsnoreal_28', settings(doubles=False)),
    ('settingsnorec_28', settings(records=False)),
]
for leaves in LEAVES:
    cases.append((f'flat_{leaves}', flat(leaves)))
    for depth in DEPTHS:
        cases.append((f'nested{depth}_{leaves}', nested(leaves, depth)))
        cases.append((f'bases{depth}_{leaves}', bases(leaves, depth)))
cases.append(('methods_40', flat(40, methods=40)))

for name, body in cases:
    for operation, call in (('write', WRITE), ('read', READ)):
        (out / f'{name}_{operation}.cpp').write_text(PRELUDE + body + call, newline='\n')
print(len(cases) * 2, 'sources written to', out)
