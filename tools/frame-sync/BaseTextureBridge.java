package mcov;

import java.lang.invoke.MethodType;
import java.lang.reflect.*;
import java.util.*;

/** CPU extraction through the game's own model/liquid/block-entity renderers.
 * All entry points run on Minecraft's GL thread. No copied block textures or
 * invented cube models; atlas IDs and UVs belong to the live resource pack.
 */
public final class BaseTextureBridge {
    static final int STRIDE=14, MAX_BLOCKS=16384, MAX_VERTICES=786432;
    private static Symbols symbols;
    private static Object level,manager,pose,random,cameraState;
    private static Object modelShape;
    private static Builder builder;
    private static long requested=Long.MIN_VALUE,serial,nextRefresh,levelReady;
    private static double[] bounds;
    private static int region,x,y,z,scanned,blocks,skipped;
    private static double originX,originY,originZ;
    private static boolean limited;
    private static Mesh completed;
    public static final class Mesh {
        public int[] textures;
        public float[][] vertices;
        public double x,y,z;
        public long serial;
    }
    public static boolean configure(Class<?> client,String profile) throws ReflectiveOperationException {
        String[][] table=BaseTextureProfiles.get(profile);
        if(table==null) return false;
        symbols=new Symbols(client.getClassLoader(),table);
        clear(); return true;
    }
    public static void clear() {
        level=manager=pose=random=cameraState=null; builder=null; completed=null;
        requested=Long.MIN_VALUE; bounds=null; nextRefresh=0;
    }
    public static double[] camera(Object renderer) throws ReflectiveOperationException {
        Object client=symbols.call("instance",null),cam=symbols.call("camera",renderer);
        Object position=symbols.call("cameraPos",cam),delta=symbols.call("delta",client);
        float partial=((Number)symbols.call("partial",delta,true)).floatValue();
        Object lightView=symbols.call("lightView",symbols.call("light",renderer));
        Object target=symbols.call("target",client);
        return new double[]{symbols.number("vecx",position),symbols.number("vecy",position),symbols.number("vecz",position),
            ((Number)symbols.call("yaw",cam)).doubleValue(),((Number)symbols.call("pitch",cam)).doubleValue(),
            ((Number)symbols.call("fov",renderer,cam,partial,true)).doubleValue(),
            ((Number)symbols.call("glId",symbols.call("viewTexture",lightView))).doubleValue(),
            ((Number)symbols.call("glId",symbols.call("targetColour",target))).doubleValue(),
            symbols.number("targetWidth",target),symbols.number("targetHeight",target),partial};
    }
    public static int[] stats() { return new int[]{builder==null?0:1,scanned,blocks,skipped,limited?1:0}; }
    public static Mesh step(Object renderer,long revision,double[] requestedBounds) throws ReflectiveOperationException {
        if(symbols==null||requestedBounds==null||requestedBounds.length==0) { clear(); return null; }
        Object client=symbols.call("instance",null),currentLevel=symbols.get("level",client);
        if(currentLevel==null) { clear(); return null; }
        long now=System.nanoTime();
        Object textures=symbols.call("textures",client);
        if(level!=currentLevel) { clear(); level=currentLevel; levelReady=now+150_000_000L; }
        if(now<levelReady) return null;
        Object dispatcher=symbols.call("blocks",client);
        int atlas=texture(textures,symbols.get("blocksAtlas",null));
        if(manager!=textures) { requested=Long.MIN_VALUE; manager=textures; }
        if(requested!=revision||bounds==null||!Arrays.equals(bounds,requestedBounds)||builder==null&&now>=nextRefresh) {
            requested=revision; bounds=requestedBounds.clone(); completed=requestedBounds.length==0?null:completed;
            originX=Math.floor(bounds[0]); originY=Math.floor(bounds[1]); originZ=Math.floor(bounds[2]);
            builder=new Builder(symbols,manager); region=0; scanned=blocks=skipped=0; limited=false;
            pose=symbols.type("Pose").getConstructor().newInstance(); random=symbols.call("random",null,0L);
            cameraState=symbols.type("CameraState").getConstructor().newInstance(); modelShape=symbols.get("modelShape",null);
            startRegion();
        }
        if(builder!=null) {
            long deadline=now+2_000_000L; int slice=0;
            Object cam=symbols.call("camera",renderer),camPos=symbols.call("cameraPos",cam);
            float partial=((Number)symbols.call("partial",symbols.call("delta",client),true)).floatValue();
            symbols.set("cameraStatePos",cameraState,camPos); symbols.set("cameraStateEntity",cameraState,camPos);
            symbols.set("cameraStateBlock",cameraState,symbols.call("cameraBlock",cam));
            symbols.set("cameraStateOrientation",cameraState,symbols.call("cameraRotation",cam)); symbols.set("cameraStateReady",cameraState,true);
            Object pos=symbols.type("MutablePos").getConstructor().newInstance();
            while(region<bounds.length/6&&scanned<MAX_BLOCKS&&!builder.full()) {
                if(slice++>=32&&System.nanoTime()>=deadline) break;
                int bx=x,by=y,bz=z; advance(); ++scanned;
                symbols.call("posSet",pos,bx,by,bz);
                if(!(Boolean)symbols.call("loaded",level,pos)) continue;
                Object state=symbols.call("state",level,pos);
                if((Boolean)symbols.call("air",state)) continue;
                symbols.call("identity",pose); symbols.call("translate",pose,bx-originX,by-originY,bz-originZ);
                builder.select(atlas,0,0,0);
                if(symbols.call("shape",state)==modelShape) {
                    Object model=symbols.call("model",dispatcher,state);
                    symbols.call("setSeed",random,((Number)symbols.call("seed",state,pos)).longValue());
                    List<Object> parts=new ArrayList<>(); symbols.call("parts",model,random,parts);
                    symbols.call("tessellate",dispatcher,state,pos,level,pose,builder.consumer,false,parts);
                    builder.finish();
                }
                Object fluid=symbols.call("fluidState",state);
                if(!(Boolean)symbols.call("fluidEmpty",fluid)) {
                    builder.select(atlas,(bx&~15)-originX,(by&~15)-originY,(bz&~15)-originZ);
                    symbols.call("liquid",dispatcher,pos,level,builder.consumer,state,fluid); builder.finish();
                }
                Object entity=symbols.call("blockEntity",level,pos);
                if(entity!=null) {
                    Object entityRenderer=symbols.call("entityRenderer",symbols.call("entities",client),entity);
                    if(entityRenderer!=null) {
                        int checkpoint=builder.count();
                        try {
                            Object stateData=symbols.call("newEntityState",entityRenderer);
                            symbols.call("extractEntity",entityRenderer,entity,stateData,partial,camPos,null);
                            symbols.call("identity",pose); symbols.call("translate",pose,bx-originX,by-originY,bz-originZ);
                            symbols.call("submitEntity",entityRenderer,stateData,pose,builder.collector,cameraState); builder.finish();
                        } catch(InvocationTargetException error) { ++skipped; builder.discardPending(); }
                        if(builder.count()==checkpoint) ++skipped;
                    }
                }
                ++blocks;
            }
            if(region>=bounds.length/6||scanned>=MAX_BLOCKS||builder.full()) {
                limited=region<bounds.length/6; completed=builder.mesh(); completed.x=originX; completed.y=originY; completed.z=originZ;
                completed.serial=++serial; builder=null; nextRefresh=System.nanoTime()+1_000_000_000L;
            }
        }
        return completed;
    }
    private static void startRegion() {
        int at=region*6; x=(int)Math.floor(bounds[at]); y=(int)Math.floor(bounds[at+1]); z=(int)Math.floor(bounds[at+2]);
    }
    private static void advance() {
        int at=region*6;
        if(++x>=Math.ceil(bounds[at+3])) { x=(int)Math.floor(bounds[at]);
            if(++z>=Math.ceil(bounds[at+5])) { z=(int)Math.floor(bounds[at+2]);
                if(++y>=Math.ceil(bounds[at+4])) { if(++region<bounds.length/6) startRegion(); }
            }
        }
    }
    private static int texture(Object manager,Object id) throws ReflectiveOperationException {
        Object atlas=symbols.call("getTexture",manager,id);
        return ((Number)symbols.call("glId",symbols.call("gpuTexture",atlas))).intValue();
    }
    static final class Symbols {
        final Map<String,Class<?>> classes=new HashMap<>();
        final Map<String,Method> methods=new HashMap<>(); final Map<String,Field> fields=new HashMap<>();
        Symbols(ClassLoader loader,String[][] table) throws ReflectiveOperationException {
            for(String[] row:table) {
                Class<?> owner=Class.forName(row[1].replace('/','.'),false,loader);
                if(row[0].startsWith("class.")) classes.put(row[0].substring(6),owner);
                else if(row[3].startsWith("(")) {
                    MethodType type=MethodType.fromMethodDescriptorString(row[3],loader);
                    Method method=owner.getDeclaredMethod(row[2],type.parameterArray());
                    if(!Modifier.isPublic(method.getModifiers())||!Modifier.isPublic(owner.getModifiers())) method.setAccessible(true);
                    methods.put(row[0],method);
                } else {
                    Field field=owner.getDeclaredField(row[2]);
                    if(!Modifier.isPublic(field.getModifiers())||!Modifier.isPublic(owner.getModifiers())) field.setAccessible(true);
                    fields.put(row[0],field);
                }
            }
        }
        Class<?> type(String name) { return classes.get(name); }
        Object call(String name,Object owner,Object... args) throws ReflectiveOperationException { return methods.get(name).invoke(owner,args); }
        Object get(String name,Object owner) throws ReflectiveOperationException { return fields.get(name).get(owner); }
        void set(String name,Object owner,Object value) throws ReflectiveOperationException { fields.get(name).set(owner,value); }
        double number(String name,Object owner) throws ReflectiveOperationException { return ((Number)get(name,owner)).doubleValue(); }
    }
    static final class Group {
        float[] data=new float[4096]; int size;
        void add(float[] vertex) { if(size+STRIDE>data.length) data=Arrays.copyOf(data,Math.min(MAX_VERTICES*STRIDE,Math.max(size+STRIDE,data.length*2))); System.arraycopy(vertex,0,data,size,STRIDE); size+=STRIDE; }
    }
    static final class Builder implements InvocationHandler {
        final Symbols s; final Object textureManager,consumer,collector;
        final Map<Integer,Group> groups=new LinkedHashMap<>();
        final float[] vertex=new float[STRIDE],quad=new float[STRIDE*4];
        Group group; int corners,vertices; boolean pending; double dx,dy,dz;
        Builder(Symbols s,Object manager) {
            this.s=s; textureManager=manager;
            consumer=Proxy.newProxyInstance(s.type("Consumer").getClassLoader(),new Class<?>[]{s.type("Consumer")},this);
            collector=Proxy.newProxyInstance(s.type("Collector").getClassLoader(),new Class<?>[]{s.type("Collector"),s.type("Ordered")},this::submit);
        }
        int count() { return vertices; } boolean full() { return vertices+6>MAX_VERTICES; }
        void select(int texture,double x,double y,double z) { finish(); group=groups.computeIfAbsent(texture,key->new Group()); dx=x; dy=y; dz=z; }
        void discardPending() { pending=false; corners=0; }
        void finish() { flushVertex(); if(corners!=0) { corners=0; ++BaseTextureBridge.skipped; } }
        void flushVertex() {
            if(!pending) return; pending=false;
            System.arraycopy(vertex,0,quad,corners++*STRIDE,STRIDE);
            if(corners==4) {
                if(!full()) for(int index:new int[]{0,1,2,0,2,3}) { group.add(Arrays.copyOfRange(quad,index*STRIDE,(index+1)*STRIDE)); ++vertices; }
                corners=0;
            }
        }
        @Override public Object invoke(Object proxy,Method method,Object[] args) throws Throwable {
            if(method.getDeclaringClass()==Object.class) return objectMethod(proxy,method,args);
            if(method.isDefault()) return InvocationHandler.invokeDefault(proxy,method,args);
            if(method.equals(s.methods.get("vertex"))) {
                flushVertex(); Arrays.fill(vertex,0); vertex[0]=((Number)args[0]).floatValue()+(float)dx;
                vertex[1]=((Number)args[1]).floatValue()+(float)dy; vertex[2]=((Number)args[2]).floatValue()+(float)dz;
                vertex[5]=vertex[6]=vertex[7]=vertex[8]=1; vertex[9]=vertex[10]=.96875f; pending=true;
            } else if(method.equals(s.methods.get("colour"))) { for(int i=0;i<4;++i) vertex[5+i]=((Number)args[i]).intValue()/255.f;
            } else if(method.equals(s.methods.get("uv"))) { vertex[3]=((Number)args[0]).floatValue(); vertex[4]=((Number)args[1]).floatValue();
            } else if(method.equals(s.methods.get("uvLight"))) { vertex[9]=(((Number)args[0]).intValue()+8)/256.f; vertex[10]=(((Number)args[1]).intValue()+8)/256.f;
            } else if(method.equals(s.methods.get("normal"))) { for(int i=0;i<3;++i) vertex[11+i]=((Number)args[i]).floatValue();
            } else if(!method.equals(s.methods.get("overlay"))&&!method.equals(s.methods.get("line"))) throw new UnsupportedOperationException("Vertex consumer: "+method);
            return method.getReturnType()==void.class?null:proxy;
        }
        Object submit(Object proxy,Method method,Object[] a) throws Throwable {
            if(method.getDeclaringClass()==Object.class) return objectMethod(proxy,method,a);
            if(method.isDefault()) return InvocationHandler.invokeDefault(proxy,method,a);
            if(method.getReturnType()==s.type("Ordered")) return proxy;
            Class<?>[] p=method.getParameterTypes();
            boolean model=p.length>0&&p[0]==s.type("Model"),part=p.length>0&&p[0]==s.type("Part");
            if(model||part) {
                int spriteAt=model?7:5;
                Object sprite=a[spriteAt];
                if(sprite==null) { ++BaseTextureBridge.skipped; return null; }
                select(texture(textureManager,s.call("spriteAtlas",sprite)),0,0,0);
                Object wrapped=s.call("wrap",sprite,consumer);
                if(model) { s.call("modelAnim",a[0],a[1]); s.call("modelDraw",a[0],a[2],wrapped,a[4],a[5],a[6]); }
                else s.call("partDraw",a[0],a[1],wrapped,a[3],a[4],-1);
                finish(); return null;
            }
            ++BaseTextureBridge.skipped; return null;
        }
        private static Object objectMethod(Object proxy,Method method,Object[] args) {
            return switch(method.getName()) { case "hashCode"->System.identityHashCode(proxy); case "equals"->proxy==args[0]; default->"mcov mesh consumer"; };
        }
        Mesh mesh() {
            finish(); Mesh mesh=new Mesh(); mesh.textures=new int[groups.size()]; mesh.vertices=new float[groups.size()][];
            int i=0; for(var entry:groups.entrySet()) { mesh.textures[i]=entry.getKey(); mesh.vertices[i]=Arrays.copyOf(entry.getValue().data,entry.getValue().size); ++i; } return mesh;
        }
    }
}
