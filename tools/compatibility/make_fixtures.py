"""Small synthetic classes for JNI regression tests; contains no game code.

The fixtures use Java 17 bytecode and an isolated loader. They exercise real
JNI method descriptors and return types, not a mock JNIEnv function table.
"""
from __future__ import annotations
import json
import re
import struct
import sys
import zipfile
from pathlib import Path
from generate_profiles import owners, schema
from geometry_schema import geometry_schema


def u2(n): return struct.pack('>H', n)
def u4(n): return struct.pack('>I', n)


class Pool:
    def __init__(self): self.entries, self.ids = [], {}
    def add(self, key, value, wide=False):
        if key in self.ids: return self.ids[key]
        index = len(self.entries) + 1
        self.ids[key] = index
        self.entries.append(value)
        if wide: self.entries.append(b'')
        return index
    def utf(self, text):
        raw = text.encode()
        return self.add(('utf', text), b'\x01' + u2(len(raw)) + raw)
    def cls(self, name): return self.add(('cls', name), b'\x07' + u2(self.utf(name)))
    def string(self, text): return self.add(('str', text), b'\x08' + u2(self.utf(text)))
    def number(self, kind, value):
        return self.add((kind, value), (b'\x06' if kind == 'D' else b'\x04') +
                        struct.pack('>d' if kind == 'D' else '>f', value), kind == 'D')
    def ref(self, kind, owner, name, desc):
        cls = self.cls(owner)
        nt = self.add(('nt', name, desc), b'\x0c' + u2(self.utf(name)) + u2(self.utf(desc)))
        return self.add((kind, owner, name, desc), bytes([kind]) + u2(cls) + u2(nt))


