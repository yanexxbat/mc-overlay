package mcov;

import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Exercises the production render bridge against an isolated game API fixture. */
public final class KnifeCheck {
    public enum Arm { RIGHT,LEFT }
    public enum Context { RIGHT,LEFT,OTHER }
    public static final class Item { String id="item.minecraft.diamond_sword"; public String name() { return id; } }
    public static final class Stack { final Item kind=new Item(); public Item item() { return kind; } public boolean empty() { return false; } }
    public static final class Inventory { int slot; public int slot() { return slot; } }
    public static final class Player {
        final Stack held=new Stack(); final Inventory inventory=new Inventory(); float attack; boolean using; Arm arm=Arm.RIGHT;
        public Stack main() { return held; } public Inventory inventory() { return inventory; }
        public float attack(float partial) { return attack; } public boolean using() { return using; } public Arm arm() { return arm; }
    }
    public static final class Client {
        static final Client INSTANCE=new Client(); public Player player=new Player(); public Object level=new Object(),screen;
        public static Client instance() { return INSTANCE; }
    }
    public static final class Entry { final org.joml.Matrix4f matrix=new org.joml.Matrix4f(); public org.joml.Matrix4f matrix() { return matrix; } }
    public static final class Pose { final Entry entry=new Entry(); public Entry last() { return entry; } }
    public interface Custom { void draw(Entry pose,Consumer out); }
    public interface Consumer { Consumer vertex(float x,float y,float z); Consumer colour(int colour); }
    public static final class Types {
        public static final Arm RIGHT=Arm.RIGHT;
        public static final Context LEFT=Context.LEFT,MAIN=Context.RIGHT;
        public static Object quads() { return "quads"; }
    }
    public static final class Snippet {}
    public static final class Pipeline {
        public static final Snippet SNIPPET=new Snippet();
        public static final String BLEND="additive",FORMAT="position_colour",MODE="quads",MAIN="main";
        boolean cull=true,depth=true;String blend,format,mode,location;
        public static Pipeline builder(Snippet[] snippets) { check(snippets.length==1&&snippets[0]==SNIPPET,"Missing glow shader snippet");return new Pipeline(); }
        public Pipeline location(String s) { location=s;return this; }
        public Pipeline blend(String s) { blend=s;return this; }
        public Pipeline cull(boolean value) { cull=value;return this; }
        public Pipeline depth(boolean value) { depth=value;return this; }
        public Pipeline format(String f,String m) { format=f;mode=m;return this; }
        public Pipeline build() { return this; }
    }
    public static final class Setup {
        final Pipeline pipeline;String output;
        Setup(Pipeline p) { pipeline=p; }
        public static Setup builder(Pipeline p) { return new Setup(p); }
        public Setup output(String s) { output=s;return this; }
        public Setup build() { return this; }
        public static Object create(String name,Setup setup) {
            check(name.equals("mcov_knife_glow")&&!setup.pipeline.cull&&!setup.pipeline.depth&&"additive".equals(setup.pipeline.blend)&&"main".equals(setup.output),"Glow pipeline has incorrect blend/depth/output");return setup;
        }
    }
    public static final class Collector implements Consumer {
        final List<Runnable> deferred=new ArrayList<>(); final List<Float> xyz=new ArrayList<>(); final List<Object> types=new ArrayList<>();final List<Integer> colours=new ArrayList<>();
        public Collector order(int value) { return this; }
        public void submit(Pose pose,Object type,Custom geometry) { types.add(type);deferred.add(()->geometry.draw(pose.entry,this)); }
        public Consumer vertex(float x,float y,float z) { check(Float.isFinite(x+y+z),"Nonfinite emitted vertex"); xyz.add(x); xyz.add(y); xyz.add(z); return this; }
        public Consumer colour(int colour) { colours.add(colour);return this; }
        void flush() { var copy=new ArrayList<>(deferred); deferred.clear(); for(var command:copy) command.run(); }
    }
    static void check(boolean yes,String message) { if(!yes) throw new AssertionError(message); }
    static String desc(Class<?> c) { return c.descriptorString(); }
    static String[] type(String name,Class<?> c) { return new String[]{"class."+name,c.getName(),"",""}; }
    static String[] method(String alias,Class<?> c,String name,Class<?> out,Class<?>... args) {
        return new String[]{alias,c.getName(),name,"("+String.join("",Arrays.stream(args).map(KnifeCheck::desc).toList())+")"+desc(out)};
    }
    static String[] field(String alias,Class<?> c,String name,Class<?> value) { return new String[]{alias,c.getName(),name,desc(value)}; }
    static void set(String name,Object value) throws Exception { Field f=KnifeBridge.class.getDeclaredField(name); f.setAccessible(true); f.set(null,value); }
    static Object get(String name) throws Exception { Field f=KnifeBridge.class.getDeclaredField(name); f.setAccessible(true); return f.get(null); }
    static void update(boolean enabled,int skin,boolean held,long serial) { KnifeBridge.update(enabled,skin,1,1,.14f,false,true,held,serial,500,true,1.2f,1,0x69ddd5); }
    static boolean render(Player p,Context context,Collector collector) { return KnifeBridge.render(new Object(),p,p.held,context,new Pose(),collector,0xf000f0); }
    public static void main(String[] args) throws Exception {
        String[][] table={type("Custom",Custom.class),type("Pose",Pose.class),type("Snippet",Snippet.class),
            field("glowSnippet",Pipeline.class,"SNIPPET",Snippet.class),field("glowBlend",Pipeline.class,"BLEND",String.class),
            field("colourFormat",Pipeline.class,"FORMAT",String.class),field("quadMode",Pipeline.class,"MODE",String.class),field("mainOutput",Pipeline.class,"MAIN",String.class),
            method("pipelineBuilder",Pipeline.class,"builder",Pipeline.class,Snippet[].class),method("pipelineLocation",Pipeline.class,"location",Pipeline.class,String.class),
            method("pipelineBlend",Pipeline.class,"blend",Pipeline.class,String.class),method("pipelineCull",Pipeline.class,"cull",Pipeline.class,boolean.class),
            method("pipelineDepth",Pipeline.class,"depth",Pipeline.class,boolean.class),method("pipelineFormat",Pipeline.class,"format",Pipeline.class,String.class,String.class),
            method("pipelineBuild",Pipeline.class,"build",Pipeline.class),method("setupBuilder",Setup.class,"builder",Setup.class,Pipeline.class),
            method("setupOutput",Setup.class,"output",Setup.class,String.class),method("setupBuild",Setup.class,"build",Setup.class),method("renderType",Setup.class,"create",Object.class,String.class,Setup.class),
            method("instance",Client.class,"instance",Client.class),field("player",Client.class,"player",Player.class),
            field("level",Client.class,"level",Object.class),field("screen",Client.class,"screen",Object.class),
            method("main",Player.class,"main",Stack.class),method("inventory",Player.class,"inventory",Inventory.class),method("slot",Inventory.class,"slot",int.class),
            method("using",Player.class,"using",boolean.class),method("attack",Player.class,"attack",float.class,float.class),method("arm",Player.class,"arm",Arm.class),
            method("empty",Stack.class,"empty",boolean.class),method("item",Stack.class,"item",Item.class),method("name",Item.class,"name",String.class),
            field("firstLeft",Types.class,"LEFT",Context.class),field("firstRight",Types.class,"MAIN",Context.class),field("rightArm",Types.class,"RIGHT",Arm.class),
            method("last",Pose.class,"last",Entry.class),method("matrix",Entry.class,"matrix",org.joml.Matrix4f.class),
            method("order",Collector.class,"order",Collector.class,int.class),method("submit",Collector.class,"submit",void.class,Pose.class,Object.class,Custom.class),
            method("quads",Types.class,"quads",Object.class),method("vertex",Consumer.class,"vertex",Consumer.class,float.class,float.class,float.class),
            method("colour",Consumer.class,"colour",Consumer.class,int.class)};
        set("api",new KnifeBridge.Api(KnifeCheck.class.getClassLoader(),table));
        Player p=Client.INSTANCE.player; Collector collector=new Collector();
        update(false,0,false,0); check(!render(p,Context.RIGHT,collector),"Disabled knife replaced Vanilla");
        update(true,0,false,0); check(render(p,Context.RIGHT,collector),"Main sword was not replaced");
        check(!collector.deferred.isEmpty()&&collector.xyz.isEmpty(),"Geometry did not use deferred game submission");
        collector.flush(); check(collector.xyz.size()>100&&KnifeBridge.status()==3,"Game callback emitted no knife");
        check(collector.types.stream().anyMatch(t->t instanceof Setup),"Glow never reached additive main-target submission");
        check(collector.colours.stream().anyMatch(c->(c>>>24)>0&&(c>>>24)<255),"Glow emitted no alpha gradient");
        collector.types.clear();KnifeBridge.update(true,0,1,1,.14f,false,false,false,0,500,false,1.2f,1,0x69ddd5);
        check(render(p,Context.RIGHT,collector)&&collector.types.size()==1&&collector.types.get(0).equals("quads"),"Disabled glow still submitted a halo");collector.flush();update(true,0,false,0);
        check(!render(p,Context.LEFT,collector)&&!render(p,Context.OTHER,collector),"Offhand or non-first-person item was replaced");
        p.arm=Arm.LEFT; check(render(p,Context.LEFT,collector),"Left-handed main sword not supported"); collector.flush(); p.arm=Arm.RIGHT;
        Client.INSTANCE.screen=new Object(); check(!render(p,Context.RIGHT,collector),"Minecraft screen failed to suspend knife"); Client.INSTANCE.screen=null;
        p.held.kind.id="item.minecraft.apple"; check(!render(p,Context.RIGHT,collector),"Non-sword replaced by a knife"); p.held.kind.id="item.minecraft.diamond_sword";
        check(render(p,Context.RIGHT,collector),"Sword did not recover after item change"); collector.flush();
        update(true,0,true,1); check(render(p,Context.RIGHT,collector)&&(Integer)get("motion")==1,"Inspect press did not start animation");
        set("started",System.nanoTime()-5_000_000_000L); render(p,Context.RIGHT,collector);
        check((Integer)get("motion")==1,"Holding Inspect did not repeat");
        update(true,0,false,1); set("started",System.nanoTime()-5_000_000_000L); render(p,Context.RIGHT,collector);
        check((Integer)get("motion")==0,"Released Inspect looped forever");
        update(true,0,false,2); render(p,Context.RIGHT,collector); p.attack=.5f; render(p,Context.RIGHT,collector);
        check((Integer)get("motion")==0,"Attack did not cancel inspect"); p.attack=0;
        update(true,0,false,3); render(p,Context.RIGHT,collector); ++p.inventory.slot; render(p,Context.RIGHT,collector);
        check((Integer)get("motion")==0,"Slot change retained an old inspect");
        update(true,1,false,4); render(p,Context.RIGHT,collector); collector.flush();
        check((Integer)get("model")==1,"Knife selection did not change model");
        KnifeBridge.record(1_000_000_000L,new float[]{0,0,0},new float[]{0,1,0});
        KnifeBridge.record(1_010_000_000L,new float[]{.1f,0,0},new float[]{.1f,1,0});
        check(KnifeBridge.trail(1_010_000_000L,.14f,0xff00ffff).vertices()>0,"Moving blade generated no trail");
        check(KnifeBridge.trail(2_000_000_000L,.14f,0xff00ffff).vertices()==0,"Expired blade trail persisted");
        KnifeBridge.update(true,0,1,1,.14f,false,true,false,5,0,true,1.2f,1,0x69ddd5);
        check(!render(p,Context.RIGHT,collector),"Expired request continued replacing the sword");
        update(true,0,false,6); p.held.kind.id=null;
        check(!render(p,Context.RIGHT,collector),"Invalid item did not fall back to Vanilla"); p.held.kind.id="item.minecraft.diamond_sword";
        Path output=args.length==0?null:Path.of(args[0]); if(output!=null) Files.createDirectories(output);
        for(int skin=0;skin<3;++skin) for(int mode=0;mode<3;++mode) {
            KnifeGeometry.Frame a=KnifeGeometry.build(skin,"item.minecraft.diamond_sword",0,mode,1),b=KnifeGeometry.build(skin,"item.minecraft.diamond_sword",1,mode,1);
            check(a.mesh().vertices()>4000&&a.mesh().vertices()<32768,"Unbounded or empty knife mesh");
            for(float v:a.mesh().xyz()) check(Float.isFinite(v),"Invalid procedural mesh");
            float max=0; for(int i=0;i<a.mesh().xyz().length;++i) max=Math.max(max,Math.abs(a.mesh().xyz()[i]-b.mesh().xyz()[i]));
            if(mode==1) check(max<.00001f,"Inspect loop has a discontinuous endpoint");
            KnifeGeometry.Mesh glow=KnifeGeometry.glow(a.glowSegments(),new KnifeGeometry.Matrix(),.032f,1.2f,0x69ddd5);
            check(glow.vertices()>0&&glow.vertices()<32768,"Invalid luminous edge geometry");
            for(float v:glow.xyz())check(Float.isFinite(v),"Nonfinite glow geometry");
            check(Arrays.stream(glow.colours()).anyMatch(c->(c>>>24)==0)&&Arrays.stream(glow.colours()).anyMatch(c->(c>>>24)>80),"Glow needs a bright core and transparent boundary");
            check(KnifeGeometry.glow(a.glowSegments(),new KnifeGeometry.Matrix(),.032f,0,0x69ddd5).vertices()==0,"Zero glow strength emitted geometry");
            if(output!=null&&mode==0) {
                String name=new String[]{"butterfly","karambit","katana"}[skin];export(output.resolve(name+".obj"),a.mesh());export(output.resolve(name+"-glow.obj"),glow);
                System.out.println(name+": "+a.mesh().vertices()+" solid vertices, "+glow.vertices()+" glow vertices");
            }
        }
        check(KnifeProfiles.get("intermediary-1.21.11")!=null&&KnifeProfiles.get("mojmap-1.20.1")==null,"Unsupported hand profile accepted");
        System.out.println("PASS knives: real submission callback, models, materials, left hand, inspect, cancellation, looping, trails, leases and fallback");
    }
    static void export(Path path,KnifeGeometry.Mesh mesh) throws Exception {
        StringBuilder text=new StringBuilder("# Synthetic preview of the actual procedural knife mesh\n");
        for(int i=0;i<mesh.vertices();++i) {
            int colour=mesh.colours()[i];
            text.append(String.format(Locale.ROOT,"v %.6f %.6f %.6f %.4f %.4f %.4f %.4f%n",mesh.xyz()[i*3],mesh.xyz()[i*3+1],mesh.xyz()[i*3+2],(colour>>16&255)/255.,(colour>>8&255)/255.,(colour&255)/255.,(colour>>>24)/255.));
        }
        for(int i=1;i<=mesh.vertices();i+=4) text.append("f ").append(i).append(' ').append(i+1).append(' ').append(i+2).append(' ').append(i+3).append('\n');
        Files.writeString(path,text);
    }
}
