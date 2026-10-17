"""Audit Anti Vanish roster, packet and sensor access against cached 1.21.11 classes."""
from pathlib import Path
import json,re,sys
from classfile import NamedJar,type_descriptor
from generate_profiles import namespace_map,namespace_method
C={
 'Client':'net/minecraft/client/Minecraft','Level':'net/minecraft/client/multiplayer/ClientLevel',
 'Local':'net/minecraft/client/player/LocalPlayer','Player':'net/minecraft/world/entity/player/Player',
 'Entity':'net/minecraft/world/entity/Entity','Connection':'net/minecraft/client/multiplayer/ClientPacketListener',
 'Info':'net/minecraft/client/multiplayer/PlayerInfo',
}

def t(n): return '['+t(n[:-2]) if n.endswith('[]') else type_descriptor(C.get(n,n))
def d(args,out): return '('+''.join(map(t,args))+')'+t(out)
S=[]
def m(key,owner,name,args,out,static=False): S.append((key,C[owner],name,d(args,out),'methods',static))
def f(key,owner,name,kind,static=False): S.append((key,C[owner],name,t(kind),'fields',static))
m('instance','Client','getInstance',[],'Client',True)
f('player','Client','player','Local');f('level','Client','level','Level')
m('connection','Client','getConnection',[],'Connection')
m('online','Connection','getOnlinePlayers',[],'java.util.Collection')
m('listed','Connection','getListedOnlinePlayers',[],'java.util.Collection')
m('profile','Info','getProfile',[],'com.mojang.authlib.GameProfile')
m('players','Level','players',[],'java.util.List')
m('playerProfile','Player','getGameProfile',[],'com.mojang.authlib.GameProfile')
m('uuid','Entity','getUUID',[],'java.util.UUID')
m('invisible','Entity','isInvisible',[],'boolean')
m('distance','Entity','distanceToSqr',['Entity'],'double')

# Packet observation and environmental checks, audited alongside the roster API.
C.update({
 'Bundle':'net/minecraft/network/protocol/BundlePacket',
 'Network':'net/minecraft/network/Connection','Packet':'net/minecraft/network/protocol/Packet',
 'Remove':'net/minecraft/network/protocol/game/ClientboundPlayerInfoRemovePacket',
 'Update':'net/minecraft/network/protocol/game/ClientboundPlayerInfoUpdatePacket',
 'Entry':'net/minecraft/network/protocol/game/ClientboundPlayerInfoUpdatePacket$Entry',
 'Action':'net/minecraft/network/protocol/game/ClientboundPlayerInfoUpdatePacket$Action',
 'Suggestions':'net/minecraft/network/protocol/game/ClientboundCommandSuggestionsPacket',
 'Suggestion':'net/minecraft/network/protocol/game/ClientboundCommandSuggestionsPacket$Entry',
 'Probe':'net/minecraft/network/protocol/game/ServerboundCommandSuggestionPacket',
 'Chat':'net/minecraft/network/protocol/game/ClientboundSystemChatPacket',
 'Sound':'net/minecraft/network/protocol/game/ClientboundSoundPacket',
 'EntitySound':'net/minecraft/network/protocol/game/ClientboundSoundEntityPacket',
 'Particles':'net/minecraft/network/protocol/game/ClientboundLevelParticlesPacket',
 'Explosion':'net/minecraft/network/protocol/game/ClientboundExplodePacket',
 'UseOn':'net/minecraft/network/protocol/game/ServerboundUseItemOnPacket',
 'Use':'net/minecraft/network/protocol/game/ServerboundUseItemPacket',
 'Dig':'net/minecraft/network/protocol/game/ServerboundPlayerActionPacket',
 'Interact':'net/minecraft/network/protocol/game/ServerboundInteractPacket',
 'Swing':'net/minecraft/network/protocol/game/ServerboundSwingPacket',
 'Component':'net/minecraft/network/chat/Component','Contents':'net/minecraft/network/chat/ComponentContents',
 'Translated':'net/minecraft/network/chat/contents/TranslatableContents',
 'Holder':'net/minecraft/core/Holder','SoundEvent':'net/minecraft/sounds/SoundEvent',
 'ParticleOptions':'net/minecraft/core/particles/ParticleOptions','ParticleType':'net/minecraft/core/particles/ParticleType',
 'Registries':'net/minecraft/core/registries/BuiltInRegistries','Registry':'net/minecraft/core/Registry',
 'Id':'net/minecraft/resources/Identifier','GameMode':'net/minecraft/world/level/GameType',
 'Vec':'net/minecraft/world/phys/Vec3','Pos':'net/minecraft/core/BlockPos',
 'State':'net/minecraft/world/level/block/state/BlockState','Block':'net/minecraft/world/level/block/Block',
 'Projectile':'net/minecraft/world/entity/projectile/Projectile','Villager':'net/minecraft/world/entity/npc/villager/Villager',
 'Hit':'net/minecraft/world/phys/BlockHitResult','Hand':'net/minecraft/world/InteractionHand',
 'ItemStack':'net/minecraft/world/item/ItemStack','Item':'net/minecraft/world/item/Item',
 'Properties':'net/minecraft/world/level/block/state/properties/BlockStateProperties',
 'Property':'net/minecraft/world/level/block/state/properties/Property',
 'BoolProperty':'net/minecraft/world/level/block/state/properties/BooleanProperty',
 'Menu':'net/minecraft/world/inventory/AbstractContainerMenu','InventoryMenu':'net/minecraft/world/inventory/InventoryMenu',
})
m('bundle','Bundle','subPackets',[],'java.lang.Iterable')
m('network','Connection','getConnection',[],'Network')
m('packetIn','Network','channelRead0',['io.netty.channel.ChannelHandlerContext','Packet'],'void')
m('packetOut','Network','send',['Packet'],'void')
m('networkMarker','Network','getRemoteAddress',[],'java.net.SocketAddress')
m('send','Connection','send',['Packet'],'void')
m('removed','Remove','profileIds',[],'java.util.List')
m('actions','Update','actions',[],'java.util.EnumSet');m('entries','Update','entries',[],'java.util.List')
m('entryId','Entry','profileId',[],'java.util.UUID');m('entryListed','Entry','listed',[],'boolean');m('entryMode','Entry','gameMode',[],'GameMode')
for key in ['ADD_PLAYER','UPDATE_LISTED','UPDATE_GAME_MODE']: f(key,'Action',key,'Action',True)
m('mode','Info','getGameMode',[],'GameMode');m('modeName','GameMode','getName',[],'java.lang.String')
m('suggestionId','Suggestions','id',[],'int');m('suggestions','Suggestions','suggestions',[],'java.util.List');m('suggestionText','Suggestion','text',[],'java.lang.String')
m('chat','Chat','content',[],'Component');m('text','Component','getString',[],'java.lang.String')
m('contents','Component','getContents',[],'Contents');m('translationKey','Translated','getKey',[],'java.lang.String');m('translationArgs','Translated','getArgs',[],'java.lang.Object[]')
m('sound','Sound','getSound',[],'Holder');m('entitySound','EntitySound','getSound',[],'Holder');m('soundEntity','EntitySound','getId',[],'int')
m('holder','Holder','value',[],'java.lang.Object');m('soundId','SoundEvent','location',[],'Id')
for owner in ['Sound','Particles']:
 for axis in ['X','Y','Z']: m(owner.lower()+axis,owner,'get'+axis,[],'double')
