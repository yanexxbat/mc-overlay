"""Generate audited JNI profiles from official jars and mapping artifacts.

Inputs stay in the build cache. No Minecraft code is copied into this project.
Run: python tools/compatibility/generate_profiles.py build/compat-cache
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

from classfile import NamedJar
from geometry_schema import geometry_schema

ROOT = Path(__file__).resolve().parents[2]
VERSIONS = ('26.2', '1.21.11', '1.21.1', '1.20.1')


def schema():
    source = (ROOT / 'src/agent/mappings.hpp').read_text(encoding='utf-8')
    clean = re.sub(r'//[^\n]*', '', source)
    roles = re.findall(r'const char\*\s+(\w+);', clean.split('struct ClassNames {')[1].split('};')[0])
    names = []
    for declaration in re.findall(r'Member\s+([^;]+);', clean.split('struct MappingSet {')[1].split('};')[0]):
        names.extend(name.strip() for name in declaration.split(','))
    base = clean.split('inline constexpr MappingSet kMojang{')[1].split('inline constexpr MappingSet kYarn{')[0]
    classes = re.findall(r'"(net/[^";]+)"', base.split('},')[0])
    members = re.findall(r'\{"([^\"]*)",\s*"([^\"]*)"\}', base)
    if len(classes) != len(roles) or len(members) != len(names):
        raise ValueError(f'schema mismatch: {len(classes)}/{len(roles)}, {len(members)}/{len(names)}')
    return dict(zip(roles, classes)), dict(zip(names, members))


def owners(classes):
    result = {}
    def add(role, names):
        for name in names.split(): result[name] = classes[role]
    add('client', 'methodGetInstance fieldPlayer fieldLevel fieldGameRenderer fieldFps fieldOptions methodCurrentServer fieldTargetedEntity fieldInteractionMgr fieldCurrentScreen')
    add('gameRenderer', 'methodGetCamera methodRealFov')
    add('camera', 'methodCameraPosition fieldCameraPosition methodGetYaw methodGetPitch')
    add('entity', 'methodGetX methodGetY methodGetZ methodGetId methodGetName methodGetBbWidth methodGetBbHeight methodEntityYaw')
    add('livingEntity', 'methodGetHealth methodGetMaxHealth methodGetAbsorption methodGetArmor methodGetEquipped methodGetStatusEffects')
    add('clientLevel', 'methodEntitiesForRendering methodDimension methodGetBlockState')
    add('component', 'methodComponentToString')
    add('vec3', 'fieldVecX fieldVecY fieldVecZ')
    add('options', 'methodFov fieldBobView')
    add('optionValue', 'methodOptionGet')
    add('serverData', 'fieldServerAddress')
    add('equipSlot', 'slotHead slotChest slotLegs slotFeet slotMainHand slotOffHand')
    add('itemStack', 'methodItemIsEmpty methodItemGetDamage methodItemGetMaxDamage methodItemIsDamageable methodItemGetCount methodItemGetName methodStackGetItem')
    add('effectInst', 'methodEffectAmplifier methodEffectDuration methodEffectTranslationKey methodEffectIsAmbient')
    add('blockState', 'methodStateIsAir methodStateGetBlock')
    add('block', 'methodBlockTransKey')
    add('blockPosMut', 'methodMutableSet')
    add('player', 'fieldScreenHandler fieldInventory')
    add('playerInv', 'methodInvGetStack methodInvSize')
    add('item', 'methodItemTransKey')
    add('interactionMgr', 'methodClickSlot')
    add('slotAction', 'fieldSwapAction')
    result['fieldSyncId'] = 'net/minecraft/world/inventory/AbstractContainerMenu'
    return result


def namespace_map(cache, version, namespace):
    classes, fields, methods = {}, {}, {}
    if namespace == 'intermediary':
        with zipfile.ZipFile(cache / f'{version}-intermediary.jar') as jar:
            text = jar.read('mappings/mappings.tiny').decode()
        owner = None
        for line in text.splitlines()[1:]:
            parts = line.split('\t')
            if parts[0] == 'c':
                owner = parts[1]
                classes[owner] = parts[2]
            elif len(parts) >= 5 and parts[1] in ('f', 'm'):
                kind, desc, original, mapped = parts[1:5]
                (fields if kind == 'f' else methods)[(owner, original, desc)] = mapped
    elif namespace == 'forge-srg':
        with zipfile.ZipFile(cache / 'mcp_config-1.20.1.zip') as jar:
            text = jar.read('config/joined.tsrg').decode()
        owner = None
        for line in text.splitlines()[1:]:
            if not line.startswith('\t'):
                owner = line.split()[0]
            elif line.startswith('\t\t'):
                continue
            else:
                parts = line.strip().split()
                if len(parts) >= 3 and parts[1].startswith('('):
                    methods[(owner, parts[0], parts[1])] = parts[2]
                elif len(parts) >= 2:
                    fields[(owner, parts[0], None)] = parts[1]
    return classes, fields, methods


def namespace_method(jar, owner, raw_name, raw_desc, methods):
    """A virtual override inherits remapped names from its superclass/interfaces.

    Tiny/TSRG may only name the interface declaration, as with Camera.position
    in 1.21.11. Looking up only Camera's own entry silently leaves the raw name.
    Private/static methods cannot inherit a virtual name with the same signature.
    """
    seen = set()
    def visit(named, inherited=False):
        if named in seen: return set()
        seen.add(named)
        raw_owner = jar.mapping.get(named, {}).get('obf', named)
        data = jar.load(named)
        if not data: return set()
        declaration = next((m for m in data['methods']
                            if m['raw_name'] == raw_name and m['raw_descriptor'] == raw_desc), None)
        if inherited and declaration and declaration['flags'] & (2 | 8): return set()
        mapped = methods.get((raw_owner, raw_name, raw_desc))
        if mapped is not None: return {mapped}
        if declaration and declaration['flags'] & (2 | 8): return set()
        result = set()
        for parent in [data['parent']] + data['interfaces']:
            if parent: result.update(visit(parent, True))
        return result
    found = visit(owner)
    if len(found) > 1:
        raise ValueError(f'Conflicting virtual mappings for {owner}.{raw_name}{raw_desc}: {sorted(found)}')
    return next(iter(found), raw_name)


def generate(cache):
    inputs = json.loads(Path(__file__).with_name('mapping_inputs.json').read_text())
    for name, artifact in inputs.items():
        if hashlib.sha256((cache / name).read_bytes()).hexdigest() != artifact['sha256']:
            raise ValueError(f'{name}: namespace mapping checksum mismatch')
    class_spec, member_spec = schema()
    owner_spec = owners(class_spec)
    if owner_spec.keys() != member_spec.keys():
        raise ValueError(f'owner schema differs: {owner_spec.keys() ^ member_spec.keys()}')
    profiles, report = [], []
    for version in VERSIONS:
        metadata = json.loads((cache / f'{version}.json').read_text(encoding='utf-8-sig'))
        for kind, suffix in [('client', '.jar'), ('client_mappings', '-mappings.txt')]:
            if kind in metadata['downloads']:
                actual = hashlib.sha1((cache / (version + suffix)).read_bytes()).hexdigest()
                if actual != metadata['downloads'][kind]['sha1']:
                    raise ValueError(f'{version} {kind} checksum mismatch')
        jar = NamedJar(cache, version)
        classes, members, resolved = dict(class_spec), {}, {}
        source = 'CameraFloat' if version == '26.2' else ('RendererFloat' if version == '1.21.11' else 'RendererDouble')
        overrides = {}
        if version in ('26.2', '1.21.11'):
            overrides.update({'methodCameraPosition': ('position', '()Lnet/minecraft/world/phys/Vec3;'),
                              'methodGetYaw': ('yRot', '()F'), 'methodGetPitch': ('xRot', '()F')})
        if version == '26.2':
            classes['slotAction'] = 'net/minecraft/world/inventory/ContainerInput'
            overrides.update({'methodGetCamera': ('mainCamera', '()Lnet/minecraft/client/Camera;'),
                         'methodCameraPosition': ('position', '()Lnet/minecraft/world/phys/Vec3;'),
                         'methodGetYaw': ('yRot', '()F'), 'methodGetPitch': ('xRot', '()F'),
                         'methodRealFov': ('getFov', '()F'),
                         'methodClickSlot': ('handleContainerInput', '(IIILnet/minecraft/world/inventory/ContainerInput;Lnet/minecraft/world/entity/player/Player;)V'),
                         'fieldSwapAction': ('SWAP', 'Lnet/minecraft/world/inventory/ContainerInput;')})
        elif version in ('1.20.1', '1.21.1'):
            overrides['methodRealFov'] = ('getFov', '(Lnet/minecraft/client/Camera;FZ)D')
        missing = []
        owner_spec = owners(classes)
        if version == '26.2': owner_spec['fieldCurrentScreen'] = 'net/minecraft/client/gui/Gui'
        for key, original in member_spec.items():
            name, desc = overrides.get(key, original)
            owner = classes['camera'] if key == 'methodRealFov' and source == 'CameraFloat' else owner_spec[key]
            kind = 'methods' if key.startswith('method') else 'fields'
            found = [(declaring, m) for declaring, m in jar.members(owner, kind)
                     if m['name'] == name and m['descriptor'] == desc]
            if not found:
                missing.append((key, name, desc))
                members[key] = ('', '')
                continue
            declaring, member = found[0]
            static = key in ('methodGetInstance', 'fieldFps', 'fieldSwapAction') or key.startswith('slot')
            if bool(member['flags'] & 8) != static:
                raise ValueError(f'{version} {key}: wrong static/instance kind')
            members[key] = (name, desc)
            raw_owner = jar.mapping.get(declaring, {}).get('obf', declaring)
            resolved[key] = (raw_owner, member['raw_name'], member['raw_descriptor'], declaring)
        for role, name in list(classes.items()):
            if not jar.load(name):
                raise ValueError(f'{version}: class {role} ({name}) missing')
        if missing:
            raise ValueError(f'{version}: missing members: {missing}')

        # Runtime version comes from the game's own WorldVersion, never a title.
        version_owner = 'net/minecraft/SharedConstants'
        world_owner = 'net/minecraft/WorldVersion'
        current = next(m for _, m in jar.members(version_owner, 'methods') if m['name'] == 'getCurrentVersion')
        ident = next(m for _, m in jar.members(world_owner, 'methods') if m['name'] in ('getId', 'id'))
        gui_class = 'net/minecraft/client/gui/Gui' if version == '26.2' else ''
        gui = None
        if gui_class:
            gui = next(m for _, m in jar.members(classes['client'], 'fields')
                       if m['name'] == 'gui' and m['descriptor'] == 'L' + gui_class + ';')
        geometry_classes, humanoids, geometry_defs, partial_source = geometry_schema(version)
        geometry_resolved = {}
        for key, (owner, name, desc, kind, static) in geometry_defs.items():
            found = [(declaring, m) for declaring, m in jar.members(owner, kind)
                     if m['name'] == name and m['descriptor'] == desc]
            if not found: raise ValueError(f'{version}: geometry {key} {owner}.{name}{desc} missing')
            declaring, member = found[0]
            if bool(member['flags'] & 8) != static: raise ValueError(f'{version}: wrong geometry member kind: {key}')
            geometry_resolved[key] = (declaring, member)
        for name in list(geometry_classes.values()) + humanoids:
            if name and not jar.load(name): raise ValueError(f'{version}: geometry class {name} missing')
        namespaces = ['mojmap']
        if version != '26.2': namespaces += ['vanilla', 'intermediary']
        if version == '1.20.1': namespaces += ['forge-srg']
        for namespace in namespaces:
            ns_classes, ns_fields, ns_methods = namespace_map(cache, version, namespace)
            def class_name(named):
                raw = jar.mapping.get(named, {}).get('obf', named)
                if namespace == 'vanilla': return raw
                if namespace == 'intermediary': return ns_classes.get(raw, raw)
                return named
            def descriptor(desc):
                return re.sub(r'L([^;]+);', lambda m: 'L' + class_name(m[1]) + ';', desc)
            mapped = {}
            for key, (name, desc) in members.items():
                if not name:
                    mapped[key] = ('', '')
                    continue
                owner, raw_name, raw_desc, declaring = resolved[key]
                kind = 'methods' if key.startswith('method') else 'fields'
                if namespace == 'vanilla': name = raw_name
                elif namespace in ('intermediary', 'forge-srg'):
                    table = ns_methods if kind == 'methods' else ns_fields
                    lookup_desc = None if namespace == 'forge-srg' and kind == 'fields' else raw_desc
                    name = namespace_method(jar, declaring, raw_name, raw_desc, ns_methods) if kind == 'methods' else table.get((owner, raw_name, lookup_desc), raw_name)
                mapped[key] = (name, descriptor(desc))
            def version_member(named_owner, m):
                raw_owner = jar.mapping.get(named_owner, {}).get('obf', named_owner)
                name = m['name']
                if namespace == 'vanilla': name = m['raw_name']
                elif namespace in ('intermediary', 'forge-srg'):
                    name = namespace_method(jar, named_owner, m['raw_name'], m['raw_descriptor'], ns_methods)
                return name, descriptor(m['descriptor'])
            profile_id = f'{namespace}-{version}'
            geom_members, geom_owners = {}, {}
            for key, (declaring, member) in geometry_resolved.items():
                owner, name, desc, kind, static = geometry_defs[key]
                raw_owner = jar.mapping.get(declaring, {}).get('obf', declaring)
                if namespace == 'vanilla': name = member['raw_name']
                elif namespace in ('intermediary', 'forge-srg'):
                    table = ns_methods if kind == 'methods' else ns_fields
                    lookup_desc = None if namespace == 'forge-srg' and kind == 'fields' else member['raw_descriptor']
                    name = namespace_method(jar, declaring, member['raw_name'], member['raw_descriptor'], ns_methods) if kind == 'methods' else table.get((raw_owner, member['raw_name'], lookup_desc), member['raw_name'])
                geom_members[key] = [name, descriptor(desc)]
                geom_owners[key] = class_name(owner)
            geometry = {'classes': {key: class_name(name) if name else '' for key, name in geometry_classes.items()},
                        'humanoids': [class_name(name) for name in humanoids], 'members': geom_members,
                        'owners': geom_owners, 'partial_source': partial_source,
                        'kinds': {key: [kind, static] for key, (_, _, _, kind, static) in geometry_defs.items()}}
            profiles.append({'id': profile_id, 'version': version, 'java': metadata['javaVersion']['majorVersion'],
                             'classes': {role: class_name(name) for role, name in classes.items()},
                             'members': mapped, 'fov': source, 'version_class': class_name(version_owner),
                             'world_class': class_name(world_owner), 'current': version_member(version_owner, current),
                             'version_id': version_member(world_owner, ident),
                             'gui_class': gui_class, 'client_gui': ('gui', gui['descriptor']) if gui else ('', ''),
                             'geometry': geometry})
            report.append({'profile': profile_id, 'java': metadata['javaVersion']['majorVersion'],
                           'resolved_members': len(resolved), 'missing_members': missing,
                           'geometry_members': len(geometry_resolved),
                           'client_sha1': metadata['downloads']['client']['sha1'], 'fov_source': source,
                           'namespace_mapping_sha256': inputs.get(version + '-intermediary.jar', {}).get('sha256')
                           if namespace == 'intermediary' else
                           (inputs['mcp_config-1.20.1.zip']['sha256'] if namespace == 'forge-srg' else None)})

    output = ['// Generated by tools/compatibility/generate_profiles.py. Do not edit.', '#pragma once',
              '#include "runtime_profile.hpp"', '', 'namespace mcov::mappings {']
    quote = lambda value: json.dumps(value)
    for i, profile in enumerate(profiles):
        output.extend([f'inline constexpr MappingSet kProfileSet{i} = [] {{', '    auto set = kMojang;',
                       f'    set.id = {quote(profile["id"])};'])
        for role, name in profile['classes'].items(): output.append(f'    set.classes.{role} = {quote(name)};')
        for key, (name, desc) in profile['members'].items():
            output.append(f'    set.{key} = {{{quote(name)}, {quote(desc)}}};')
        output.extend(['    return set;', '}();', ''])
        g = profile['geometry']
        output.extend([f'inline constexpr GeometryProfile kGeometry{i} = [] {{', '    GeometryProfile g{};'])
        for role, name in g['classes'].items(): output.append(f'    g.{role} = {quote(name)};')
        for n, name in enumerate(g['humanoids']): output.append(f'    g.humanoids[{n}] = {quote(name)};')
        for key, (name, desc) in g['members'].items(): output.append(f'    g.{key} = {{{quote(name)}, {quote(desc)}}};')
        output.extend([f'    g.partialSource = PartialTickSource::{g["partial_source"]};', '    return g;', '}();', ''])
    output.append('inline constexpr RuntimeProfile kRuntimeProfiles[] = {')
    for i, p in enumerate(profiles):
        cur, ident = p['current'], p['version_id']
        output.append(f'    {{&kProfileSet{i}, {quote(p["version"])}, {p["java"]}, FovSource::{p["fov"]},')
        output.append(f'     {quote(p["version_class"])}, {{{quote(cur[0])}, {quote(cur[1])}}},')
        output.append(f'     {quote(p["world_class"])}, {{{quote(ident[0])}, {quote(ident[1])}}},')
        gui = p['client_gui']
        output.append(f'     {quote(p["gui_class"])}, {{{quote(gui[0])}, {quote(gui[1])}}}, &kGeometry{i}}},')
    output.extend(['};', '} // namespace mcov::mappings', ''])
    (ROOT / 'src/agent/generated_profiles.hpp').write_text('\n'.join(output), encoding='utf-8')
    (ROOT / 'docs/compatibility-signatures.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    (cache / 'profiles.json').write_text(json.dumps(profiles, indent=2) + '\n', encoding='utf-8')
    java = ['// Generated from the same audited profiles as generated_profiles.hpp.',
            'package mcov.sync;', 'final class FrameProfiles {',
            '    record Profile(String id, String client, String frame, String frameDesc, String camera, String update, String updateDesc, String instance, String instanceDesc, String position, String positionDesc, String positionField, String positionFieldDesc) {}',
            '    static final Profile[] PROFILES = {']
    for p in profiles:
        frame, update = p['geometry']['members']['runFrame'], p['geometry']['members']['updateCamera']
        values = [p['id'], p['classes']['client'], *frame, p['classes']['camera'], *update,
                  *p['members']['methodGetInstance'], *p['members']['methodCameraPosition'], *p['members']['fieldCameraPosition']]
        java.append('        new Profile(' + ', '.join(quote(v) for v in values) + '),')
    java.extend(['    };', '    static boolean isTarget(String owner) {',
        '        for (Profile p : PROFILES) if (p.client().equals(owner) || p.camera().equals(owner)) return true;',
        '        return false;', '    }', '    static boolean marker(String owner, String name, String descriptor, boolean isStatic) {',
        '        for (Profile p : PROFILES) {',
        '            if (isStatic && p.client().equals(owner) && p.instance().equals(name) && p.instanceDesc().equals(descriptor)) return true;',
        '            if (!isStatic && p.camera().equals(owner) && p.position().equals(name) && p.positionDesc().equals(descriptor)) return true;',
        '        }', '        return false;', '    }', '    static boolean fieldMarker(String owner, String name, String descriptor) {',
        '        for (Profile p : PROFILES)',
        '            if (p.camera().equals(owner) && p.positionField().equals(name) && p.positionFieldDesc().equals(descriptor)) return true;',
        '        return false;', '    }', '    static int role(String owner, String name, String descriptor) {',
        '        for (Profile p : PROFILES) {',
        '            if (p.client().equals(owner) && p.frame().equals(name) && p.frameDesc().equals(descriptor)) return 1;',
        '            if (p.camera().equals(owner) && p.update().equals(name) && p.updateDesc().equals(descriptor)) return 2;',
        '        }', '        return 0;', '    }', '}', ''])
    (ROOT / 'tools/frame-sync/FrameProfiles.java').write_text('\n'.join(java), encoding='utf-8')
    print(f'Generated {len(profiles)} runtime profiles from checksum-verified official clients.')


if __name__ == '__main__':
    generate(Path(sys.argv[1]))
