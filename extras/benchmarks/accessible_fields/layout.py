"""Prototype emitted accessible-field layouts for the compile-cost experiment.

A is the tool's original output. B copies accessible field definitions into
individual record metas; C emits flat tuples referring to existing field tags.
The controlled fixtures use concrete records and unambiguous public base chains.
This is a generated-header experiment, not a replacement for Clang name lookup.
"""
import argparse
import json
from pathlib import Path
import re

RECORD = re.compile(r'^struct _reflected<(?:struct|class|union) (.+?), T> \{\n(.*?)^\};', re.M | re.S)
FIELDS = re.compile(r'^  struct fields \{\n(.*?)^  \};', re.M | re.S)
FIELD = re.compile(r'^    struct (\w+)_t \{\n.*?^    \};', re.M | re.S)
OWN = re.compile(r'  using own_public_fields_t =\s*std::tuple<(.*?)>;', re.S)
BASES = re.compile(r'  using public_bases_t = std::tuple<(.*?)>;', re.S)


def emit(source, copied, referenced):
    original = Path(source).read_text()
    records = {}
    for match in RECORD.finditer(original):
        name, body = match.groups()
        own = OWN.search(body)
        if not own:
            continue
        fields = FIELDS.search(body)
        definitions = {f.group(1): f.group(0) for f in FIELD.finditer(fields.group(1))} if fields else {}
        names = re.findall(r'typename fields::(\w+)_t', own.group(1))
        if set(names) != set(definitions):
            raise ValueError(f'Unexpected generated field definitions for {name}')
        bases = [b.strip() for b in BASES.search(body).group(1).split(',') if b.strip()]
        records[name] = dict(body=body, definitions=definitions, names=names, bases=bases)

    def collect(name):
        record = records[name]
        inherited = [field for base in record['bases'] for field in collect(base)]
        # The benchmark generator uses distinct member names. Reject shapes
        # needing hiding/ambiguity resolution rather than silently changing it.
        selected = inherited + [(name, field) for field in record['names']]
        if len({field for _, field in selected}) != len(selected):
            raise ValueError(f'{name}: prototype needs unambiguous, distinct field names')
        return selected

    def replace(match, duplicate):
        name, body = match.groups()
        if name not in records:
            return match.group(0)
        selected = collect(name)
        if duplicate and selected:
            definitions = '\n\n'.join(records[owner]['definitions'][field] for owner, field in selected)
            block = '  struct fields {\n' + definitions + '\n  };'
            fields = FIELDS.search(body)
            if fields:
                body = body[:fields.start()] + block + body[fields.end():]
            else:
                own = OWN.search(body)
                body = body[:own.start()] + block + '\n\n' + body[own.start():]
        elements = []
        for owner, field in selected:
            tag = f'typename fields::{field}_t'
            if not duplicate and owner != name:
                # Reuse the generated dependent-name helper: base metas may
                # be defined later and source records are still incomplete.
                tag = f'typename _meta<typename _wrt<{owner}, T>::type>::fields::{field}_t'
            elements.append(f'omni::data_member_meta_t<{tag}>')
        flat = '\n\n  using accessible_public_fields_t = std::tuple<' + ',\n    '.join(elements) + '>;'
        own = OWN.search(body)
        body = body[:own.end()] + flat + body[own.end():]
        return match.group(0).replace(match.group(2), body, 1)

    Path(copied).write_text(RECORD.sub(lambda m: replace(m, True), original))
    Path(referenced).write_text(RECORD.sub(lambda m: replace(m, False), original))
    Path(referenced + '.json').write_text(json.dumps({
        'header_bytes': {'A': len(original.encode()), 'B': Path(copied).stat().st_size, 'C': Path(referenced).stat().st_size},
        'fields': {name: collect(name) for name in records},
    }, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('copied')
    parser.add_argument('referenced')
    args = parser.parse_args()
    emit(args.source, args.copied, args.referenced)