def emit_class(name, fields, methods, specials, parent_name='java/lang/Object'):
    cp = Pool()
    this, parent, code_name = cp.cls(name), cp.cls(parent_name), cp.utf('Code')
    compiled_fields = [u2(9 if static else 1) + u2(cp.utf(n)) + u2(cp.utf(d)) + u2(0)
                       for (n, d), static in fields.items()]
    methods = dict(methods)
    methods[('<init>', '()V')] = False
    compiled_methods = []
    for (method, desc), static in methods.items():
        special = specials.get((method, desc))
        if method == '<init>':
            code = (b'\x2a\x13'+u2(cp.string('BLOCK'))+b'\x03\xb7'+u2(cp.ref(10,parent_name,'<init>','(Ljava/lang/String;I)V'))) if parent_name=='java/lang/Enum' else (b'\x2a\xb7'+u2(cp.ref(10,parent_name,'<init>','()V')))
            if ('testUuid','Ljava/util/UUID;') in fields:
                code += b'\x2a\xb8' + u2(cp.ref(10,'java/util/UUID','randomUUID','()Ljava/util/UUID;'))
                code += b'\xb5' + u2(cp.ref(9,name,'testUuid','Ljava/util/UUID;'))
            if ('testPitch','F') in fields:
                code += b'\x2a\x13' + u2(cp.number('F',10)) + b'\xb5' + u2(cp.ref(9,name,'testPitch','F'))
            if ('testTranslation','Ljava/lang/String;') in fields:
                code += b'\x2a\x13' + u2(cp.string('item.minecraft.diamond_helmet'))
                code += b'\xb5' + u2(cp.ref(9,name,'testTranslation','Ljava/lang/String;'))
            if special and special[0]=='constructor_fields':
                for field,field_desc,load in special[1]:
                    code+=b'\x2a'+load+b'\xb5'+u2(cp.ref(9,name,field,field_desc))
            if ('testStepY','I') in fields:
                code+=b'\x2a\x04\xb5'+u2(cp.ref(9,name,'testStepY','I'))
            if ('testMovementGuard','Ljava/lang/Object;') in fields:
                code+=b'\x2a\xbb'+u2(cp.cls('java/lang/Object'))+b'\x59\xb7'+u2(cp.ref(10,'java/lang/Object','<init>','()V'))
                code+=b'\xb5'+u2(cp.ref(9,name,'testMovementGuard','Ljava/lang/Object;'))
            code += b'\xb1'
        elif special and special[0]=='mutable_position':
            owner,coordinates=special[1:]
            code=b''
            for i,field in enumerate(coordinates):
                code+=b'\x2a'+bytes([0x1b+i])+b'\xb5'+u2(cp.ref(9,owner,field,'I'))
            code+=b'\x2a\xb0'
        elif special and special[0]=='world_states':
            pos,get_y,state=special[1:]
            code=b'\xb2'+u2(cp.ref(9,name,'testStates','Ljava/util/Map;'))+b'\x59\xc7\x00\x08\x57'
            code+=b'\xb2'+u2(cp.ref(9,name,'testBlockState','L'+state+';'))+b'\xb0'
            code+=b'\x2b\xb6'+u2(cp.ref(10,pos,get_y,'()I'))
            code+=b'\xb8'+u2(cp.ref(10,'java/lang/Integer','valueOf','(I)Ljava/lang/Integer;'))
            code+=b'\xb9'+u2(cp.ref(11,'java/util/Map','get','(Ljava/lang/Object;)Ljava/lang/Object;'))+b'\x02\x00\xc0'+u2(cp.cls(state))+b'\xb0'
        elif special and special[0]=='optional_field':
            backing,fallback,field_desc=special[1:]
            code=b'\x2a\xb4'+u2(cp.ref(9,name,backing,field_desc))+b'\x59\xc7\x00\x08\x57'
            code+=b'\xb2'+u2(cp.ref(9,name,fallback,field_desc))+b'\xb0\xb0'
        elif special and special[0]=='use_on':
            hit_desc,result=special[1:]
            code=b'\x2d\xb3'+u2(cp.ref(9,name,'testLastHit',hit_desc))
            code+=b'\xb2'+u2(cp.ref(9,name,'testSiteUses','I'))+b'\x04\x60\xb3'+u2(cp.ref(9,name,'testSiteUses','I'))
            code+=b'\xbb'+u2(cp.cls(result))+b'\x59\xb7'+u2(cp.ref(10,result,'<init>','()V'))+b'\xb0'
        elif special and special[0] == 'inventory_get':
            stack=special[1]
            code=b'\x2a\xb4'+u2(cp.ref(9,name,'testStacks','Ljava/util/List;'))+b'\x1b'
            code+=b'\xb9'+u2(cp.ref(11,'java/util/List','get','(I)Ljava/lang/Object;'))+b'\x02\x00'
            code+=b'\xc0'+u2(cp.cls(stack))+b'\xb0'
        elif special and special[0] == 'inventory_size':
            code=b'\x2a\xb4'+u2(cp.ref(9,name,'testStacks','Ljava/util/List;'))
            code+=b'\xb9'+u2(cp.ref(11,'java/util/List','size','()I'))+b'\x01\x00\xac'
        elif special and special[0] == 'swap':
            player,inventory,inv_field,inv_desc,living,stack_desc=special[1:]
            code=b'\x19\x05\xb4'+u2(cp.ref(9,player,inv_field,inv_desc))
            code+=b'\xb4'+u2(cp.ref(9,inventory,'testStacks','Ljava/util/List;'))+b'\x1c'
            code+=b'\xb9'+u2(cp.ref(11,'java/util/List','get','(I)Ljava/lang/Object;'))+b'\x02\x00'
            code+=b'\xc0'+u2(cp.cls(stack_desc[1:-1]))+b'\xb3'+u2(cp.ref(9,living,'testEquipment',stack_desc))
            for field,load in [('testSwapId',b'\x1b'),('testSwapSlot',b'\x1c'),('testSwapButton',b'\x1d')]:
                code+=load+b'\xb3'+u2(cp.ref(9,name,field,'I'))
            code+=b'\xb2'+u2(cp.ref(9,name,'testSwaps','I'))+b'\x04\x60\xb3'+u2(cp.ref(9,name,'testSwaps','I'))+b'\xb1'
        elif special and special[0] == 'increment':
            field=special[1]
            code=b'\xb2'+u2(cp.ref(9,name,field,'I'))+b'\x04\x60\xb3'+u2(cp.ref(9,name,field,'I'))+b'\xb1'
        elif special and special[0] == 'field':
            result = special[2][0]
            code = b'\xb2' + u2(cp.ref(9, name, special[1], special[2])) + {'D': b'\xaf', 'F': b'\xae', 'I': b'\xac', 'Z': b'\xac', 'J': b'\xad'}.get(result, b'\xb0')
        elif special and special[0] == 'instance_field':
            result = special[2][0]
            code = b'\x2a\xb4' + u2(cp.ref(9, name, special[1], special[2])) + {'D': b'\xaf', 'F': b'\xae', 'I': b'\xac', 'Z': b'\xac', 'J': b'\xad'}.get(result, b'\xb0')
        elif special and special[0] == 'set_float':
            code = b'\x2a\x23\xb5' + u2(cp.ref(9, name, special[1], 'F')) + b'\xb1'
        elif special and special[0] == 'freecam_position':
            vec,field,vector_desc=special[1:]
            code=b'\x2a\xbb'+u2(cp.cls(vec))+b'\x59\x27\x29\x18\x05\xb7'+u2(cp.ref(10,vec,'<init>','(DDD)V'))+b'\xb5'+u2(cp.ref(9,name,field,vector_desc))+b'\xb1'
        elif special and special[0] == 'freecam_rotation':
            code=b'\x2a\x23\xb5'+u2(cp.ref(9,name,'testFreeYaw','F'))+b'\x2a\x24\xb5'+u2(cp.ref(9,name,'testFreePitch','F'))+b'\xb1'
        elif special and special[0] == 'freecam_click':
            code=b'\xb2'+u2(cp.ref(9,name,'testClicks','I'))+b'\x04\x60\xb3'+u2(cp.ref(9,name,'testClicks','I'))+b'\x04\xac'
        elif special and special[0] == 'freecam_update':
            code=b'\x2a\x0b\xb6'+u2(cp.ref(10,name,special[1],'(F)V'))+b'\xb1'
        elif special and special[0] == 'attack':
            player, cooldown = special[1:]
            code = b'\x2b\x0b\xb5' + u2(cp.ref(9, player, cooldown, 'F'))
            code += b'\xb2' + u2(cp.ref(9, name, 'testAttacks', 'I')) + b'\x04\x60\xb3' + u2(cp.ref(9, name, 'testAttacks', 'I')) + b'\xb1'
        elif special and special[0] == 'turn':
            code=b''
            for backing,load in [('testYaw',b'\x27'),('testPitch',b'\x29')]:
                code += b'\x2a\x59\xb4'+u2(cp.ref(9,name,backing,'F'))+load
                code += b'\x14'+u2(cp.number('D',.15))+b'\x6b\x90\x62\xb5'+u2(cp.ref(9,name,backing,'F'))
            code += b'\xb1'
        elif special and special[0] == 'movement':
            entity,yaw,pitch,nested,move,vec,axes=special[1:]
            code=b''
            if nested:
                owner,nested_method,descriptor=nested
                code+=b'\x2a'
                if descriptor.startswith('(L'):
                    code+=b'\xbb'+u2(cp.cls(vec))+b'\x59\xb7'+u2(cp.ref(10,vec,'<init>','()V'))
                code+=b'\xb7'+u2(cp.ref(10,owner,nested_method,descriptor))
            if desc.startswith('(L'):
                # Nested scope + a read after its return catches premature cleanup.
                code+=b'\x2a\x0c\x2b\xb6'+u2(cp.ref(10,entity,move,'(FL'+vec+';)V'))
            for getter,backing in [(yaw,'testMovementYaw'),(pitch,'testMovementPitch')]:
                code+=b'\x2a\x2a\xb6'+u2(cp.ref(10,entity,getter,'()F'))+b'\xb5'+u2(cp.ref(9,entity,backing,'F'))
            if desc.startswith('(F'):
                # Synthetischer Vanilla-Eingabevektor: X/Z rotieren, Y bleibt unabhängig.
                def component(field): return b'\x2c\xb4'+u2(cp.ref(9,vec,field,'D'))
                angle=b'\x2a\xb6'+u2(cp.ref(10,entity,yaw,'()F'))+b'\x8d\x14'+u2(cp.number('D',3.141592653589793/180))+b'\x6b'
                sin=angle+b'\xb8'+u2(cp.ref(10,'java/lang/Math','sin','(D)D'))
                cos=angle+b'\xb8'+u2(cp.ref(10,'java/lang/Math','cos','(D)D'))
                for backing,expression in [
                    ('testMoveX',component(axes[0])+cos+b'\x6b'+component(axes[2])+sin+b'\x6b\x67'),
                    ('testMoveZ',component(axes[2])+cos+b'\x6b'+component(axes[0])+sin+b'\x6b\x63'),
                    ('testMoveY',component(axes[1]))]:
                    code+=b'\x2a'+expression+b'\x23\x8d\x6b\xb5'+u2(cp.ref(9,entity,backing,'D'))
            # Eine Exception aus einem Callee (kein explizites ATHROW im Scope).
            code+=b'\x2a\xb4'+u2(cp.ref(9,entity,'testMovementGuard','Ljava/lang/Object;'))
            code+=b'\xb8'+u2(cp.ref(10,'java/util/Objects','requireNonNull','(Ljava/lang/Object;)Ljava/lang/Object;'))+b'\x57\xb1'
        elif special and special[0] == 'camera_view':
            owner,getter,orbit=special[1:]
            code=b'\x2b\x24\xb6'+u2(cp.ref(10,owner,getter,'(F)F'))
            if orbit:
                code += b'\x8d\x14'+u2(cp.number('D',3.141592653589793/180))+b'\x6b'
                code += b'\xb8'+u2(cp.ref(10,'java/lang/Math',orbit,'(D)D'))
                code += b'\x14'+u2(cp.number('D',4))+b'\x6b\xaf'
            else: code += b'\xae'
        elif special and special[0] == 'statics':
            code = b''
            for field, field_desc in special[1]:
                target=field_desc[1:-1]
                code += b'\xbb' + u2(cp.cls(target)) + b'\x59\xb7' + u2(cp.ref(10, target, '<init>', '()V'))
                code += b'\xb3' + u2(cp.ref(9, name, field, field_desc))
            code += b'\xb1'
        elif special and special[0] == 'string':
            code = b'\x13' + u2(cp.string(special[1])) + b'\xb0'
        elif special and special[0] == 'new':
            owner = special[1]
            code = b'\xbb' + u2(cp.cls(owner)) + b'\x59\xb7' + u2(cp.ref(10, owner, '<init>', '()V')) + b'\xb0'
        elif special and special[0] == 'number':
            kind, value = special[1:]
            code = (b'\x11' + struct.pack('>h', int(value)) + b'\xac') if kind in ('I', 'Z') else (
                (b'\x14' if kind == 'D' else b'\x13') + u2(cp.number(kind, value)) + (b'\xaf' if kind == 'D' else b'\xae'))
        elif special and special[0] == 'uuid':
            code = b'\xb8' + u2(cp.ref(10, 'java/util/UUID', 'randomUUID', '()Ljava/util/UUID;')) + b'\xb0'
        elif special and special[0] == 'nop':
            code = b'\x00\xb1'
        elif special and special[0] == 'tick_done':
            code = b'\x04\xb3' + u2(cp.ref(9,name,'testVanillaTickDone','Z')) + b'\xb1'
        elif special and special[0] == 'camera_branch':
            code = b'\x2b\xc6\x00\x04\xb1\x00\xb1'
        else:
            result = desc.split(')')[1][0]
            code = {'V': b'\xb1', 'D': b'\x0e\xaf', 'F': b'\x0b\xae', 'J': b'\x09\xad',
                    'L': b'\x01\xb0', '[': b'\x01\xb0'}.get(result, b'\x03\xac')
        args = re.findall(r'\[*(?:L[^;]+;|[BCDFIJSZ])', desc[1:desc.index(')')])
        locals_count = (0 if static else 1) + sum(2 if arg in ('D', 'J') else 1 for arg in args)
        nested = u2(0)
        exception_table = u2(0)
        if special and special[0] == 'camera_guarded':
            # A live local must survive every injected branch inside a try block.
            # Real Camera.setup methods (and camera mods) keep such locals in handlers.
            marker_slot=locals_count
            code=b'\xbb'+u2(cp.cls('java/lang/Object'))+b'\x59\xb7'+u2(cp.ref(10,'java/lang/Object','<init>','()V'))+b'\x3a'+bytes([marker_slot])
            code+=b'\x09\x37'+bytes([marker_slot+1])+b'\x0e\x39'+bytes([marker_slot+3])
            start=len(code)
            code+=b'\x19'+bytes([marker_slot])+b'\xb6'+u2(cp.ref(10,'java/lang/Object','hashCode','()I'))+b'\x57\xb1'
            handler=len(code)
            code+=b'\x3a'+bytes([marker_slot+5])+b'\x19'+bytes([marker_slot])+b'\xb6'+u2(cp.ref(10,'java/lang/Object','hashCode','()I'))+b'\x57'
            code+=b'\x16'+bytes([marker_slot+1])+b'\x58\x18'+bytes([marker_slot+3])+b'\x58'
            terminal=len(code)
            code+=b'\xb1'
            def verification_type(t):
                if t in ('Z','B','C','S','I'): return b'\x01'
                if t in ('F','D','J'): return {'F':b'\x02','D':b'\x03','J':b'\x04'}[t]
                return b'\x07'+u2(cp.cls(t[1:-1] if t.startswith('L') else t))
            frame_locals=b'\x07'+u2(cp.cls(name))+b''.join(verification_type(t) for t in args)+b'\x07'+u2(cp.cls('java/lang/Object'))+b'\x04\x03'
            stack_map=u2(2)+b'\xff'+u2(handler)+u2(len(args)+4)+frame_locals+u2(1)+b'\x07'+u2(cp.cls('java/lang/Throwable'))
            # Vanilla also drops every local, including `this`, at terminal
            # returns. A hook that loads receiver slot 0 must retain its type.
            stack_map+=b'\xff'+u2(terminal-handler-1)+u2(0)+u2(0)
            nested=u2(1)+u2(cp.utf('StackMapTable'))+u4(len(stack_map))+stack_map
            exception_table=u2(1)+u2(start)+u2(handler)+u2(handler)+u2(0)
            locals_count+=6
        elif special and special[0] == 'camera_branch':
            stack_map = u2(1) + b'\x05'  # same_frame at the null branch's offset 5
            nested = u2(1) + u2(cp.utf('StackMapTable')) + u4(len(stack_map)) + stack_map
        elif special and special[0] in ('world_states','optional_field'):
            offset=12 if special[0]=='world_states' else 13
            stack_type='java/util/Map' if special[0]=='world_states' else special[3][1:-1]
            stack_map=u2(1)+bytes([64+offset])+b'\x07'+u2(cp.cls(stack_type))
            nested=u2(1)+u2(cp.utf('StackMapTable'))+u4(len(stack_map))+stack_map
        attr = u2(12) + u2(locals_count) + u4(len(code)) + code + exception_table + nested
        compiled_methods.append(u2(9 if static else 1) + u2(cp.utf(method)) + u2(cp.utf(desc)) + u2(1) +
                                u2(code_name) + u4(len(attr)) + attr)
    return (b'\xca\xfe\xba\xbe' + u2(0) + u2(61) + u2(len(cp.entries) + 1) + b''.join(cp.entries) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(len(compiled_fields)) + b''.join(compiled_fields) +
            u2(len(compiled_methods)) + b''.join(compiled_methods) + u2(0))


