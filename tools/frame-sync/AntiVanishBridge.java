package mcov;
import java.lang.reflect.*;
import java.util.*;
/** Samples at 2 Hz on the client tick, never on the render thread or network thread. */
public final class AntiVanishBridge {
    private AntiVanishBridge() {}
    public static final class View {
        public final int status;public final boolean limited,packetHooks;
        public final String[] names,reasons;public final long[] ids;public final int[] evidence;
        View(int status,boolean limited,List<AntiVanishTracker.Detection> rows) {
            this.status=status;this.limited=limited;packetHooks=AntiVanishPackets.installed;names=new String[rows.size()];reasons=new String[rows.size()];ids=new long[rows.size()*2];evidence=new int[rows.size()];
            for(int i=0;i<rows.size();++i){var r=rows.get(i);names[i]=r.name();reasons[i]=r.reason();ids[i*2]=r.id().getMostSignificantBits();ids[i*2+1]=r.id().getLeastSignificantBits();evidence[i]=r.evidence();}
        }
    }
    private record Request(boolean enabled,boolean invisible,float range,long deadline,long generation) {}
    private static volatile Request request=new Request(false,false,64,0,0);
    public static volatile View view=new View(0,false,List.of());
    private static volatile Api api;private static volatile boolean fault;
    private static final AntiVanishTracker tracker=new AntiVanishTracker();
    private static Object level,connection,player;
    private static long next,nextRoster,nextProbe,generation=-1;
    private static boolean limited;
    public static boolean configure(Class<?> client,String profile) {
        String[][] table=AntiVanishProfiles.get(profile);if(table==null)return false;
        try {api=new Api(client.getClassLoader(),table);view=new View(1,false,List.of());return true;}
        catch(ReflectiveOperationException|LinkageError|RuntimeException e){api=null;System.err.println("[mcov] Anti Vanish unavailable: "+e);return false;}
    }
    public static void update(boolean enabled,boolean invisible,float range,long remaining) {
        Request previous=request;if(!enabled)fault=false;boolean on=enabled&&!fault&&api!=null&&remaining>0&&Float.isFinite(range)&&range>=8&&range<=128;
        request=new Request(on,invisible,range,System.nanoTime()+Math.max(0,Math.min(500,remaining))*1_000_000L,previous.generation+(on!=previous.enabled?1:0));
        if(on)FrameSignal.antiVanishEnabled=true;
        else if(view.status!=0&&view.status!=1&&!(enabled&&fault))view=new View(api==null?0:1,false,List.of());
    }
    public static void disable(){update(false,false,64,0);}
    private static void clear(){tracker.clear();AntiVanishPackets.clear();if(api!=null)api.clear();level=connection=player=null;next=nextRoster=nextProbe=0;limited=false;}
    public static void tick(Object client) {
        Request r=request;long now=System.nanoTime();Api a=api;
        if(!r.enabled||a==null||now>r.deadline){clear();FrameSignal.antiVanishEnabled=false;view=new View(a==null?0:1,false,List.of());return;}
        if(generation!=r.generation){clear();generation=r.generation;}
        if(now<next)return;next=now+50_000_000L;
        try {
            Object world=a.s.get("level",client),local=a.s.get("player",client),conn=a.s.call("connection",client);
            if(world==null||local==null||conn==null){clear();view=new View(2,false,List.of());return;}
            if(level!=world||connection!=conn||player!=local){clear();level=world;connection=conn;player=local;nextProbe=now+5_000_000_000L;}
            if(AntiVanishPackets.installed)AntiVanishPackets.bind(a.s.call("network",conn),r.deadline);
            if(now>=nextRoster){
            nextRoster=now+500_000_000L;
            UUID self=(UUID)a.s.call("uuid",local);tracker.local(self,a.name(a.s.call("playerProfile",local)));Set<UUID> listed=new HashSet<>();limited=false;int count=0;
            for(Object info:(Collection<?>)a.s.call("listed",conn)) {
                if(++count>512){limited=true;break;}Object profile=a.s.call("profile",info);if(profile!=null)listed.add(a.id(profile));
            }
            LinkedHashMap<UUID,AntiVanishTracker.Player> players=new LinkedHashMap<>();count=0;
            for(Object info:(Collection<?>)a.s.call("online",conn)) {
                if(++count>512){limited=true;break;}Object profile=a.s.call("profile",info);if(profile==null)continue;UUID id=a.id(profile);
                players.put(id,new AntiVanishTracker.Player(id,a.name(profile),listed.contains(id),true,false,false,a.s.methods.containsKey("mode")&&"spectator".equals(a.s.call("modeName",a.s.call("mode",info)))));
            }
            count=0;
            for(Object player:(List<?>)a.s.call("players",world)) {
                if(++count>256){limited=true;break;}if(player==local)continue;
                double distance=((Number)a.s.call("distance",player,local)).doubleValue();if(!Double.isFinite(distance)||distance>r.range*r.range)continue;
                Object profile=a.s.call("playerProfile",player);if(profile==null)continue;UUID id=a.id(profile);var old=players.get(id);
                if(old==null&&players.size()>=512){limited=true;continue;}
                players.put(id,new AntiVanishTracker.Player(id,a.name(profile),listed.contains(id),old!=null,true,(Boolean)a.s.call("invisible",player),old!=null&&old.spectator()));
            }
            tracker.sample(new ArrayList<>(players.values()),self,now/1_000_000,r.invisible);
            }
            if(AntiVanishPackets.installed){
                if(a.s.get("menu",local)!=a.s.get("inventoryMenu",local))a.container=now/1_000_000;
                a.range=r.range;AntiVanishPackets.drain(a,world,local,tracker,now/1_000_000,true);
                if(now>=nextProbe){nextProbe=now+5_000_000_000L;a.probe(conn,now/1_000_000);}
            }
            limited|=AntiVanishPackets.overflowed();
            view=new View(3,limited,tracker.rows(now/1_000_000));
        }catch(Throwable e){fault=true;clear();view=new View(4,false,List.of());request=new Request(false,false,64,0,r.generation+1);FrameSignal.antiVanishEnabled=false;System.err.println("[mcov] Anti Vanish sampling failed: "+e);}
    }
    static final class Api {
        final BaseTextureBridge.Symbols s;final Method id,name;
        float range=64;int nextProbeId=0x60000000;
        final Map<Integer,Long> probes=new LinkedHashMap<>();
        final Map<Cell,Long> selfPlaced=new HashMap<>(),mechanisms=new HashMap<>();
        final Map<String,Long> bursts=new HashMap<>();
        final Deque<Explosion> explosions=new ArrayDeque<>();
        long action=-6000,container=-3000;
        record Cell(int x,int y,int z){}
        record Position(double x,double y,double z){double distance(Position other){return Math.pow(x-other.x,2)+Math.pow(y-other.y,2)+Math.pow(z-other.z,2);}}
        record Explosion(Position position,double radius,long at){}
        void clear(){probes.clear();selfPlaced.clear();mechanisms.clear();bursts.clear();explosions.clear();action=-6000;container=-3000;}

