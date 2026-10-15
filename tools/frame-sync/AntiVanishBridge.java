package mcov;
import java.lang.reflect.*;
import java.util.*;
/** Samples at 2 Hz on the client tick, never on the render thread or network thread. */
public final class AntiVanishBridge {
    private AntiVanishBridge() {}
    public static final class View {
        public final int status;public final boolean limited;
        public final String[] names;public final long[] ids;public final int[] evidence;
        View(int status,boolean limited,List<AntiVanishTracker.Detection> rows) {
            this.status=status;this.limited=limited;names=new String[rows.size()];ids=new long[rows.size()*2];evidence=new int[rows.size()];
            for(int i=0;i<rows.size();++i){var r=rows.get(i);names[i]=r.name();ids[i*2]=r.id().getMostSignificantBits();ids[i*2+1]=r.id().getLeastSignificantBits();evidence[i]=r.evidence();}
        }
    }
    private record Request(boolean enabled,boolean invisible,float range,long deadline,long generation) {}
    private static volatile Request request=new Request(false,false,64,0,0);
    public static volatile View view=new View(0,false,List.of());
    private static volatile Api api;
    private static final AntiVanishTracker tracker=new AntiVanishTracker();
    private static Object level,connection;
    private static long next,generation=-1;
    public static boolean configure(Class<?> client,String profile) {
        String[][] table=AntiVanishProfiles.get(profile);if(table==null)return false;
        try {api=new Api(client.getClassLoader(),table);view=new View(1,false,List.of());return true;}
        catch(ReflectiveOperationException|LinkageError|RuntimeException e){api=null;System.err.println("[mcov] Anti Vanish unavailable: "+e);return false;}
    }
    public static void update(boolean enabled,boolean invisible,float range,long remaining) {
        Request previous=request;boolean on=enabled&&api!=null&&remaining>0&&Float.isFinite(range)&&range>=8&&range<=128;
        request=new Request(on,invisible,range,System.nanoTime()+Math.max(0,Math.min(500,remaining))*1_000_000L,previous.generation+(on!=previous.enabled?1:0));
        if(on)FrameSignal.antiVanishEnabled=true;
        else if(view.status!=0&&view.status!=1)view=new View(api==null?0:1,false,List.of());
    }
    public static void disable(){update(false,false,64,0);}
    private static void clear(){tracker.clear();level=connection=null;next=0;}
    public static void tick(Object client) {
        Request r=request;long now=System.nanoTime();Api a=api;
        if(!r.enabled||a==null||now>r.deadline){clear();FrameSignal.antiVanishEnabled=false;view=new View(a==null?0:1,false,List.of());return;}
        if(generation!=r.generation){clear();generation=r.generation;}
        if(now<next)return;next=now+500_000_000L;
        try {
            Object world=a.s.get("level",client),local=a.s.get("player",client),conn=a.s.call("connection",client);
            if(world==null||local==null||conn==null){clear();view=new View(2,false,List.of());return;}
            if(level!=world||connection!=conn){tracker.clear();level=world;connection=conn;}
            UUID self=(UUID)a.s.call("uuid",local);Set<UUID> listed=new HashSet<>();boolean limited=false;int count=0;
            for(Object info:(Collection<?>)a.s.call("listed",conn)) {
                if(++count>512){limited=true;break;}Object profile=a.s.call("profile",info);if(profile!=null)listed.add(a.id(profile));
            }
            LinkedHashMap<UUID,AntiVanishTracker.Player> players=new LinkedHashMap<>();count=0;
            for(Object info:(Collection<?>)a.s.call("online",conn)) {
                if(++count>512){limited=true;break;}Object profile=a.s.call("profile",info);if(profile==null)continue;UUID id=a.id(profile);
                players.put(id,new AntiVanishTracker.Player(id,a.name(profile),listed.contains(id),true,false,false));
            }
            count=0;
            for(Object player:(List<?>)a.s.call("players",world)) {
                if(++count>256){limited=true;break;}if(player==local)continue;
                double distance=((Number)a.s.call("distance",player,local)).doubleValue();if(!Double.isFinite(distance)||distance>r.range*r.range)continue;
                Object profile=a.s.call("playerProfile",player);if(profile==null)continue;UUID id=a.id(profile);var old=players.get(id);
                if(old==null&&players.size()>=512){limited=true;continue;}
                players.put(id,new AntiVanishTracker.Player(id,a.name(profile),listed.contains(id),old!=null,true,(Boolean)a.s.call("invisible",player)));
            }
            view=new View(3,limited,tracker.sample(new ArrayList<>(players.values()),self,now/1_000_000,r.invisible));
        }catch(Throwable e){clear();view=new View(4,false,List.of());request=new Request(false,false,64,0,r.generation+1);FrameSignal.antiVanishEnabled=false;System.err.println("[mcov] Anti Vanish sampling failed: "+e);}
    }
    static final class Api {
        final BaseTextureBridge.Symbols s;final Method id,name;
        Api(ClassLoader loader,String[][] table)throws ReflectiveOperationException {
            s=new BaseTextureBridge.Symbols(loader,table);Class<?> profile=Class.forName("com.mojang.authlib.GameProfile",false,loader);
            id=profile.getMethod("id");name=profile.getMethod("name");
        }
        UUID id(Object profile)throws ReflectiveOperationException{return (UUID)id.invoke(profile);}
        String name(Object profile)throws ReflectiveOperationException{return (String)name.invoke(profile);}
    }
}