def read_profiles(path):
    if path.suffix == '.json': return json.loads(path.read_text())
    # Read the checked-in generated table so tests need no game downloads.
    source = path.read_text()
    sets = {}
    for index, body in re.findall(r'inline constexpr MappingSet kProfileSet(\d+) = \[\] \{(.*?)\}\(\);', source, re.S):
        sets[index] = {'id': re.search(r'set.id = "([^"]+)";', body)[1],
                       'classes': dict(re.findall(r'set.classes.(\w+) = "([^"]+)";', body)),
                       'members': {key: [name, desc] for key, name, desc in
                                   re.findall(r'set.(\w+) = \{"([^"]*)", "([^"]*)"\};', body)}}
    result = []
    pattern = (r'\{&kProfileSet(\d+), "([^"]+)", (\d+), FovSource::(\w+),\s*'
               r'"([^"]+)", \{"([^"]*)", "([^"]*)"\},\s*'
               r'"([^"]+)", \{"([^"]*)", "([^"]*)"\},\s*'
               r'"([^"]*)", \{"([^"]*)", "([^"]*)"\}, &kGeometry\d+\}')
    for row in re.findall(pattern, source):
        index, version, java, fov, vclass, current, csig, wclass, ident, isig, gui, gfield, gsig = row
        result.append(dict(sets[index], version=version, java=int(java), fov=fov,
                           version_class=vclass, current=[current, csig], world_class=wclass,
                           version_id=[ident, isig], gui_class=gui, client_gui=[gfield, gsig]))
        body = re.search(r'inline constexpr GeometryProfile kGeometry' + index + r' = \[\] \{(.*?)\}\(\);', source, re.S)[1]
        classes = dict(re.findall(r'g\.(\w+) = "([^"]*)";', body))
        humanoids = [name for _, name in re.findall(r'g\.humanoids\[(\d+)\] = "([^"]+)";', body)]
        members = {key: [name, desc] for key, name, desc in re.findall(r'g\.(\w+) = \{"([^"]*)", "([^"]*)"\};', body)}
        core_classes, _ = schema()
        geometry_classes, _, definitions, partial = geometry_schema(version)
        class_map = {canonical: result[-1]['classes'][role] for role, canonical in core_classes.items()}
        class_map.update({canonical: classes[role] for role, canonical in geometry_classes.items() if canonical})
        class_map['net/minecraft/world/inventory/AbstractContainerMenu']=result[-1]['members']['fieldScreenHandler'][1][1:-1]
        class_map['net/minecraft/world/level/Level']=result[-1]['classes']['clientLevel']
        result[-1]['geometry'] = dict(classes=classes, humanoids=humanoids, members=members,
            owners={key: class_map[definition[0]] for key, definition in definitions.items()},
            kinds={key: definition[3:] for key, definition in definitions.items()}, partial_source=partial)
    if len(result) != len(sets) or not result: raise ValueError('invalid generated profile table')
    return result


