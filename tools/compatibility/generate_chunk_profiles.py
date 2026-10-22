"""Resolve the chunk snapshot API against the official class/mapping cache."""
import hashlib,json,re,sys
from pathlib import Path
from classfile import NamedJar
from generate_profiles import ROOT,VERSIONS,namespace_map,namespace_method

S=[]
def m(key,owner,name,desc): S.append((key,owner,name,desc,'methods'))
def f(key,owner,name,desc): S.append((key,owner,name,desc,'fields'))
client='net/minecraft/client/Minecraft'; level='net/minecraft/client/multiplayer/ClientLevel'
chunk='net/minecraft/world/level/chunk/ChunkAccess'; section='net/minecraft/world/level/chunk/LevelChunkSection'
palette='net/minecraft/world/level/chunk/PalettedContainer'; entity='net/minecraft/world/entity/Entity'
f('level',client,'level','L'+level+';')
f('player',client,'player','Lnet/minecraft/client/player/LocalPlayer;')
m('source',level,'getChunkSource','()Lnet/minecraft/client/multiplayer/ClientChunkCache;')
f('storage','net/minecraft/client/multiplayer/ClientChunkCache','storage','Lnet/minecraft/client/multiplayer/ClientChunkCache$Storage;')
f('chunks','net/minecraft/client/multiplayer/ClientChunkCache$Storage','chunks','Ljava/util/concurrent/atomic/AtomicReferenceArray;')
m('sections',chunk,'getSections','()[L'+section+';')
m('position',chunk,'getPos','()Lnet/minecraft/world/level/ChunkPos;')
f('cx','net/minecraft/world/level/ChunkPos','x','I'); f('cz','net/minecraft/world/level/ChunkPos','z','I')
m('states',section,'getStates','()L'+palette+';')
m('biomes',section,'getBiomes','()Lnet/minecraft/world/level/chunk/PalettedContainerRO;')
m('copy',palette,'copy','()L'+palette+';')
m('cell',palette,'get','(III)Ljava/lang/Object;')
m('entities',level,'entitiesForRendering','()Ljava/lang/Iterable;')
for axis in 'xyz': m(axis,entity,'get'+axis.upper(),'()D')
m('dimension',level,'dimension','()Lnet/minecraft/resources/ResourceKey;')
m('blockEntities','net/minecraft/world/level/chunk/LevelChunk','getBlockEntities','()Ljava/util/Map;')
for axis in 'XYZ': m('pos'+axis,'net/minecraft/core/BlockPos','get'+axis,'()I')
m('bees','net/minecraft/world/level/block/entity/BeehiveBlockEntity','getOccupantCount','()I')
m('chat','net/minecraft/client/player/LocalPlayer','sendSystemMessage','(Lnet/minecraft/network/chat/Component;)V')
m('literal','net/minecraft/network/chat/Component','literal','(Ljava/lang/String;)Lnet/minecraft/network/chat/MutableComponent;')

def generate(cache):
    for name,entry in json.loads(Path(__file__).with_name('mapping_inputs.json').read_text()).items():
        if hashlib.sha256((cache/name).read_bytes()).hexdigest()!=entry['sha256']: raise ValueError('Mapping checksum mismatch: '+name)
    tables=[]
    for version in VERSIONS:
        meta=json.loads((cache/(version+'.json')).read_text(encoding='utf-8-sig'))
        for kind,suffix in [('client','.jar'),('client_mappings','-mappings.txt')]:
            if kind in meta['downloads'] and hashlib.sha1((cache/(version+suffix)).read_bytes()).hexdigest()!=meta['downloads'][kind]['sha1']: raise ValueError('Client checksum mismatch: '+version)
        jar=NamedJar(cache,version); resolved=[]
        for key,owner,name,desc,kind in S:
            if key=='chat' and version!='26.2': name,desc='displayClientMessage','(Lnet/minecraft/network/chat/Component;Z)V'
            found=next(((o,v) for o,v in jar.members(owner,kind) if v['name']==name and v['descriptor']==desc),None)
            if not found: raise ValueError(f'{version}: missing {key}: {owner}.{name}{desc}')
            resolved.append((key,*found,kind))
        # Height accessor was renamed in newer releases. Resolve the actual API.
        height=next(((o,v) for o,v in jar.members(chunk,'methods') if v['name'] in ('getMinY','getMinBuildHeight') and v['descriptor']=='()I'),None)
        if not height: raise ValueError('Missing chunk height: '+version)
        resolved.append(('minY',*height,'methods'))
        for ns in ['mojmap']+([] if version=='26.2' else ['vanilla','intermediary'])+(['forge-srg'] if version=='1.20.1' else []):
            classes,fields,methods=namespace_map(cache,version,ns)
            def cls(name):
                raw=jar.mapping.get(name,{}).get('obf',name)
                return raw if ns=='vanilla' else classes.get(raw,raw) if ns=='intermediary' else name
            def desc(value): return re.sub(r'L([^;]+);',lambda a:'L'+cls(a[1])+';',value)
            rows=[]
            for key,owner,v,kind in resolved:
                name=v['name']; raw=jar.mapping.get(owner,{}).get('obf',owner)
                if ns=='vanilla': name=v['raw_name']
                elif ns in ('intermediary','forge-srg'):
                    name=namespace_method(jar,owner,v['raw_name'],v['raw_descriptor'],methods) if kind=='methods' else fields.get((raw,v['raw_name'],None if ns=='forge-srg' else v['raw_descriptor']),v['raw_name'])
                rows.append([key,cls(owner),name,desc(v['descriptor'])])
            for key,name in [('Mob','net/minecraft/world/entity/Mob'),('Item','net/minecraft/world/entity/item/ItemEntity'),('Bear','net/minecraft/world/entity/animal/PolarBear'),('Bee','net/minecraft/world/entity/animal/Bee'),('Hive','net/minecraft/world/level/block/entity/BeehiveBlockEntity')]:
                # Recent versions reorganized animal packages.
                if not jar.load(name):
                    suffix=name.rsplit('/',1)[-1]
                    choices=jar.mapping if jar.mapping else [n[:-6] for n in jar.jar.namelist() if n.endswith('.class')]
                    name=next((n for n in choices if n.endswith('/'+suffix)),name)
                if not jar.load(name): raise ValueError('Missing entity class: '+name)
                rows.append(['class.'+key,cls(name),'',''])
            tables.append((ns+'-'+version,rows))
    out=['// Generated against official Minecraft classes; do not hand-edit.','package mcov;','final class ChunkFinderProfiles {','static String[][] get(String id) { return switch(id) {']
    for i,(name,rows) in enumerate(tables): out.append(f'case {json.dumps(name)} -> P{i};')
    out+=['default -> null; }; }']
    for i,(_,rows) in enumerate(tables):
        out.append(f'private static final String[][] P{i}={{')
        out.extend('{'+','.join(map(json.dumps,row))+'},' for row in rows)
        out.append('};')
    out.append('}')
    (ROOT/'tools/frame-sync/ChunkFinderProfiles.java').write_text('\n'.join(out)+'\n',encoding='utf-8')
    print(f'Audited {len(tables)} Chunkfinder profiles')

if __name__=='__main__': generate(Path(sys.argv[1]))


