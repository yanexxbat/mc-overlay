"""Audit the 1.21.11 native model/atlas bridge against cached official classes.
No game code or textures are copied into the helper.
"""
from pathlib import Path
import json
import re
import sys
from classfile import NamedJar, type_descriptor
from generate_profiles import namespace_map, namespace_method

C = {
    'Client':'net/minecraft/client/Minecraft', 'Level':'net/minecraft/client/multiplayer/ClientLevel',
    'Pos':'net/minecraft/core/BlockPos', 'MutablePos':'net/minecraft/core/BlockPos$MutableBlockPos',
    'State':'net/minecraft/world/level/block/state/BlockState', 'World':'net/minecraft/world/level/BlockAndTintGetter',
    'Dispatcher':'net/minecraft/client/renderer/block/BlockRenderDispatcher', 'StateModel':'net/minecraft/client/renderer/block/model/BlockStateModel',
    'Random':'net/minecraft/util/RandomSource', 'Pose':'com/mojang/blaze3d/vertex/PoseStack',
    'Consumer':'com/mojang/blaze3d/vertex/VertexConsumer', 'TextureManager':'net/minecraft/client/renderer/texture/TextureManager',
    'Atlas':'net/minecraft/client/renderer/texture/TextureAtlas', 'Sprite':'net/minecraft/client/renderer/texture/TextureAtlasSprite',
    'Texture':'net/minecraft/client/renderer/texture/AbstractTexture', 'GpuTexture':'com/mojang/blaze3d/textures/GpuTexture',
    'TextureView':'com/mojang/blaze3d/textures/GpuTextureView', 'GlTexture':'com/mojang/blaze3d/opengl/GlTexture',
    'Id':'net/minecraft/resources/Identifier', 'Renderer':'net/minecraft/client/renderer/GameRenderer',
    'Camera':'net/minecraft/client/Camera', 'Vec':'net/minecraft/world/phys/Vec3',
    'Light':'net/minecraft/client/renderer/LightTexture', 'Delta':'net/minecraft/client/DeltaTracker',
    'EntityDispatcher':'net/minecraft/client/renderer/blockentity/BlockEntityRenderDispatcher',
    'EntityRenderer':'net/minecraft/client/renderer/blockentity/BlockEntityRenderer', 'Entity':'net/minecraft/world/level/block/entity/BlockEntity',
    'EntityState':'net/minecraft/client/renderer/blockentity/state/BlockEntityRenderState',
    'Collector':'net/minecraft/client/renderer/SubmitNodeCollector', 'Ordered':'net/minecraft/client/renderer/OrderedSubmitNodeCollector',
    'CameraState':'net/minecraft/client/renderer/state/CameraRenderState', 'Model':'net/minecraft/client/model/Model',
    'Part':'net/minecraft/client/model/geom/ModelPart', 'Overlay':'net/minecraft/client/renderer/feature/ModelFeatureRenderer$CrumblingOverlay',
    'Fluid':'net/minecraft/world/level/material/FluidState', 'Shape':'net/minecraft/world/level/block/RenderShape',
    'Target':'com/mojang/blaze3d/pipeline/RenderTarget',
    'RenderType':'net/minecraft/client/renderer/rendertype/RenderType',
}
def t(name): return type_descriptor(C.get(name, name.replace('/', '.')))
def d(args, result): return '('+''.join(map(t,args))+')'+t(result)
S = []
def m(symbol, owner, name, args, result, static=False): S.append((symbol,C[owner],name,d(args,result),'methods',static))
def f(symbol, owner, name, kind, static=False): S.append((symbol,C[owner],name,t(kind),'fields',static))
m('instance','Client','getInstance',[],'Client',True)
f('level','Client','level','Level')
m('blocks','Client','getBlockRenderer',[],'Dispatcher')
m('textures','Client','getTextureManager',[],'TextureManager')
m('entities','Client','getBlockEntityRenderDispatcher',[],'EntityDispatcher')
m('target','Client','getMainRenderTarget',[],'Target')
m('targetColour','Target','getColorTexture',[],'GpuTexture')
f('targetWidth','Target','width','int'); f('targetHeight','Target','height','int')
m('delta','Client','getDeltaTracker',[],'Delta')
m('partial','Delta','getGameTimeDeltaPartialTick',['boolean'],'float')
m('camera','Renderer','getMainCamera',[],'Camera')
m('cameraPos','Camera','position',[],'Vec')
m('cameraRotation','Camera','rotation',[],'org.joml.Quaternionf')
m('cameraBlock','Camera','blockPosition',[],'Pos')
f('cameraStatePos','CameraState','pos','Vec'); f('cameraStateEntity','CameraState','entityPos','Vec')
f('cameraStateBlock','CameraState','blockPos','Pos'); f('cameraStateOrientation','CameraState','orientation','org.joml.Quaternionf')
f('cameraStateReady','CameraState','initialized','boolean')
m('yaw','Camera','yRot',[],'float'); m('pitch','Camera','xRot',[],'float')
for axis in ('x','y','z'): f('vec'+axis,'Vec',axis,'double')
m('fov','Renderer','getFov',['Camera','float','boolean'],'float')
m('light','Renderer','lightTexture',[],'Light')
m('lightView','Light','getTextureView',[],'TextureView')
m('viewTexture','TextureView','texture',[],'GpuTexture')
m('glId','GlTexture','glId',[],'int')
m('getTexture','TextureManager','getTexture',['Id'],'Texture')
m('gpuTexture','Texture','getTexture',[],'GpuTexture')
f('blocksAtlas','Atlas','LOCATION_BLOCKS','Id',True)
m('spriteAtlas','Sprite','atlasLocation',[],'Id')
m('wrap','Sprite','wrap',['Consumer'],'Consumer')
m('posSet','MutablePos','set',['int','int','int'],'MutablePos')
m('loaded','Level','hasChunkAt',['Pos'],'boolean')
m('state','Level','getBlockState',['Pos'],'State')
m('blockEntity','Level','getBlockEntity',['Pos'],'Entity')
m('air','State','isAir',[],'boolean')
m('shape','State','getRenderShape',[],'Shape')
f('modelShape','Shape','MODEL','Shape',True)
m('seed','State','getSeed',['Pos'],'long')
m('model','Dispatcher','getBlockModel',['State'],'StateModel')
m('parts','StateModel','collectParts',['Random','java.util.List'],'void')
m('random','Random','create',['long'],'Random',True)
m('setSeed','Random','setSeed',['long'],'void')
m('tessellate','Dispatcher','renderBatched',['State','Pos','World','Pose','Consumer','boolean','java.util.List'],'void')
m('fluidState','State','getFluidState',[],'Fluid')
m('fluidEmpty','Fluid','isEmpty',[],'boolean')
m('liquid','Dispatcher','renderLiquid',['Pos','World','Consumer','State','Fluid'],'void')
m('identity','Pose','setIdentity',[],'void')
m('translate','Pose','translate',['double','double','double'],'void')
m('posePush','Pose','pushPose',[],'void'); m('posePop','Pose','popPose',[],'void')
m('vertex','Consumer','addVertex',['float','float','float'],'Consumer')
m('colour','Consumer','setColor',['int','int','int','int'],'Consumer')
m('colourPacked','Consumer','setColor',['int'],'Consumer')
m('uv','Consumer','setUv',['float','float'],'Consumer')
m('overlay','Consumer','setUv1',['int','int'],'Consumer')
m('uvLight','Consumer','setUv2',['int','int'],'Consumer')
m('normal','Consumer','setNormal',['float','float','float'],'Consumer')
m('line','Consumer','setLineWidth',['float'],'Consumer')
m('entityRenderer','EntityDispatcher','getRenderer',['Entity'],'EntityRenderer')
m('newEntityState','EntityRenderer','createRenderState',[],'EntityState')
m('extractEntity','EntityRenderer','extractRenderState',['Entity','EntityState','float','Vec','Overlay'],'void')
m('submitEntity','EntityRenderer','submit',['EntityState','Pose','Collector','CameraState'],'void')
m('submitModel','Ordered','submitModel',['Model','java.lang.Object','Pose','RenderType','int','int','int','Sprite','int','Overlay'],'void')
m('submitPart','Ordered','submitModelPart',['Part','Pose','RenderType','int','int','Sprite','boolean','boolean','int','Overlay','int'],'void')
m('order','Collector','order',['int'],'Ordered')
m('modelAnim','Model','setupAnim',['java.lang.Object'],'void')
m('modelDraw','Model','renderToBuffer',['Pose','Consumer','int','int','int'],'void')
m('partDraw','Part','render',['Pose','Consumer','int','int','int'],'void')