def generate(profiles_path, output):
    output.mkdir(parents=True, exist_ok=True)
    profiles = read_profiles(profiles_path)
    for profile in profiles:
        classes = {}
        def cls(name):
            return classes.setdefault(name, {'fields': {}, 'methods': {}, 'specials': {}})
        def add(owner, kind, name, desc, static=False):
            cls(owner)[kind][(name, desc)] = static
            for reference in re.findall(r'L([^;]+);', desc):
                if not reference.startswith('java/'): cls(reference)
        owner_spec = owners(profile['classes'])
        owner_spec['fieldSyncId'] = profile['members']['fieldScreenHandler'][1][1:-1]
        if profile['fov'] == 'CameraFloat': owner_spec['methodRealFov'] = profile['classes']['camera']
        if profile['gui_class']: owner_spec['fieldCurrentScreen'] = profile['gui_class']
        for name in profile['classes'].values(): cls(name)
        for key, (name, desc) in profile['members'].items():
            add(owner_spec[key], 'methods' if key.startswith('method') else 'fields', name, desc,
                key in ('methodGetInstance', 'fieldFps', 'fieldSwapAction') or key.startswith('slot'))
        geometry = profile['geometry']
        for name in geometry['classes'].values():
            if name: cls(name)
        for name in geometry['humanoids']: cls(name)
        for key, (name, desc) in geometry['members'].items():
            kind, static = geometry['kinds'][key]
            add(geometry['owners'][key], kind, name, desc, static)
        core = profile['classes']
        cls(core['camera'])['specials'][tuple(geometry['members']['freecamPosition'])]=('freecam_position',core['vec3'],*profile['members']['fieldCameraPosition'])
        cls(core['camera'])['specials'][tuple(geometry['members']['freecamRotation'])]=('freecam_rotation',)
        for field in ('testFreeYaw','testFreePitch'): add(core['camera'],'fields',field,'F')
        add(core['client'],'fields','testClicks','I',True)
        add(core['client'],'fields','testBreaks','I',True)
        cls(core['client'])['specials'][tuple(geometry['members']['freecamAttack'])]=('freecam_click',)
        cls(core['client'])['specials'][tuple(geometry['members']['freecamContinueAttack'])]=('increment','testBreaks')
        add(core['gameRenderer'],'fields','testHands','I',True)
        cls(core['gameRenderer'])['specials'][tuple(geometry['members']['freecamHand'])]=('increment','testHands')
        cls(core['blockPosMut'])['parent_name']=geometry['members']['blockHitPos'][1].split(')')[1][1:-1]
        cls(core['livingEntity'])['parent_name'] = core['entity']
        cls(core['player'])['parent_name'] = core['livingEntity']
        avatar_owner = geometry['classes']['avatarOwnerClass']
        if avatar_owner: cls(avatar_owner)['parent_name'] = core['player']
        cls(core['localPlayer'])['parent_name'] = avatar_owner or core['player']
        for name in geometry['humanoids']: cls(name)['parent_name'] = core['livingEntity']
        pose = geometry['classes']['poseClass']
        constants = [tuple(geometry['members'][key]) for key in ('poseStanding', 'poseCrouch', 'poseSwim', 'poseGlide', 'poseSleep')]
        add(pose, 'methods', '<clinit>', '()V', True)
        cls(pose)['specials'][('<clinit>', '()V')] = ('statics', constants)
        for key, backing in [('getBounds', 'testBounds'), ('getPose', 'testPose'), ('getDimensions', 'testDimensions'),
                             ('avatarState', 'testAvatar')]:
            if key not in geometry['members']: continue
            member = geometry['members'][key]
            owner = geometry['owners'][key]
            desc = member[1].split(')')[1]
            add(owner, 'fields', backing, desc, True)
            cls(owner)['specials'][tuple(member)] = ('field', backing, desc)
        for key, value in [('partialTick', .5), ('walkSpeed', .5), ('walkPosition', 1.5), ('headPitch', 10)]:
            member = geometry['members'][key]
            cls(geometry['owners'][key])['specials'][tuple(member)] = ('number', 'F', value)
        for key in ('runFrame', 'updateCamera'):
            member = geometry['members'][key]
            cls(geometry['owners'][key])['specials'][tuple(member)] = ('camera_branch' if key == 'updateCamera' else 'nop',)
        if profile['version']=='26.2':
            cls(core['camera'])['specials'][tuple(geometry['members']['updateCamera'])]=('freecam_update',geometry['members']['freecamSetup'][0])
        cls(core['camera'])['specials'][tuple(geometry['members']['freecamSetup'])]=('camera_guarded',)
        if 'getDelta' in geometry['members']:
            member = geometry['members']['getDelta']
            cls(core['client'])['specials'][tuple(member)] = ('new', geometry['classes']['deltaClass'])
        opt_get = profile['members']['methodOptionGet']
        add(core['optionValue'], 'fields', 'testValue', 'Ljava/lang/Object;', True)
        cls(core['optionValue'])['specials'][tuple(opt_get)] = ('field', 'testValue', 'Ljava/lang/Object;')
        add(profile['version_class'], 'methods', *profile['current'], True)
        add(profile['world_class'], 'methods', *profile['version_id'])
        cls(profile['version_class'])['specials'][tuple(profile['current'])] = ('new', profile['world_class'])
        cls(profile['world_class'])['specials'][tuple(profile['version_id'])] = ('string', profile['version'])
        for key, field, role in [('methodGetInstance', 'testInstance', 'client'),
                                  ('methodGetCamera', 'testCamera', 'gameRenderer'),
                                  ('methodCameraPosition', 'testPosition', 'camera'),
                                  ('methodConnection', 'testConnection', 'client'),
                                  ('methodPlayerInfo', 'testPlayerInfo', 'connection'),
                                  ('methodEntityById', 'testEntity', 'clientLevel'),
                                  ('methodGetEquipped', 'testEquipment', 'livingEntity')]:
            member = profile['members'][key]
            desc = member[1].split(')')[1]
            owner = profile['classes'][role]
            add(owner, 'fields', field, desc, True)
            cls(owner)['specials'][tuple(member)] = ('field', field, desc)
        add(core['equipSlot'], 'methods', '<clinit>', '()V', True)
        slots=[tuple(profile['members'][key]) for key in
               ('slotHead','slotChest','slotLegs','slotFeet','slotMainHand','slotOffHand')]
        cls(core['equipSlot'])['specials'][('<clinit>', '()V')] = ('statics', slots)
        member=profile['members']['methodEntityUuid']
        cls(core['entity'])['specials'][tuple(member)] = ('uuid',)
        # Echte Zustandsänderungen für die native Client-Tick-Brücke.
        hand=geometry['classes']['handClass']
        add(hand,'methods','<clinit>','()V',True)
        cls(hand)['specials'][('<clinit>','()V')]=('statics',[tuple(geometry['members']['mainHand'])])
        for key, backing, static in [('clientPaused','testPaused',True),('eyePosition','testEye',True),
                                     ('lineOfSight','testVisible',True),('attackCooldown','testCooldown',False),
                                     ('attackReach','testReach',True),('farReach','testFarReach',True),
                                     ('pickBlocks','testHit',True),('hitLocation','testLocation',True),
                                     ('entityAlive','testAlive',False)]:
            if key not in geometry['members']: continue
            member=geometry['members'][key]; owner=geometry['owners'][key]; desc=member[1].split(')')[1]
            add(owner,'fields',backing,desc,static)
            cls(owner)['specials'][tuple(member)]=('field' if static else 'instance_field',backing,desc)
        for key,backing in [('methodGetX','testX'),('methodGetY','testY'),('methodGetZ','testZ'),('methodGetId','testId'),
                            ('methodEntityYaw','testYaw'),('methodGetHealth','testHealth'),('methodEntityUuid','testUuid')]:
            member=profile['members'][key]; owner=owner_spec[key]; desc=member[1].split(')')[1]
            add(owner,'fields',backing,desc,False)
            cls(owner)['specials'][tuple(member)]=('instance_field',backing,desc)
        # Ping-Test kann eine leere UUID nicht nutzen; alle Test-Entities initialisieren diese selbst.
        for key,backing in [('setYaw','testYaw'),('setPitch','testPitch')]:
            member=geometry['members'][key]; owner=geometry['owners'][key]
            add(owner,'fields',backing,'F',False); cls(owner)['specials'][tuple(member)]=('set_float',backing)
        member=geometry['members']['headPitch']
        cls(geometry['owners']['headPitch'])['specials'][tuple(member)]=('instance_field','testPitch','F')
        for key,backing in [('viewYaw','testYaw'),('viewPitch','testPitch')]:
            member=geometry['members'][key]
            cls(core['entity'])['specials'][tuple(member)]=('instance_field',backing,'F')
        member=geometry['members']['turn']
        cls(core['entity'])['specials'][tuple(member)]=('turn',)
        for backing in ('testMovementYaw','testMovementPitch'):
            add(core['entity'],'fields',backing,'F')
        for backing in ('testMoveX','testMoveY','testMoveZ'):
            add(core['entity'],'fields',backing,'D')
        add(core['entity'],'fields','testMovementGuard','Ljava/lang/Object;')
        for key in ('movementRelative','movementTravel','movementJump','movementPlayerTravel','movementPlayerJump','movementLocalStep'):
            if key not in geometry['members']: continue
            member=geometry['members'][key]; owner=geometry['owners'][key]
            nested=None
            if key=='movementPlayerTravel': nested=(core['livingEntity'],*geometry['members']['movementTravel'])
            if key=='movementPlayerJump': nested=(core['livingEntity'],*geometry['members']['movementJump'])
            if key=='movementLocalStep': nested=(core['player'],*geometry['members']['movementPlayerTravel'])
            cls(owner)['specials'][tuple(member)]=('movement',core['entity'],profile['members']['methodEntityYaw'][0],
                geometry['members']['headPitch'][0],nested,geometry['members']['movementRelative'][0],core['vec3'],
                [profile['members'][field][0] for field in ('fieldVecX','fieldVecY','fieldVecZ')])
        member=geometry['members']['clientTick']; owner=geometry['owners']['clientTick']
        add(owner,'fields','testVanillaTickDone','Z',True)
        cls(owner)['specials'][tuple(member)]=('tick_done',)
        # Kamera-Witnesses nutzen genau dieselben Getter-Aufrufstellen wie die auditierten
        # Setup-/alignWithEntity-Methoden, inklusive eines F5-Abstands von vier Blöcken.
        for name,key,orbit in [('testCameraYaw','viewYaw',''),('testCameraPitch','viewPitch',''),
                               ('testOrbitX','viewYaw','sin'),('testOrbitZ','viewYaw','cos')]:
            desc='(L'+core['entity']+';F)'+('D' if orbit else 'F')
            add(core['camera'],'methods',name,desc)
            cls(core['camera'])['specials'][(name,desc)]=('camera_view',core['entity'],geometry['members'][key][0],orbit)
        member=profile['members']['methodEntitiesForRendering']
        add(core['clientLevel'],'fields','testIterable','Ljava/lang/Iterable;',True)
        cls(core['clientLevel'])['specials'][tuple(member)]=('field','testIterable','Ljava/lang/Iterable;')
        member=geometry['members']['attackEntity']; owner=geometry['owners']['attackEntity']
        add(owner,'fields','testAttacks','I',True)
        cls(owner)['specials'][tuple(member)]=('attack',core['player'],'testCooldown')
        for key, value in [('methodGetArmor',18),('methodLatency',87),('methodItemGetDamage',17),
                           ('methodItemGetMaxDamage',100),('methodItemGetCount',3),('methodItemIsDamageable',1)]:
            member=profile['members'][key]
            cls(owner_spec[key])['specials'][tuple(member)] = ('number', member[1].split(')')[1], value)
        for key, text in [('methodComponentToString','Diamond Helmet')]:
            member=profile['members'][key]
            cls(owner_spec[key])['specials'][tuple(member)] = ('string', text)
        member=profile['members']['methodItemGetName']
        cls(core['itemStack'])['specials'][tuple(member)] = ('new',core['component'])
        for key, value in [('methodRealFov', 82.5), ('methodGetYaw', 29.25), ('methodGetPitch', -11.5)]:
            member = profile['members'][key]
            cls(owner_spec[key])['specials'][tuple(member)] = ('number', member[1].split(')')[1], value)
        # Zustandsabhängige Inventar- und Block-Fixtures für echte JNI-Aktionen.
        for key,backing,role,desc in [('methodStackGetItem','testItem','itemStack','L'+core['item']+';'),
                                    ('methodItemIsEmpty','testEmpty','itemStack','Z'),
                                    ('methodItemTransKey','testTranslation','item','Ljava/lang/String;')]:
            member=profile['members'][key]; owner=core[role]
            add(owner,'fields',backing,desc,False)
            cls(owner)['specials'][tuple(member)]=('instance_field',backing,desc)
        inventory=core['playerInv']; add(inventory,'fields','testStacks','Ljava/util/List;')
        cls(inventory)['specials'][tuple(profile['members']['methodInvGetStack'])]=('inventory_get',core['itemStack'])
        cls(inventory)['specials'][tuple(profile['members']['methodInvSize'])]=('inventory_size',)
        menu_desc=profile['members']['fieldScreenHandler'][1]
        own_desc=geometry['members']['inventoryMenu'][1]
        cls(own_desc[1:-1])['parent_name']=menu_desc[1:-1]
        cls(geometry['classes']['crystalClass'])['parent_name']=core['entity']
        cls(geometry['classes']['blockHitClass'])['parent_name']=geometry['classes']['hitClass']
        property_name=geometry['members']['stateValue'][1][2:geometry['members']['stateValue'][1].index(';')]
        for key in ('axisProperty','anchorCharge'):
            member=geometry['members'][key]; owner=geometry['owners'][key]
            cls(member[1][1:-1])['parent_name']=property_name
            add(owner,'methods','<clinit>','()V',True)
            cls(owner)['specials'][('<clinit>','()V')]=('statics',[tuple(member)])
        add(core['slotAction'],'methods','<clinit>','()V',True)
        cls(core['slotAction'])['specials'][('<clinit>','()V')]=('statics',[tuple(profile['members']['fieldSwapAction'])])
        swap_owner=owner_spec['methodClickSlot']; stack_desc=profile['members']['methodGetEquipped'][1].split(')')[1]
        for field in ('testSwaps','testSwapId','testSwapSlot','testSwapButton'): add(swap_owner,'fields',field,'I',True)
        cls(swap_owner)['specials'][tuple(profile['members']['methodClickSlot'])]=('swap',core['player'],inventory,*profile['members']['fieldInventory'],core['livingEntity'],stack_desc)
        cls(geometry['members']['hitType'][1].split(')')[1][1:-1])['parent_name']='java/lang/Enum'
        for key,backing in [('hitType','testHitType'),('blockHitPos','testBlockPos'),('carriedStack','testCarried'),('stateValue','testStateValue'),
                            ('dimensionType','testDimension'),('environment','testEnvironment'),('environmentValue','testAttribute')]:
            if key not in geometry['members']: continue
            member=geometry['members'][key]; owner=geometry['owners'][key]; desc=member[1].split(')')[1]
            add(owner,'fields',backing,desc,True); cls(owner)['specials'][tuple(member)]=('field',backing,desc)
        if 'environmentAnchor' in geometry['members']:
            member=geometry['members']['environmentAnchor']; owner=geometry['owners']['environmentAnchor']
            add(owner,'methods','<clinit>','()V',True); cls(owner)['specials'][('<clinit>','()V')]=('statics',[tuple(member)])
        member=profile['members']['methodGetBlockState']; add(core['clientLevel'],'fields','testBlockState',member[1].split(')')[1],True)
        cls(core['clientLevel'])['specials'][tuple(member)]=('field','testBlockState',member[1].split(')')[1])
        member=profile['members']['methodStateGetBlock']; add(core['blockState'],'fields','testBlock',member[1].split(')')[1],True)
        cls(core['blockState'])['specials'][tuple(member)]=('field','testBlock',member[1].split(')')[1])
        member=profile['members']['methodBlockTransKey']; add(core['block'],'fields','testBlockKey','Ljava/lang/String;')
        cls(core['block'])['specials'][tuple(member)]=('instance_field','testBlockKey','Ljava/lang/String;')
        if 'anchorWorks' in geometry['members']:
            member=geometry['members']['anchorWorks']; owner=geometry['owners']['anchorWorks']; add(owner,'fields','testAnchorWorks','Z')
            cls(owner)['specials'][tuple(member)]=('instance_field','testAnchorWorks','Z')
        member=geometry['members']['useItem']; add(core['client'],'fields','testUses','I',True)
        cls(core['client'])['specials'][tuple(member)]=('increment','testUses')
        # Place/Use kommt aus dem Spiel, feste HitResults und bestätigte Weltzustände.
        for key,backing,desc,static,value in [('keyDown','testDown','Z',False,None),
            ('replaceable','testReplaceable','Z',False,None),('blockReach','testBlockReach','D',True,None),
            ('oldBlockReach','testBlockReach','F',True,None),('consumesAction','testConsumes','Z',True,None)]:
            if key not in geometry['members']: continue
            member=geometry['members'][key]; owner=geometry['owners'][key]
            add(owner,'fields',backing,desc,static)
            cls(owner)['specials'][tuple(member)]=('field' if static else 'instance_field',backing,desc)
        pos=geometry['classes']['blockPosClass']; vec=core['vec3']; hit=geometry['classes']['blockHitClass']
        mutable=core['blockPosMut']; cls(mutable)['parent_name']=pos
        cls(mutable)['specials'][tuple(profile['members']['methodMutableSet'])]=('mutable_position',pos,
            [geometry['members']['pos'+axis][0] for axis in 'XYZ'])
        for key,owner,fields_to_load in [('blockPosCtor',pos,[(geometry['members']['pos'+axis][0],'I',bytes([0x1b+i])) for i,axis in enumerate('XYZ')]),
            ('vecCtor',vec,[(profile['members']['fieldVec'+axis][0],'D',load) for axis,load in zip('XYZ',(b'\x27',b'\x29',b'\x18\x05'))]),
            ('blockHitCtor',hit,[('testOwnLocation','L'+vec+';',b'\x2b'),('testDirection','L'+geometry['classes']['directionClass']+';',b'\x2c'),('testOwnPos','L'+pos+';',b'\x2d')])]:
            for field,desc,load in fields_to_load: add(owner,'fields',field,desc,False)
            cls(owner)['specials'][tuple(geometry['members'][key])]=('constructor_fields',fields_to_load)
        for axis in 'XYZ':
            member=geometry['members']['pos'+axis]
            cls(pos)['specials'][tuple(member)]=('instance_field',member[0],'I')
            member=geometry['members']['step'+axis]; direction=geometry['classes']['directionClass']
            add(direction,'fields','testStep'+axis,'I',False)
            cls(direction)['specials'][tuple(member)]=('instance_field','testStep'+axis,'I')
        add(direction,'methods','<clinit>','()V',True)
        cls(direction)['specials'][('<clinit>','()V')]=('statics',[tuple(geometry['members']['upDirection'])])
        member=geometry['members']['hitDirection']; cls(hit)['specials'][tuple(member)]=('instance_field','testDirection','L'+direction+';')
        member=geometry['members']['blockHitPos']; cls(hit)['specials'][tuple(member)]=('optional_field','testOwnPos','testBlockPos','L'+pos+';')
        member=geometry['members']['hitLocation']; add(hit,'methods',*member)
        cls(hit)['specials'][tuple(member)]=('instance_field','testOwnLocation','L'+vec+';')
        member=geometry['members']['useOn']; owner=geometry['owners']['useOn']; result=geometry['classes']['resultClass']
        add(owner,'fields','testLastHit','L'+hit+';',True); add(owner,'fields','testSiteUses','I',True)
        cls(owner)['specials'][tuple(member)]=('use_on','L'+hit+';',result)
        add(core['clientLevel'],'fields','testStates','Ljava/util/Map;',True)
        member=profile['members']['methodGetBlockState']
        cls(core['clientLevel'])['specials'][tuple(member)]=('world_states',pos,geometry['members']['posY'][0],core['blockState'])
        add(core['blockState'],'fields','testOwnBlock','L'+core['block']+';',False)
        member=profile['members']['methodStateGetBlock']
        cls(core['blockState'])['specials'][tuple(member)]=('optional_field','testOwnBlock','testBlock','L'+core['block']+';')
        if profile['gui_class']:
            add(profile['classes']['client'], 'fields', *profile['client_gui'])
        for suffix in ('', '-field-camera'):
            with zipfile.ZipFile(output / (profile['id'] + suffix + '.jar'), 'w') as jar:
                for name, spec in classes.items():
                    if suffix and name == core['camera']:
                        spec = dict(spec, methods=dict(spec['methods']))
                        spec['methods'].pop(tuple(profile['members']['methodCameraPosition']))
                    jar.writestr(name + '.class', emit_class(name, **spec))
    print(f'Generated {len(profiles) * 2} isolated JNI fixtures (accessor and field-only cameras).')


if __name__ == '__main__':
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
