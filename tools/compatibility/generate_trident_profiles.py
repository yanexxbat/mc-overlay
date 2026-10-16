"""Audit trident release bindings against checksum-verified cached game jars."""
import hashlib,json,re,sys
from pathlib import Path
from classfile import NamedJar
from generate_profiles import namespace_map,namespace_method,VERSIONS,ROOT

C={'living':'net/minecraft/world/entity/LivingEntity','entity':'net/minecraft/world/entity/Entity',
   'manager':'net/minecraft/client/multiplayer/MultiPlayerGameMode','helper':'net/minecraft/world/item/enchantment/EnchantmentHelper'}
STACK='Lnet/minecraft/world/item/ItemStack;'
S=[('usingItem','living','isUsingItem','()Z','methods'),
   ('ticks','living','getTicksUsingItem','()I','methods'),
   ('hand','living','getUsedItemHand','()Lnet/minecraft/world/InteractionHand;','methods'),
   ('stack','living','getUseItem','()'+STACK,'methods'),
   ('wet','entity','isInWaterOrRain','()Z','methods'),
   ('autoSpin','living','isAutoSpinAttack','()Z','methods'),
   ('release','manager','releaseUsingItem','(Lnet/minecraft/world/entity/player/Player;)V','methods'),
   ('spin','helper','getTridentSpinAttackStrength','('+STACK+'L'+C['living']+';)F','methods')]

def generate(cache):
    for name,entry in json.loads(Path(__file__).with_name('mapping_inputs.json').read_text()).items():
        if hashlib.sha256((cache/name).read_bytes()).hexdigest()!=entry['sha256']: raise ValueError('Mapping checksum mismatch: '+name)
    rows=[]
    for version in VERSIONS:
        meta=json.loads((cache/(version+'.json')).read_text(encoding='utf-8-sig'))
        for kind,suffix in [('client','.jar'),('client_mappings','-mappings.txt')]:
            if kind in meta['downloads'] and hashlib.sha1((cache/(version+suffix)).read_bytes()).hexdigest()!=meta['downloads'][kind]['sha1']:
                raise ValueError('Client checksum mismatch: '+version)
        jar=NamedJar(cache,version); resolved=[]
        specs=[(k,r,("getRiptide" if version=="1.20.1" else n),("("+STACK+")I" if version=="1.20.1" else d),t) if k=="spin" else (k,r,n,d,t) for k,r,n,d,t in S]
        for key,role,name,desc,kind in specs:
            found=next(((owner,m) for owner,m in jar.members(C[role],kind) if m['name']==name and m['descriptor']==desc),None)
            if not found or bool(found[1]['flags']&8)!=(key=='spin'): raise ValueError('Trident member missing: '+version+' '+key)
            resolved.append((key,role,*found,kind))
        spaces=['mojmap']+([] if version=='26.2' else ['vanilla','intermediary'])+(['forge-srg'] if version=='1.20.1' else [])
        for ns in spaces:
            classes,fields,methods=namespace_map(cache,version,ns)
            def cls(name):
                raw=jar.mapping.get(name,{}).get('obf',name)
                return raw if ns=='vanilla' else classes.get(raw,raw) if ns=='intermediary' else name
            def desc(value): return re.sub(r'L([^;]+);',lambda m:'L'+cls(m[1])+';',value)
            row={'id':ns+'-'+version,'classes':{key:cls(value) for key,value in C.items()},'members':{},'owners':{},'kinds':{}}
            for key,role,owner,m,kind in resolved:
                name=m['name']; raw=jar.mapping.get(owner,{}).get('obf',owner)
                if ns=='vanilla': name=m['raw_name']
                elif ns in ('intermediary','forge-srg'):
                    name=namespace_method(jar,owner,m['raw_name'],m['raw_descriptor'],methods) if kind=='methods' else fields.get((raw,m['raw_name'],None if ns=='forge-srg' else m['raw_descriptor']),m['raw_name'])
                row['members'][key]=[name,desc(m['descriptor'])];row['owners'][key]=cls(C[role]);row['kinds'][key]=kind
            rows.append(row)
    header=['// Generated from checksum-verified cached game classes.','#pragma once','#include "mappings.hpp"','#include <string_view>',
      'namespace mcov::agent {','struct TridentProfile { const char* id; const char* helperClass; mcov::mappings::Member usingItem,ticks,hand,stack,wet,autoSpin,release,spin; };',
      'inline constexpr TridentProfile kTridentProfiles[]={']
    for row in rows:
        header.append('    {'+','.join(map(json.dumps,[row['id'],row['classes']['helper']]))+','+','.join('{'+','.join(map(json.dumps,row['members'][key]))+'}' for key,_,_,_,_ in S)+'},')
    header += ['};','inline const TridentProfile* TridentMapping(std::string_view id) { for(const auto& p:kTridentProfiles) if(id==p.id) return &p; return nullptr; }','}']
    (ROOT/'src/agent/trident_profiles.hpp').write_text('\n'.join(header)+'\n',encoding='utf-8',newline='\n')
    Path(__file__).with_name('trident_profiles.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(f'Audited {len(rows)} trident profiles / {len(S)} members; Riptide only; normal game release action')

if __name__=='__main__': generate(Path(sys.argv[1]))