m('particle','Particles','getParticle',[],'ParticleOptions');m('particleType','ParticleOptions','getType',[],'ParticleType')
f('particleRegistry','Registries','PARTICLE_TYPE','Registry',True);m('registryKey','Registry','getKey',['java.lang.Object'],'Id')
m('explosionPos','Explosion','center',[],'Vec');m('explosionRadius','Explosion','radius',[],'float')
for axis in ['X','Y','Z']: m('entity'+axis,'Entity','get'+axis,[],'double');f('vec'+axis,'Vec',axis.lower(),'double')
m('entityById','Level','getEntity',['int'],'Entity');m('entities','Level','entitiesForRendering',[],'java.lang.Iterable')
m('state','Level','getBlockState',['Pos'],'State');m('block','State','getBlock',[],'Block');m('blockKey','Block','getDescriptionId',[],'java.lang.String')
m('loaded','Level','isLoaded',['Pos'],'boolean');m('powered','Level','hasNeighborSignal',['Pos'],'boolean')
f('poweredProperty','Properties','POWERED','BoolProperty',True);m('hasProperty','State','hasProperty',['Property'],'boolean');m('property','State','getValue',['Property'],'java.lang.Comparable')
m('hit','UseOn','getHitResult',[],'Hit');m('hitPos','Hit','getBlockPos',[],'Pos');m('hand','UseOn','getHand',[],'Hand')
for axis in ['X','Y','Z']:m('pos'+axis,'Pos','get'+axis,[],'int')
m('held','Player','getItemInHand',['Hand'],'ItemStack');m('item','ItemStack','getItem',[],'Item');m('itemKey','Item','getDescriptionId',[],'java.lang.String')
f('menu','Player','containerMenu','Menu');f('inventoryMenu','Player','inventoryMenu','InventoryMenu')

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
 text='// Generated from audited 1.21.11 signatures.\npackage mcov;\npublic final class AntiVanishProfiles {\n'
 for i,rows in enumerate(profiles):
  text+=f'    private static final String[][] P{i}={{\n'+''.join('        {'+','.join(map(json.dumps,row))+'},\n' for row in rows)+'    };\n'
 text+='    public static String[][] get(String id) {\n'
 for i,ns in enumerate(('mojmap','vanilla','intermediary')): text+=f'        if(id.equals("{ns}-1.21.11")) return P{i};\n'
 text+='        return null;\n    }\n'
 text+='    public static String[][][] all() { return new String[][][]{P0,P1,P2}; }\n'
 text+='    public static String[] row(String[][] p,String key) { for(String[] r:p) if(r[0].equals(key)) return r; throw new IllegalArgumentException(key); }\n'
 text+='}\n'
 target.write_text(text,encoding='utf-8',newline='\n');print(f'Audited {len(C)} Anti Vanish classes / {len(S)} members for three namespaces')
if __name__=='__main__':generate(Path(sys.argv[1]),Path(sys.argv[2]))