def generate(cache, target):
    jar=NamedJar(cache,'1.21.11')
    abstract={(member['name'],member['descriptor']) for _,member in jar.members(C['Consumer'],'methods') if member['flags']&0x400}
    supplied={(name,descriptor) for _,owner,name,descriptor,kind,_ in S if owner==C['Consumer'] and kind=='methods'}
    missing=abstract-supplied
    if missing: raise ValueError(f'Unhandled abstract vertex operations: {sorted(missing)}')
    resolved=[]
    for symbol,owner,name,desc,kind,static in S:
        matches=[(declaring,member) for declaring,member in jar.members(owner,kind) if member['name']==name and member['descriptor']==desc]
        if not matches: raise ValueError(f'Missing {symbol}: {owner}.{name}{desc}')
        declaring,member=matches[0]
        if bool(member['flags']&8)!=static: raise ValueError(f'Wrong static flag: {symbol}')
        resolved.append((symbol,declaring,member,kind))
    profiles=[]
    for namespace in ('mojmap','vanilla','intermediary'):
        classes,fields,methods=namespace_map(cache,'1.21.11',namespace)
        def cls(name):
            raw=jar.mapping.get(name,{}).get('obf',name)
            return raw if namespace=='vanilla' else classes.get(raw,raw) if namespace=='intermediary' else name
        def desc(value): return re.sub(r'L([^;]+);',lambda match:'L'+cls(match[1])+';',value)
        rows=[['class.'+alias,cls(owner),'',''] for alias,owner in C.items()]
        for symbol,owner,member,kind in resolved:
            rawOwner=jar.mapping.get(owner,{}).get('obf',owner)
            name=member['name']
            if namespace=='vanilla': name=member['raw_name']
            elif namespace=='intermediary':
                name=namespace_method(jar,owner,member['raw_name'],member['raw_descriptor'],methods) if kind=='methods' else fields.get((rawOwner,member['raw_name'],member['raw_descriptor']),member['raw_name'])
            rows.append([symbol,cls(owner),name,desc(member['descriptor'])])
        profiles.append(rows)
    quote=lambda value:json.dumps(value)
    text='// Generated from audited 1.21.11 classes and mappings.\npackage mcov;\nfinal class BaseTextureProfiles {\n'
    for i,rows in enumerate(profiles):
        text+=f'    private static final String[][] P{i}={{\n'
        text+=''.join('        {'+','.join(map(quote,row))+'},\n' for row in rows)+'    };\n'
    text+='    static String[][] get(String profile) {\n        if(profile.equals("mojmap-1.21.11")) return P0;\n        if(profile.equals("vanilla-1.21.11")) return P1;\n        if(profile.equals("intermediary-1.21.11")) return P2;\n        return null;\n    }\n}\n'
    target.write_text(text,encoding='utf-8',newline='\n')
    print(f'Audited {len(C)} classes / {len(S)} members for three 1.21.11 namespaces')
if __name__=='__main__': generate(Path(sys.argv[1]),Path(sys.argv[2]))
