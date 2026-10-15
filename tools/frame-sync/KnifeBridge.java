package mcov;

import java.lang.reflect.*;
import java.util.*;

/** Optional render-thread geometry submission. JNI updates only a leased POD
 * configuration; Minecraft's hand pass owns the mesh, animation and trails. */
public final class KnifeBridge {
    private KnifeBridge() {}
    static final long LEASE=500_000_000L;
    record Settings(boolean enabled,int skin,float speed,float size,float lifetime,boolean deploy,boolean trails,
                    boolean held,long serial,long deadline,boolean glow,float strength,float width,int colour) {}
    private static volatile Settings settings=new Settings(false,0,1,1,.14f,true,true,false,0,0,true,1.2f,1,0x69ddd5);
    private static volatile int status;
    private static volatile Api api;
    private static long started,consumed,lastFrame;
    private static int motion,slot=-1,model=-1;
    private static String item="";
    private static Object world;
    private static final ArrayDeque<Streak> streaks=new ArrayDeque<>();
    private record Streak(long time,float[] base,float[] tip) {}
    public static synchronized boolean configure(Class<?> client,String profile) {
        String[][] table=KnifeProfiles.get(profile); if(table==null) return false;
        int index=profile.startsWith("mojmap")?0:profile.startsWith("vanilla")?1:2;
        try {
            Class.forName(KnifeProfiles.row(table,"class.Hand")[1].replace('/','.'),false,client.getClassLoader());
            if((FrameSignal.knifeProfiles&(1L<<index))==0) return false;
            api=new Api(client.getClassLoader(),table); status=1; return true;
        }
        catch(ReflectiveOperationException|LinkageError error) { api=null; status=0; return false; }
    }
    public static void update(boolean enabled,int skin,float speed,float size,float lifetime,boolean deploy,boolean trails,boolean held,long serial,long remainingMs,boolean glow,float strength,float width,int colour) {
        boolean valid=skin>=0&&skin<=2&&Float.isFinite(speed)&&speed>=.5f&&speed<=2&&Float.isFinite(size)&&size>=.5f&&size<=2&&
            Float.isFinite(lifetime)&&lifetime>=.05f&&lifetime<=.35f&&Float.isFinite(strength)&&strength>=0&&strength<=3&&Float.isFinite(width)&&width>=.25f&&width<=3&&colour>=0&&colour<=0xffffff;
        settings=new Settings(enabled&&valid&&remainingMs>0,skin,speed,size,lifetime,deploy,trails,held,serial,
            System.nanoTime()+Math.max(0,Math.min(500,remainingMs))*1_000_000L,glow,strength,width,colour);
        if(api!=null) {
            if(!enabled||!valid||remainingMs<=0) status=1;
            else if(status!=3&&status!=4) status=2;
        }
    }
    public static int status() { return status; }
    public static void disable() { update(false,0,1,1,.14f,false,false,false,0,0,false,0,1,0); }
    private static void reset(long serial) {
        started=lastFrame=0; consumed=serial; motion=0; slot=model=-1; item=""; world=null; streaks.clear();
    }
    static boolean sword(String id) { return id!=null&&(id.startsWith("item.minecraft.")&&id.endsWith("_sword")); }
    static float phase(long now,float speed) {
        float duration=motion==2?.85f:model==0?3.8f:model==1?3.2f:4.4f;
        return Math.max(0,(now-started)*1e-9f*speed/duration);
    }
    public static boolean render(Object renderer,Object holder,Object stack,Object context,Object pose,Object collector,int light) {
        Settings c=settings; Api a=api; long now=System.nanoTime();
        if(a==null||!c.enabled||now>c.deadline) { reset(c.serial); if(a!=null) status=1; return false; }
        if(status==4) return false;
        try {
            Object client=a.s.call("instance",null),level=a.s.get("level",client),local=a.s.get("player",client);
            if(holder!=local||local==null||level==null||a.s.get("screen",client)!=null) { reset(c.serial); status=2; return false; }
            boolean right=a.s.call("arm",holder)==a.s.get("rightArm",null);
            if(context!=a.s.get(right?"firstRight":"firstLeft",null)) return false;
            if(stack==null||(Boolean)a.s.call("empty",stack)) { reset(c.serial); status=2; return false; }
            Object held=a.s.call("main",holder),kind=a.s.call("item",stack);
            String name=(String)a.s.call("name",kind);
            if(held==null||kind!=a.s.call("item",held)||!sword(name)) { reset(c.serial); status=2; return false; }
            int selected=((Number)a.s.call("slot",a.s.call("inventory",holder))).intValue();
            boolean changed=world!=level||model!=c.skin||!item.equals(name)||slot!=selected||lastFrame!=0&&now-lastFrame>200_000_000L;
            if(changed) {
                reset(c.serial); world=level; model=c.skin; item=name; slot=selected;
                if(c.deploy) { motion=2; started=now; }
            }
            boolean busy=(Boolean)a.s.call("using",holder);
            float attack=((Number)a.s.call("attack",holder,1.f)).floatValue();
            if(busy||attack>.01f) { motion=0; consumed=c.serial; }
            else if(consumed!=c.serial) { consumed=c.serial; started=now; motion=1; streaks.clear(); }
            float progress=phase(now,c.speed);
            if(motion!=0&&progress>=1) {
                if(motion==1&&c.held) { started=now; progress=0; }
                else { motion=0; progress=0; }
            }
            KnifeGeometry.Frame frame=KnifeGeometry.build(c.skin,name,progress,motion,c.size);
            // Mirroring happens in our vertices, leaving Minecraft's stack intact.
            KnifeGeometry.Matrix mirror=new KnifeGeometry.Matrix().scale(right?1:-1,1,1);
            KnifeGeometry.Mesh mesh=right?frame.mesh():transform(frame.mesh(),mirror);
            float[] matrix=new float[16]; Object entry=a.s.call("last",pose);
            a.matrixGet.invoke(a.s.call("matrix",entry),(Object)matrix);
            KnifeGeometry.Matrix current=new KnifeGeometry.Matrix(matrix);
            float[] base=current.point(mirror.point(frame.base())),tip=current.point(mirror.point(frame.tip()));
            if(c.trails&&(motion!=0||attack>.01f)&&!busy) record(now,base,tip);
            else streaks.clear();
            boolean glowing=c.glow&&c.strength>0;
            KnifeGeometry.Mesh trail=trail(now,c.lifetime,glowing?(0xff000000|c.colour):KnifeGeometry.metal(name));
            Object ordered=a.s.call("order",collector,0);
            // Submission may defer until the end of the hand pass. Capture only
            // immutable per-frame geometry, never the next frame's mutable state.
            Object submission=a.callback(mesh);
            a.s.call("submit",ordered,pose,a.quads,submission);
            if(trail.vertices()!=0) a.s.call("submit",ordered,a.identity,glowing?a.glow:a.quads,a.callback(trail));
            if(glowing) {
                KnifeGeometry.Mesh halo=KnifeGeometry.glow(frame.glowSegments(),new KnifeGeometry.Matrix(current.a).mul(mirror),.032f*c.width*c.size,c.strength,c.colour);
                Object luminous=a.s.call("order",collector,1);
                a.s.call("submit",luminous,a.identity,a.glow,a.callback(halo));
                KnifeGeometry.Mesh wake=trailGlow(now,c.lifetime,.04f*c.width*c.size,c.strength,c.colour);
                if(wake.vertices()!=0)a.s.call("submit",luminous,a.identity,a.glow,a.callback(wake));
            }
            lastFrame=now; status=3; return true;
        } catch(Throwable error) {
            // A failed optional visual must preserve Vanilla item rendering.
            reset(c.serial); status=4;
            System.err.println("[mcov] knife rendering disabled: "+error.getClass().getSimpleName());
            return false;
        }
    }
    static KnifeGeometry.Mesh transform(KnifeGeometry.Mesh source,KnifeGeometry.Matrix matrix) {
        float[] result=new float[source.xyz().length];
        for(int i=0;i<source.vertices();++i) System.arraycopy(matrix.point(source.xyz()[i*3],source.xyz()[i*3+1],source.xyz()[i*3+2]),0,result,i*3,3);
        // A reflection reverses winding. Preserve outward faces for culling.
        int[] colours=source.colours().clone();
        for(int first=0;first<source.vertices();first+=4) for(int i=0;i<2;++i) {
            int a=first+i,b=first+3-i; int colour=colours[a]; colours[a]=colours[b]; colours[b]=colour;
            for(int axis=0;axis<3;++axis) { float value=result[a*3+axis]; result[a*3+axis]=result[b*3+axis]; result[b*3+axis]=value; }
        }
        return new KnifeGeometry.Mesh(result,colours);
    }
    static void record(long now,float[] base,float[] tip) {
        Streak last=streaks.peekLast();
        if(last!=null&&now-last.time<4_000_000L) return;
        if(last!=null) {
            double d=0; for(int i=0;i<3;++i) d+=Math.pow(tip[i]-last.tip[i],2);
            if(d<.000004) return;
            if(d>4) streaks.clear(); // Equip/camera discontinuity is not a slash.
        }
        streaks.addLast(new Streak(now,base,tip)); while(streaks.size()>48) streaks.removeFirst();
    }
    static KnifeGeometry.Mesh trail(long now,float lifetime,int colour) {
        long duration=(long)(lifetime*1e9);
        while(!streaks.isEmpty()&&now-streaks.peekFirst().time>duration) streaks.removeFirst();
        KnifeGeometry.Builder b=new KnifeGeometry.Builder(); Streak previous=null;
        for(Streak next:streaks) {
            if(previous!=null) {
                float alpha=Math.max(0,1-(now-previous.time)/(float)duration);
                int ink=colour&0xffffff|(Math.round(alpha*105)<<24);
                b.quad(previous.base,previous.tip,next.tip,next.base,ink);
                b.quad(next.base,next.tip,previous.tip,previous.base,ink);
            }
            previous=next;
        }
        return b.finish();
    }
    static KnifeGeometry.Mesh trailGlow(long now,float lifetime,float width,float strength,int colour) {
        KnifeGeometry.Builder result=new KnifeGeometry.Builder();Streak previous=null;
        for(Streak next:streaks) {
            if(previous!=null) {
                float alpha=Math.max(0,1-(now-previous.time)/(lifetime*1e9f));
                float[] lines=new float[12];System.arraycopy(previous.tip,0,lines,0,3);System.arraycopy(next.tip,0,lines,3,3);
                System.arraycopy(previous.base,0,lines,6,3);System.arraycopy(next.base,0,lines,9,3);
                result.append(KnifeGeometry.glow(lines,new KnifeGeometry.Matrix(),width,strength*alpha,colour,false),new KnifeGeometry.Matrix());
            }
            previous=next;
        }
        return result.finish();
    }
    static final class Api {
        final BaseTextureBridge.Symbols s;
        final Method matrixGet;
        final Object quads,identity,glow;
        Api(ClassLoader loader,String[][] table) throws ReflectiveOperationException {
            s=new BaseTextureBridge.Symbols(loader,Arrays.stream(table).filter(row->!row[0].equals("mainItem")).toArray(String[][]::new));
            matrixGet=Class.forName("org.joml.Matrix4f",false,loader).getMethod("get",float[].class);
            quads=s.call("quads",null); identity=s.type("Pose").getConstructor().newInstance();
            Object snippets=Array.newInstance(s.type("Snippet"),1);Array.set(snippets,0,s.get("glowSnippet",null));
            Object builder=s.call("pipelineBuilder",null,snippets);
            s.call("pipelineLocation",builder,"mcov:knife_glow");
            s.call("pipelineBlend",builder,s.get("glowBlend",null));
            s.call("pipelineCull",builder,false);s.call("pipelineDepth",builder,false);
            s.call("pipelineFormat",builder,s.get("colourFormat",null),s.get("quadMode",null));
            Object setup=s.call("setupBuilder",null,s.call("pipelineBuild",builder));
            s.call("setupOutput",setup,s.get("mainOutput",null));
            glow=s.call("renderType",null,"mcov_knife_glow",s.call("setupBuild",setup));
        }
        Object callback(KnifeGeometry.Mesh mesh) {
            return Proxy.newProxyInstance(s.type("Custom").getClassLoader(),new Class<?>[]{s.type("Custom")},(proxy,method,args)->{
                if(method.getDeclaringClass()==Object.class) return switch(method.getName()) {
                    case "hashCode"->System.identityHashCode(proxy); case "equals"->proxy==args[0]; default->"mcov knife geometry";
                };
                try {
                if(args==null||args.length!=2) throw new IllegalStateException("Unexpected custom geometry callback");
                float[] raw=new float[16]; matrixGet.invoke(s.call("matrix",args[0]),(Object)raw);
                KnifeGeometry.Matrix transform=new KnifeGeometry.Matrix(raw);
                for(int i=0;i<mesh.vertices();++i) {
                    float[] xyz=transform.point(mesh.xyz()[i*3],mesh.xyz()[i*3+1],mesh.xyz()[i*3+2]);
                    s.call("vertex",args[1],xyz[0],xyz[1],xyz[2]); s.call("colour",args[1],mesh.colours()[i]);
                }
                } catch(Throwable error) { status=4; }
                return null;
            });
        }
    }
}
