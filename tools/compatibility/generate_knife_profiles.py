"""Audit the optional first-person knife bridge against cached 1.21.11 classes."""
from pathlib import Path
import json,re,sys
from classfile import NamedJar,type_descriptor
from generate_profiles import namespace_map,namespace_method
C={
 'Client':'net/minecraft/client/Minecraft','Hand':'net/minecraft/client/renderer/ItemInHandRenderer',
 'Living':'net/minecraft/world/entity/LivingEntity','Local':'net/minecraft/client/player/LocalPlayer',
 'Player':'net/minecraft/world/entity/player/Player','Inventory':'net/minecraft/world/entity/player/Inventory',
 'Stack':'net/minecraft/world/item/ItemStack','Item':'net/minecraft/world/item/Item',
 'Context':'net/minecraft/world/item/ItemDisplayContext','Pose':'com/mojang/blaze3d/vertex/PoseStack',
 'Entry':'com/mojang/blaze3d/vertex/PoseStack$Pose','Consumer':'com/mojang/blaze3d/vertex/VertexConsumer',
 'Collector':'net/minecraft/client/renderer/SubmitNodeCollector','Ordered':'net/minecraft/client/renderer/OrderedSubmitNodeCollector',
 'Custom':'net/minecraft/client/renderer/SubmitNodeCollector$CustomGeometryRenderer',
 'Types':'net/minecraft/client/renderer/rendertype/RenderTypes','Type':'net/minecraft/client/renderer/rendertype/RenderType',
 'Screen':'net/minecraft/client/gui/screens/Screen','Level':'net/minecraft/client/multiplayer/ClientLevel',
 'Arm':'net/minecraft/world/entity/HumanoidArm',
}
def t(n): return type_descriptor(C.get(n,n))
def d(args,out): return '('+''.join(map(t,args))+')'+t(out)
S=[]
def m(key,owner,name,args,out,static=False): S.append((key,C[owner],name,d(args,out),'methods',static))
def f(key,owner,name,kind): S.append((key,C[owner],name,t(kind),'fields',False))
m('instance','Client','getInstance',[],'Client',True)
f('player','Client','player','Local'); f('screen','Client','screen','Screen'); f('level','Client','level','Level')
f('mainItem','Hand','mainHandItem','Stack')
m('hook','Hand','renderItem',['Living','Stack','Context','Pose','Collector','int'],'void')
m('marker','Hand','tick',[],'void')
m('main','Living','getMainHandItem',[],'Stack'); m('attack','Living','getAttackAnim',['float'],'float')
m('using','Living','isUsingItem',[],'boolean'); m('arm','Living','getMainArm',[],'Arm')
m('inventory','Player','getInventory',[],'Inventory'); m('slot','Inventory','getSelectedSlot',[],'int')
m('empty','Stack','isEmpty',[],'boolean'); m('item','Stack','getItem',[],'Item'); m('name','Item','getDescriptionId',[],'java.lang.String')
m('last','Pose','last',[],'Entry'); m('matrix','Entry','pose',[],'org.joml.Matrix4f')
m('order','Collector','order',['int'],'Ordered'); m('submit','Ordered','submitCustomGeometry',['Pose','Type','Custom'],'void')
m('quads','Types','debugQuads',[],'Type',True)
m('vertex','Consumer','addVertex',['float','float','float'],'Consumer'); m('colour','Consumer','setColor',['int'],'Consumer')
def generate(cache,target):
 jar=NamedJar(cache,'1.21.11'); resolved=[]
 for key,owner,name,desc,kind,static in S:
  found=[(a,b) for a,b in jar.members(owner,kind) if b['name']==name and b['descriptor']==desc]
  if not found: raise ValueError(f'Missing {key}: {owner}.{name}{desc}')
  a,b=found[0]
  if bool(b['flags']&8)!=static: raise ValueError('Static mismatch '+key)
  resolved.append((key,a,b,kind))
 profiles=[]
 for namespace in ('mojmap','vanilla','intermediary'):
  classes,fields,methods=namespace_map(cache,'1.21.11',namespace)
  def cls(name):
   obf=jar.mapping.get(name,{}).get('obf',name)
   return obf if namespace=='vanilla' else classes.get(obf,obf) if namespace=='intermediary' else name
  def desc(value): return re.sub(r'L([^;]+);',lambda x:'L'+cls(x[1])+';',value)
  rows=[['class.'+key,cls(owner),'',''] for key,owner in C.items()]
  for key,owner,member,kind in resolved:
   raw=jar.mapping.get(owner,{}).get('obf',owner); name=member['name']
   if namespace=='vanilla': name=member['raw_name']
   elif namespace=='intermediary':
    name=namespace_method(jar,owner,member['raw_name'],member['raw_descriptor'],methods) if kind=='methods' else fields.get((raw,member['raw_name'],member['raw_descriptor']),member['raw_name'])
   rows.append([key,cls(owner),name,desc(member['descriptor'])])
  profiles.append(rows)
 text='// Generated from audited 1.21.11 signatures.\npackage mcov;\npublic final class KnifeProfiles {\n'
 for i,rows in enumerate(profiles):
  text+=f'    private static final String[][] P{i}={{\n'+''.join('        {'+','.join(map(json.dumps,row))+'},\n' for row in rows)+'    };\n'
 text+='    public static String[][] get(String id) {\n'
 for i,ns in enumerate(('mojmap','vanilla','intermediary')): text+=f'        if(id.equals("{ns}-1.21.11")) return P{i};\n'
 text+='        return null;\n    }\n'
 text+='    public static String[][][] all() { return new String[][][]{P0,P1,P2}; }\n'
 text+='    public static String[] row(String[][] p,String key) { for(String[] r:p) if(r[0].equals(key)) return r; throw new IllegalArgumentException(key); }\n'
 text+='    public static boolean target(String name) { for(String[][] p:all()) if(row(p,"class.Hand")[1].equals(name)) return true; return false; }\n}\n'
 target.write_text(text,encoding='utf-8',newline='\n'); print(f'Audited {len(C)} knife classes / {len(S)} members for three namespaces')
if __name__=='__main__': generate(Path(sys.argv[1]),Path(sys.argv[2]))
