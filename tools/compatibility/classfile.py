"""Read class signatures without loading game code or requiring its libraries."""
from __future__ import annotations

import json
import re
import struct
import sys
import zipfile
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, count):
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def number(self, fmt):
        return struct.unpack(fmt, self.read(struct.calcsize(fmt)))[0]

    def u1(self): return self.number('>B')
    def u2(self): return self.number('>H')
    def u4(self): return self.number('>I')


def parse_class(data):
    r = Reader(data)
    if r.u4() != 0xCAFEBABE:
        raise ValueError('not a class file')
    minor, major, count = r.u2(), r.u2(), r.u2()
    pool = [None] * count
    i = 1
    while i < count:
        tag = r.u1()
        if tag == 1:
            pool[i] = r.read(r.u2()).decode('utf-8', errors='replace')
        elif tag in (7, 8, 16, 19, 20):
            pool[i] = r.u2()
        elif tag in (3, 4, 9, 10, 11, 12, 17, 18):
            r.read(4)
        elif tag in (5, 6):
            r.read(8)
            i += 1
        elif tag == 15:
            r.read(3)
        else:
            raise ValueError(f'unknown constant pool tag {tag}')
        i += 1
    flags, this, parent = r.u2(), r.u2(), r.u2()
    name = lambda index: pool[pool[index]] if index else None
    interfaces = [name(r.u2()) for _ in range(r.u2())]

    def members():
        result = []
        for _ in range(r.u2()):
            flags, member_name, descriptor = r.u2(), pool[r.u2()], pool[r.u2()]
            for _ in range(r.u2()):
                r.u2()
                r.read(r.u4())
            result.append({'name': member_name, 'descriptor': descriptor, 'flags': flags})
        return result

    fields, methods = members(), members()
    return {'name': name(this), 'parent': name(parent), 'interfaces': interfaces,
            'major': major, 'flags': flags, 'fields': fields, 'methods': methods}


def type_descriptor(name):
    primitives = {'void': 'V', 'boolean': 'Z', 'byte': 'B', 'char': 'C',
                  'short': 'S', 'int': 'I', 'long': 'J', 'float': 'F', 'double': 'D'}
    if name.endswith('[]'):
        return '[' + type_descriptor(name[:-2])
    return primitives.get(name, 'L' + name.replace('.', '/') + ';')


def read_proguard(path):
    classes, owner = {}, None
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        match = re.match(r'^(\S+) -> (\S+):$', line)
        if match:
            owner = match[1].replace('.', '/')
            classes[owner] = {'obf': match[2].replace('.', '/'), 'fields': {}, 'methods': {}}
            continue
        if not owner or not line.startswith('    '):
            continue
        match = re.match(r'\s+(?:\d+:\d+:)?(\S+) (\S+)\((.*?)\)(?::\d+(?::\d+)?)? -> (\S+)$', line)
        if match:
            args = ''.join(type_descriptor(arg) for arg in match[3].split(',') if arg)
            desc = '(' + args + ')' + type_descriptor(match[1])
            classes[owner]['methods'][(match[2], desc)] = match[4]
        else:
            match = re.match(r'\s+(\S+) (\S+) -> (\S+)$', line)
            if match:
                classes[owner]['fields'][(match[2], type_descriptor(match[1]))] = match[3]
    return classes


class NamedJar:
    def __init__(self, cache, version):
        self.jar = zipfile.ZipFile(cache / (version + '.jar'))
        mapping = cache / (version + '-mappings.txt')
        self.mapping = read_proguard(mapping) if mapping.exists() else {}
        self.reverse = {entry['obf']: name for name, entry in self.mapping.items()}
        self.cache = {}

    def normalize(self, desc):
        return re.sub(r'L([^;]+);', lambda m: 'L' + self.reverse.get(m[1], m[1]) + ';', desc)

    def load(self, name):
        if name in self.cache:
            return self.cache[name]
        obf = self.mapping.get(name, {}).get('obf', name)
        try:
            data = parse_class(self.jar.read(obf + '.class'))
        except KeyError:
            return None
        data['name'] = name
        data['parent'] = self.reverse.get(data['parent'], data['parent'])
        data['interfaces'] = [self.reverse.get(i, i) for i in data['interfaces']]
        for kind in ('fields', 'methods'):
            names = {(obf_name, desc): official for (official, desc), obf_name
                     in self.mapping.get(name, {}).get(kind, {}).items()}
            for member in data[kind]:
                member['raw_name'] = member['name']
                member['raw_descriptor'] = member['descriptor']
                member['descriptor'] = self.normalize(member['descriptor'])
                member['name'] = names.get((member['name'], member['descriptor']), member['name'])
        self.cache[name] = data
        return data

    def members(self, owner, kind, seen=None):
        seen = set() if seen is None else seen
        if owner in seen:
            return []
        seen.add(owner)
        data = self.load(owner)
        if not data:
            return []
        result = [(owner, member) for member in data[kind]]
        for parent in [data['parent']] + data['interfaces']:
            if parent:
                result.extend(self.members(parent, kind, seen))
        return result


if __name__ == '__main__':
    cache, version, *names = sys.argv[1:]
    jar = NamedJar(Path(cache), version)
    for name in names:
        data = jar.load(name.replace('.', '/'))
        print(json.dumps(data, indent=2))
