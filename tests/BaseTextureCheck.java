package mcov;
import java.lang.reflect.*;
public class BaseTextureCheck {
    public interface Consumer {
        Consumer vertex(float x,float y,float z); Consumer colour(int r,int g,int b,int a);
        Consumer colour(int rgba);
        Consumer uv(float u,float v); Consumer overlay(int x,int y); Consumer light(int x,int y); Consumer normal(float x,float y,float z); Consumer line(float width);
        default void packed(float x,float y,float z,int rgba,float u,float v,int light) {
            vertex(x,y,z).colour(rgba).uv(u,v).light(light&65535,light>>>16).normal(0,1,0);
        }
    }
    public interface Ordered {
        void model(Model model,Object state,Pose pose,Object renderType,int light,int overlay,int colour,Sprite sprite,int outline,Object crumble);
        default void model(Model model,Object state,Pose pose,int light,Sprite sprite) { model(model,state,pose,null,light,0,-1,sprite,0,null); }
    }
    public interface Collector extends Ordered { Ordered order(int order); }
    public static class Pose {}
    public static class Sprite {
        public String atlas() { return "atlas"; }
        public Consumer wrap(Consumer c) {
            return (Consumer)Proxy.newProxyInstance(Consumer.class.getClassLoader(),new Class<?>[]{Consumer.class},(proxy,method,args)->{
                if(method.isDefault()) return InvocationHandler.invokeDefault(proxy,method,args);
                if(method.getName().equals("uv")) args=new Object[]{.25f+.5f*(Float)args[0],.125f+.25f*(Float)args[1]};
                method.invoke(c,args); return method.getReturnType()==Consumer.class?proxy:null;
            });
        }
    }
    public static class Texture { public Texture gpu() { return this; } public int id() { return 7; } }
    public static class Manager { public Texture texture(String key) { return new Texture(); } }
    public static class Model {
        boolean animated;
        public void animate(Object state) { animated=true; }
        public void draw(Pose pose,Consumer c,int light,int overlay,int colour) {
            for(float[] v:new float[][]{{0,0},{1,0},{1,1},{0,1}}) c.packed(v[0],v[1],2,0xff804020,v[0],v[1],light);
        }
    }
    static void check(boolean yes,String why) { if(!yes) throw new AssertionError(why); }
    static String[] type(String alias,Class<?> cls) { return new String[]{"class."+alias,cls.getName(),"",""}; }
    static String[] method(String alias,Class<?> cls,String name,String descriptor) { return new String[]{alias,cls.getName(),name,descriptor}; }
    public static void main(String[] args) throws Throwable {
        String c="Lmcov/BaseTextureCheck$Consumer;",p="Lmcov/BaseTextureCheck$Pose;",t="Lmcov/BaseTextureCheck$Texture;";
        BaseTextureBridge.Symbols s=new BaseTextureBridge.Symbols(BaseTextureCheck.class.getClassLoader(),new String[][]{
            type("Consumer",Consumer.class),type("Collector",Collector.class),type("Ordered",Ordered.class),type("Model",Model.class),type("Part",Pose.class),
            method("vertex",Consumer.class,"vertex","(FFF)"+c),method("colour",Consumer.class,"colour","(IIII)"+c),method("uv",Consumer.class,"uv","(FF)"+c),
            method("colourPacked",Consumer.class,"colour","(I)"+c),
            method("overlay",Consumer.class,"overlay","(II)"+c),method("uvLight",Consumer.class,"light","(II)"+c),method("normal",Consumer.class,"normal","(FFF)"+c),method("line",Consumer.class,"line","(F)"+c),
            method("spriteAtlas",Sprite.class,"atlas","()Ljava/lang/String;"),method("wrap",Sprite.class,"wrap","("+c+")"+c),
            method("getTexture",Manager.class,"texture","(Ljava/lang/String;)"+t),method("gpuTexture",Texture.class,"gpu","()"+t),method("glId",Texture.class,"id","()I"),
            method("modelAnim",Model.class,"animate","(Ljava/lang/Object;)V"),method("modelDraw",Model.class,"draw","("+p+c+"III)V")});
        BaseTextureBridge.Builder b=new BaseTextureBridge.Builder(s,new Manager()); b.select(7,10,20,30);
        Consumer consumer=(Consumer)b.consumer;
        for(float[] v:new float[][]{{0,0},{1,0},{1,1},{0,1}}) consumer.packed(v[0],v[1],2,0xff804020,v[0],v[1],0x00f00080);
        b.finish(); BaseTextureBridge.Mesh mesh=b.mesh(); float[] data=mesh.vertices[0];
        check(data.length==84&&data[0]==10&&data[1]==20&&data[2]==32,"Quad triangulation or liquid offset broken");
        check(data[5]==128/255.f&&data[6]==64/255.f&&data[7]==32/255.f&&data[8]==1,"Native model tint lost");
        check(data[9]==136/256.f&&data[10]==248/256.f,"Packed lightmap coordinates wrong");
        check(data[14]==11&&data[28]==11&&data[42]==10&&data[70]==10,"Triangle winding wrong");
        b=new BaseTextureBridge.Builder(s,new Manager()); b.select(7,0,0,0); consumer=(Consumer)b.consumer;
        for(float[] v:new float[][]{{0,0},{1,0},{1,1},{0,1}}) consumer.packed(v[0],v[1],0,0x80336699,v[0],v[1],0x00f000f0);
        b.finish(); data=b.mesh().vertices[0];
        check(data[5]==51/255.f&&data[6]==102/255.f&&data[7]==153/255.f&&data[8]==128/255.f,"Packed ARGB swapped channels or lost alpha");
        b=new BaseTextureBridge.Builder(s,new Manager()); Model model=new Model();
        ((Collector)b.collector).order(2).model(model,new Object(),new Pose(),0x00f000f0,new Sprite());
        mesh=b.mesh(); data=mesh.vertices[0];
        check(model.animated&&mesh.textures[0]==7&&data.length==84,"Block-entity model not invoked");
        check(data[3]==.25f&&data[4]==.125f&&data[14+3]==.75f,"Block-entity sprite UV wrapping lost");
        check(BaseTextureProfiles.get("other")==null&&BaseTextureProfiles.get("vanilla-1.21.11").length>100,"Version gate lost");
        // The two features share implementation, never mutable render state.
        Field baseField=BaseTextureBridge.class.getDeclaredField("base"),storageField=BaseTextureBridge.class.getDeclaredField("storage");
        baseField.setAccessible(true); storageField.setAccessible(true);
        Object base=baseField.get(null),storage=storageField.get(null);
        Field completed=base.getClass().getDeclaredField("completed"); completed.setAccessible(true);
        completed.set(base,mesh); completed.set(storage,new BaseTextureBridge.Mesh());
        BaseTextureBridge.storageClear();
        check(completed.get(base)==mesh&&completed.get(storage)==null,"Clearing storage also cleared Base ESP");
        completed.set(storage,mesh); BaseTextureBridge.clear();
        check(completed.get(base)==null&&completed.get(storage)==mesh,"Clearing Base ESP also cleared storage");
        BaseTextureBridge.storageClear();
        b=new BaseTextureBridge.Builder(s,new Manager()); b.select(7,0,0,0);
        ((Consumer)b.consumer).vertex(0,0,0); b.finish();
        check(b.skipped==1&&BaseTextureBridge.stats()[3]==0&&BaseTextureBridge.storageStats()[3]==0,"Capture statistics leaked between renderers");
        System.out.println("PASS model capture: defaults, quads, tint, lightmap, offsets, animation and atlas sprites");
    }
}