        Api(ClassLoader loader,String[][] table)throws ReflectiveOperationException {
            s=new BaseTextureBridge.Symbols(loader,table);Class<?> profile=Class.forName("com.mojang.authlib.GameProfile",false,loader);
            id=profile.getMethod("id");name=profile.getMethod("name");AntiVanishPackets.configure(s);
        }
        boolean is(String key,Object value){Class<?> type=s.type(key);return type!=null&&type.isInstance(value);}
        double number(String key,Object value)throws ReflectiveOperationException{return ((Number)s.call(key,value)).doubleValue();}
        Position position(Object entity)throws ReflectiveOperationException{return new Position(number("entityX",entity),number("entityY",entity),number("entityZ",entity));}
        Position packetPosition(String key,Object p)throws ReflectiveOperationException{return new Position(number(key+"X",p),number(key+"Y",p),number(key+"Z",p));}
        Cell cell(Position p){return new Cell((int)Math.floor(p.x),(int)Math.floor(p.y),(int)Math.floor(p.z));}
        Object pos(Cell c)throws ReflectiveOperationException{return s.type("Pos").getConstructor(int.class,int.class,int.class).newInstance(c.x,c.y,c.z);}
        String block(Object world,Cell c)throws ReflectiveOperationException{
            Object p=pos(c);if(!(Boolean)s.call("loaded",world,p))return "";
            return String.valueOf(s.call("blockKey",s.call("block",s.call("state",world,p))));
        }
        void probe(Object conn,long now)throws ReflectiveOperationException{
            probes.values().removeIf(at->now-at>15000);if(probes.size()>=8)return;
            int probe=nextProbeId++;if(nextProbeId>=0x70000000)nextProbeId=0x60000000;
            Object packet=s.type("Probe").getConstructor(int.class,String.class).newInstance(probe,"minecraft:msg ");
            probes.put(probe,now);s.call("send",conn,packet);
        }
        String sound(Object holder)throws ReflectiveOperationException{return String.valueOf(s.call("soundId",s.call("holder",holder))).toLowerCase(Locale.ROOT);}
        static boolean suspiciousSound(String id){return id.contains("chest.open")||id.contains("chest.close")||id.contains("chest.locked")||id.contains("barrel.open")||id.contains("barrel.close")||id.contains("shulker_box.open")||id.contains("shulker_box.close")||id.contains("door.open")||id.contains("door.close")||id.contains("fence_gate.open")||id.contains("fence_gate.close")||id.contains("button.click")||id.contains("lever.click");}
        static boolean suspiciousParticle(String id){return id.contains("crit")||id.contains("enchanted_hit")||id.contains("damage_indicator")||id.contains("smoke")||id.equals("block");}
        static String path(String id){int at=id.indexOf(':');return (at<0?id:id.substring(at+1)).toLowerCase(Locale.ROOT);}
        void observe(Object packet,boolean outbound,Object world,Object local,AntiVanishTracker tracker,long now,boolean sensors)throws ReflectiveOperationException{
            selfPlaced.values().removeIf(until->until<=now);mechanisms.values().removeIf(until->until<=now);
            bursts.values().removeIf(at->now-at>2000);probes.values().removeIf(at->now-at>15000);
            while(!explosions.isEmpty()&&now-explosions.peekFirst().at>3000)explosions.removeFirst();
            if(outbound){
                if(is("UseOn",packet)){
                    action=now;Object hit=s.call("hitPos",s.call("hit",packet));
                    int x=((Number)s.call("posX",hit)).intValue(),y=((Number)s.call("posY",hit)).intValue(),z=((Number)s.call("posZ",hit)).intValue();
                    // Includes both halves of beds/doors and the adjacent placement cell.
                    for(int dx=-1;dx<=1;++dx)for(int dy=-2;dy<=2;++dy)for(int dz=-1;dz<=1;++dz)selfPlaced.put(new Cell(x+dx,y+dy,z+dz),now+6000);
                    if(selfPlaced.size()>2048)selfPlaced.clear();
                }else if(is("Use",packet)||is("Dig",packet)||is("Interact",packet)||is("Swing",packet))action=now;
                return;
            }
            if(is("Remove",packet)){
                List<?> ids=(List<?>)s.call("removed",packet);List<UUID> values=new ArrayList<>();for(Object value:ids)if(value instanceof UUID id)values.add(id);tracker.removed(values,now);return;
            }
            if(is("Update",packet)){
                Set<?> actions=(Set<?>)s.call("actions",packet);List<?> entries=(List<?>)s.call("entries",packet);
                if(actions.contains(s.get("UPDATE_LISTED",null))){List<UUID> hidden=new ArrayList<>();
                    for(Object entry:entries)if(!(Boolean)s.call("entryListed",entry))hidden.add((UUID)s.call("entryId",entry));
                    tracker.tabHidden(hidden,now);
                }
                if(actions.contains(s.get("UPDATE_GAME_MODE",null))&&!actions.contains(s.get("ADD_PLAYER",null)))
                    for(Object entry:entries){Object mode=s.call("entryMode",entry);if(mode!=null)tracker.mode((UUID)s.call("entryId",entry),"spectator".equals(s.call("modeName",mode)),now);}
                return;
            }
            if(is("Chat",packet)){
                Object component=s.call("chat",packet);String text=String.valueOf(s.call("text",component)),departed="";
                Object contents=s.call("contents",component);
                if(is("Translated",contents)&&"multiplayer.player.left".equals(s.call("translationKey",contents))){
                    Object[] args=(Object[])s.call("translationArgs",contents);if(args.length>0)departed=is("Component",args[0])?String.valueOf(s.call("text",args[0])):String.valueOf(args[0]);
                }
                tracker.message(text,departed,now);return;
            }
            if(is("Suggestions",packet)){
                int id=((Number)s.call("suggestionId",packet)).intValue();Long sent=probes.remove(id);if(sent==null||now-sent>15000)return;
                List<String> names=new ArrayList<>();for(Object entry:(List<?>)s.call("suggestions",packet)){if(names.size()>=512)break;names.add(String.valueOf(s.call("suggestionText",entry)));}
                tracker.completions(names,now);return;
            }
            if(!sensors)return;
            if(s.get("menu",local)!=s.get("inventoryMenu",local))container=now;
            if(is("Explosion",packet)){
                Object vector=s.call("explosionPos",packet);Position at=new Position(s.number("vecX",vector),s.number("vecY",vector),s.number("vecZ",vector));
                explosions.addLast(new Explosion(at,Math.max(2,number("explosionRadius",packet)+4),now));while(explosions.size()>8)explosions.removeFirst();return;
            }
            if(is("EntitySound",packet)){
                String sound=sound(s.call("entitySound",packet));if(!suspiciousSound(sound))return;
                Object entity=s.call("entityById",world,((Number)s.call("soundEntity",packet)).intValue());
                if(is("Player",entity)&&entity!=local&&(Boolean)s.call("invisible",entity)&&number("distance",entity,local)<=range*range){
                    Object profile=s.call("playerProfile",entity);tracker.sensor(id(profile),name(profile),AntiVanishTracker.SOUND,"Sound from invisible entity",16,true,now,3000);
                }return;
            }
            boolean particle=is("Particles",packet);if(!particle&&!is("Sound",packet))return;
            String kind=particle?path(String.valueOf(s.call("registryKey",s.get("particleRegistry",null),s.call("particleType",s.call("particle",packet))))):path(sound(s.call("sound",packet)));
            if(particle?!suspiciousParticle(kind):!suspiciousSound(kind))return;
            Position at=packetPosition(particle?"particles":"sound",packet),me=position(local);
            if(!Double.isFinite(at.x+at.y+at.z)||at.distance(me)>range*range||now-action<(particle?900:1000))return;
            if(!particle&&(selfPlaced.getOrDefault(cell(at),0L)>now||now-container<2500&&(kind.contains("chest")||kind.contains("barrel")||kind.contains("shulker"))))return;
            for(Explosion explosion:explosions)if(at.distance(explosion.position)<=explosion.radius*explosion.radius)return;
            int count=0;for(Object entity:(Iterable<?>)s.call("entities",world)){
                if(++count>2048)return; // Fail closed if the visible-cause scan was incomplete.
                if(entity==local)continue;double distance=position(entity).distance(at);
                if(distance<=16&&(is("Projectile",entity)||is("Player",entity)&&!(Boolean)s.call("invisible",entity)))return;
                if(!particle&&distance<=9&&is("Villager",entity)&&(kind.contains("door")&&!kind.contains("trapdoor")||kind.contains("fence_gate")))return;
            }
            Cell centre=cell(at);
            if(!particle&&(kind.contains("door")||kind.contains("trapdoor"))){
                for(int dy=-1;dy<=1;++dy){Cell c=new Cell(centre.x,centre.y+dy,centre.z);Object p=pos(c);if(!(Boolean)s.call("loaded",world,p))return;
                    Object state=s.call("state",world,p),property=s.get("poweredProperty",null);
                    boolean powered=(Boolean)s.call("hasProperty",state,property)&&(Boolean)s.call("property",state,property)||(Boolean)s.call("powered",world,p);
                    if(powered){mechanisms.put(c,now+5000);return;}if(mechanisms.getOrDefault(c,0L)>now)return;
                }
            }
            if(particle&&kind.contains("smoke"))for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy)for(int dz=-2;dz<=2;++dz){
                String b=block(world,new Cell(centre.x+dx,centre.y+dy,centre.z+dz));
                if(b.isEmpty()||b.contains("campfire")||b.contains("furnace")||b.contains("smoker")||b.contains("torch")||b.contains("fire")||b.contains("candle")||b.contains("respawn_anchor"))return;
            }
            if(particle&&(kind.equals("block")||kind.contains("smoke"))){
                if(at.distance(me)<=6.25)return;Long last=bursts.put(kind,now);if(last==null||now-last>2000)return;bursts.remove(kind);
            }
            String direction=(at.z-me.z < -1?"N":at.z-me.z>1?"S":"")+(at.x-me.x>1?"E":at.x-me.x < -1?"W":"");
            String location=Math.round(Math.sqrt(at.distance(me)))+"m "+direction;
            tracker.sensor(null,location,particle?AntiVanishTracker.PARTICLE:AntiVanishTracker.SOUND,(particle?"Ghost particle: ":"Suspicious sound: ")+kind,particle?16:14,false,now,3000);
        }
        double number(String key,Object value,Object argument)throws ReflectiveOperationException{return ((Number)s.call(key,value,argument)).doubleValue();}
        UUID id(Object profile)throws ReflectiveOperationException{return (UUID)id.invoke(profile);}
        String name(Object profile)throws ReflectiveOperationException{return (String)name.invoke(profile);}
    }
}
