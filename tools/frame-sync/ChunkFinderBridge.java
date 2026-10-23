package mcov;

import java.lang.invoke.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicReferenceArray;

/** Copies palettes on the client tick. Workers own copies and never access Minecraft's world. */
public final class ChunkFinderBridge {
    private ChunkFinderBridge() {}
    public static final class View {
        public final int status,scanned,pending,activityStatus,baselines; public final long world,revision;
        public final boolean limited; public final int[] rows,blocks; public final long[] alerts,scannedChunks;
        View(int status) {this(status,0,0,0,0,false,new int[0],new int[0],new long[0],new long[0]);}
        View(int status,int scanned,int pending,long world,long revision,boolean limited,int[] rows,int[] blocks,long[] alerts,long[] scannedChunks) {
            activityStatus=ChunkActivity.status();baselines=ChunkActivity.tracker.size();
            this.status=status;this.scanned=scanned;this.pending=pending;this.world=world;this.revision=revision;this.limited=limited;this.rows=rows;this.blocks=blocks;this.alerts=alerts;this.scannedChunks=scannedChunks;
        }
    }
    private record Request(boolean enabled,int[] settings,int threads,int perTick,int alerts,boolean chat,long clear,long deadline,long generation) {}
    private static volatile Request request=new Request(false,new int[]{(1<<21)-1,1,2,4,2,8,50,5},2,3,6,true,0,0,0);
    public static volatile View view=new View(0);
    private static volatile Api api;
    private static Object level,player; private static long generation=-1,world,revision,alertSerial,clearId,nextPublish;
    private static final LightScan lightScan=new LightScan();
    private static int scanned; private static boolean limited,fault;
    private static ThreadPoolExecutor workers;
    private record Pending(int x,int z,Object identity,Future<ChunkFinderDetector.Result> future) {}
    private record Saved(ChunkFinderDetector.Result result,long alert) {}
    private static final Map<Long,Pending> pending=new HashMap<>();
    private static final Map<Long,Long> sampled=new HashMap<>();
    private static final LinkedHashMap<Long,Saved> finds=new LinkedHashMap<>();
    private static final Map<Long,ChunkFinderDetector.Result> activityFinds=new HashMap<>();
    private static final ArrayDeque<Long> alertTimes=new ArrayDeque<>();
    static long key(int x,int z) {return (long)x<<32|(z&0xffffffffL);}
    public static boolean configure(Class<?> client,String profile) {
        try {String[][] table=ChunkFinderProfiles.get(profile);if(table==null)return false;api=new Api(client.getClassLoader(),table);ChunkActivity.configure(api);view=new View(1);return true;}
        catch(Throwable e) {api=null;view=new View(0);System.err.println("[mcov] Chunkfinder unavailable: "+e);return false;}
    }
    // Agent sampler is the sole request writer; all mutable discovery state belongs to the tick.
    public static void update(boolean enabled,int[] settings,int threads,int perTick,int alerts,boolean chat,long clear,long remaining) {
        Request old=request;
        if(settings==null||(settings.length!=8&&settings.length!=11&&settings.length!=15))return;
        int[] s=Arrays.copyOf(settings,15);s[8]=Math.max(0,Math.min(2,s[8]));s[9]=s[9]!=0?1:0;s[10]=s[10]!=0?1:0;s[0]&=(1<<21)-1;
        s[11]=s[11]!=0?1:0;s[12]=Math.max(1,Math.min(16,s[12]));s[13]=Math.max(1,Math.min(15,s[13]));s[14]=s[14]!=0?1:0;
        int[] lo={0,1,0,1,1,2,25,1},hi={0,16,16,32,16,40,99,10};
        for(int i=1;i<8;++i)s[i]=Math.max(lo[i],Math.min(hi[i],s[i]));
        boolean on=enabled&&api!=null&&remaining>0&&(s[8]!=0||(s[0]&((1<<0)|(1<<2)|(1<<14)|(1<<20)))!=0);
        boolean changed=on!=old.enabled||clear!=old.clear||!Arrays.equals(s,old.settings);
        request=new Request(on,s,Math.max(1,Math.min(4,threads)),Math.max(1,Math.min(12,perTick)),Math.max(1,Math.min(30,alerts)),chat,clear,
            System.nanoTime()+Math.max(0,Math.min(500,remaining))*1_000_000L,old.generation+(changed?1:0));
        if(on||old.enabled)FrameSignal.chunkFinderEnabled=true;
    }
    public static void disable() {Request r=request;update(false,r.settings,r.threads,r.perTick,r.alerts,r.chat,r.clear,0);}
    private static void reset() {
        for(Pending p:pending.values())p.future.cancel(true);pending.clear();
        if(workers!=null)workers.shutdownNow();workers=null;
        lightScan.clear();finds.clear();activityFinds.clear();ChunkActivity.clear();sampled.clear();alertTimes.clear();scanned=0;limited=false;level=player=null;nextPublish=0;++world;++revision;view=new View(2);
    }
    public static void tick(Object client) {
        Request r=request;Api a=api;long now=System.nanoTime();
        if(!r.enabled||now>r.deadline||a==null) {
            if(level!=null||workers!=null)reset();fault=false;view=new View(a==null?0:1);FrameSignal.chunkFinderEnabled=false;return;
        }
        if(fault&&generation==r.generation)return;
        try {
            Object current=a.s.get("level",client),local=a.s.get("player",client);
            if(current==null||local==null) {if(level!=null)reset();view=new View(2);return;}
            boolean newWorld=current!=level||local!=player;
            if(newWorld||generation!=r.generation) {
                int threads=workers==null?r.threads:workers.getCorePoolSize(); // thread changes apply on next enable
                reset();level=current;player=local;generation=r.generation;clearId=r.clear;fault=false;
                workers=new ThreadPoolExecutor(threads,threads,0,TimeUnit.SECONDS,new ArrayBlockingQueue<>(12),task->{Thread t=new Thread(task,"Keim chunk analysis");t.setDaemon(true);return t;},new ThreadPoolExecutor.AbortPolicy());
            }
            AtomicReferenceArray<?> cache=(AtomicReferenceArray<?>)a.s.get("chunks",a.s.get("storage",a.s.call("source",level)));
            if(cache.length()>65536)throw new IllegalStateException("Unexpected loaded chunk cache size");
            Map<Long,Object> loaded=new HashMap<>();
            for(int i=0;i<cache.length();++i) {Object c=cache.get(i);if(c==null)continue;Object pos=a.s.call("position",c);loaded.put(key((int)a.s.number("cx",pos),(int)a.s.number("cz",pos)),c);}
            double px=((Number)a.s.call("x",local)).doubleValue(),pz=((Number)a.s.call("z",local)).doubleValue();
            if(!Double.isFinite(px)||!Double.isFinite(pz))return;
            boolean changed=false;
            if(r.settings[11]!=0)lightScan.tick(a,level,loaded,(int)Math.floor(px/16),(int)Math.floor(pz/16),r.settings[12],r.settings[13],r.settings[14]!=0,now);
            if(r.settings[8]!=0&&ChunkActivity.available&&AntiVanishPackets.installed) {
                Object connection=a.s.call("activityConnection",client);
                if(connection!=null)ChunkActivity.bind(a.s.call("activityNetwork",connection),r.deadline);
                for(var hit:ChunkActivity.drain(a,loaded,(int)Math.floor(px/16),(int)Math.floor(pz/16),r.settings[9]!=0,r.settings[10]!=0,now)) {
                    int cx=Math.floorDiv(hit.x(),16),cz=Math.floorDiv(hit.z(),16);long key=key(cx,cz);
                    var previous=activityFinds.get(key);int evidence=(previous==null?0:previous.evidence())|(1<<hit.signal());
                    Object chunk=loaded.get(key);if(chunk==null)continue;
                    int min=((Number)a.s.call("minY",chunk)).intValue(),max=min+((Object[])a.s.call("sections",chunk)).length*16;
                    var result=new ChunkFinderDetector.Result(cx,cz,16,0,0,min,max,evidence,List.of(new ChunkFinderDetector.Point(hit.x(),hit.y(),hit.z(),hit.signal())));
                    if(previous==null&&activityFinds.size()>=2048){limited=true;continue;}
                    activityFinds.put(key,result);Saved old=finds.get(key);
                    save(merge(old==null?null:old.result,result),key,local,a,r,now);changed=true;
                }
                limited|=ChunkActivity.limited();
            }

            Iterator<Map.Entry<Long,Pending>> it=pending.entrySet().iterator();
            while(it.hasNext()) {
                var entry=it.next();Pending p=entry.getValue();
                if(loaded.get(entry.getKey())!=p.identity) {p.future.cancel(true);it.remove();continue;}
                if(!p.future.isDone())continue;
                it.remove();ChunkFinderDetector.Result result=p.future.get();++scanned;sampled.put(entry.getKey(),now);changed=true;
                result=merge(result,activityFinds.get(entry.getKey()));
                save(result,entry.getKey(),local,a,r,now);
            }
            sampled.keySet().removeIf(k->!loaded.containsKey(k));
            List<Map.Entry<Long,Object>> ordered=new ArrayList<>(loaded.entrySet());
            // Unseen chunks always precede rescans, so large render distances cannot starve the outer rings.
            ordered.sort(Comparator.<Map.Entry<Long,Object>>comparingInt(e->sampled.containsKey(e.getKey())?1:0)
                .thenComparingLong(e->sampled.getOrDefault(e.getKey(),0L)).thenComparingDouble(e->distance(e.getKey(),px,pz)));
            long budget=System.nanoTime()+2_000_000L;int queued=0;
            Map<Long,int[]> entities=null;
            for(var e:r.settings[8]==1?List.<Map.Entry<Long,Object>>of():ordered) {
                if(queued>=r.perTick||pending.size()>=12||System.nanoTime()>=budget)break;
                if(pending.containsKey(e.getKey())||now-sampled.getOrDefault(e.getKey(),Long.MIN_VALUE/2)<10_000_000_000L)continue;
                if(entities==null)entities=a.entities(level);
                int cx=(int)(e.getKey()>>32),cz=(int)(long)e.getKey();
                Snapshot snapshot=a.capture(loaded,cx,cz);
                int[] mobs=entities.getOrDefault(e.getKey(),new int[4]);boolean end=a.s.call("dimension",level).toString().contains("the_end");
                Future<ChunkFinderDetector.Result> future=workers.submit(()->ChunkFinderDetector.analyze(snapshot,cx,cz,snapshot.minY,snapshot.maxY,end,r.settings,mobs,snapshot.hives));
                pending.put(e.getKey(),new Pending(cx,cz,e.getValue(),future));++queued;
            }
            if(changed||now>=nextPublish) {publish(px,pz);nextPublish=now+250_000_000L;}
        } catch(Throwable e) {reset();fault=true;generation=r.generation;view=new View(4);System.err.println("[mcov] Chunkfinder stopped: "+e);}
    }
    private static ChunkFinderDetector.Result merge(ChunkFinderDetector.Result classic,ChunkFinderDetector.Result activity) {
        if(classic==null)return activity;if(activity==null)return classic;
        List<ChunkFinderDetector.Point> points=new ArrayList<>(classic.blocks());points.removeIf(p->p.signal()>=25);points.addAll(activity.blocks());
        return new ChunkFinderDetector.Result(classic.x(),classic.z(),classic.categories()|activity.categories(),classic.score(),classic.chance(),classic.minY(),classic.maxY(),classic.evidence()|activity.evidence(),List.copyOf(points));
    }
    private static void save(ChunkFinderDetector.Result result,long key,Object local,Api a,Request r,long now)throws ReflectiveOperationException {
        if(result==null||result.categories()==0){finds.remove(key);return;}
        Saved previous=finds.get(key);long alert=previous==null?0:previous.alert;
        if(previous==null) {
            while(!alertTimes.isEmpty()&&now-alertTimes.peekFirst()>=60_000_000_000L)alertTimes.removeFirst();
            if(alertTimes.size()<r.alerts){alertTimes.addLast(now);alert=++alertSerial;if(r.chat)a.chat(local,"[Keim] Chunkfinder: "+description(result)+" at "+(result.x()*16+8)+", "+(result.z()*16+8));}
        }
        if(previous!=null||finds.size()<2048)finds.put(key,new Saved(result,alert));else limited=true;
    }
    static double distance(long k,double x,double z) {return Math.hypot((int)(k>>32)*16.+8-x,(int)k*16.+8-z);}
    static String description(ChunkFinderDetector.Result r) {
        List<String> names=new ArrayList<>();if((r.categories()&1)!=0)names.add("placed blocks");if((r.categories()&2)!=0)names.add("base "+r.score()+" pts");if((r.categories()&4)!=0)names.add("farm "+r.chance()+"%");if((r.categories()&8)!=0)names.add("fully grown amethyst cluster");if((r.categories()&16)!=0)names.add("underground activity");return String.join(", ",names);
    }
    private static void publish(double x,double z) {
        List<Saved> sorted=new ArrayList<>(finds.values());sorted.sort(Comparator.comparingDouble(f->distance(key(f.result.x(),f.result.z()),x,z)));
        int[] rows=new int[sorted.size()*8];long[] alerts=new long[sorted.size()];List<Integer> blocks=new ArrayList<>();
        for(var point:lightScan.visible){blocks.add(point.x());blocks.add(point.y());blocks.add(point.z());blocks.add(point.signal());}
        for(int i=0;i<sorted.size();++i) {Saved s=sorted.get(i);var f=s.result;int o=i*8;
            rows[o]=f.x();rows[o+1]=f.z();rows[o+2]=f.categories();rows[o+3]=f.score();rows[o+4]=f.chance();rows[o+5]=f.minY();rows[o+6]=f.maxY();rows[o+7]=f.evidence();alerts[i]=s.alert;
            for(var b:f.blocks()) {if(blocks.size()>=4000)break;blocks.add(b.x());blocks.add(b.y());blocks.add(b.z());blocks.add(b.signal()|(b.kind()<<8));}
        }
        view=new View(3,scanned,pending.size(),world,++revision,limited,rows,blocks.stream().mapToInt(Integer::intValue).toArray(),alerts,sampled.keySet().stream().limit(4096).mapToLong(Long::longValue).toArray());
    }
    static final class LightScan {
        record Section(int x,int y,int z) {}
        final ArrayDeque<Section> todo=new ArrayDeque<>();
        final List<ChunkFinderDetector.Point> building=new ArrayList<>();
        List<ChunkFinderDetector.Point> visible=List.of();
        long next;
        void clear(){todo.clear();building.clear();visible=List.of();next=0;}
        static int value(byte[] data,int i){return data==null?0:(data[i>>1]>>((i&1)*4))&15;}
        void tick(Api a,Object level,Map<Long,Object> loaded,int px,int pz,int radius,int minimum,boolean denoise,long now)throws ReflectiveOperationException {
            visible=visible.stream().filter(p->loaded.containsKey(key(Math.floorDiv(p.x(),16),Math.floorDiv(p.z(),16)))).toList();
            if(todo.isEmpty()){
                if(now<next)return;
                building.clear();
                List<Long> ordered=new ArrayList<>(loaded.keySet());
                ordered.sort(Comparator.comparingDouble(k->distance(k,px*16.,pz*16.)));
                for(long k:ordered){int x=(int)(k>>32),z=(int)k;
                    if(Math.abs(x-px)>radius||Math.abs(z-pz)>radius)continue;
                    int bottom=((Number)a.s.call("minY",loaded.get(k))).intValue();
                    for(int y=Math.max(-32,Math.floorDiv(bottom,16));y<0;++y)todo.add(new Section(x,y,z));
                }
            }
            Object engine=a.s.call("lightEngine",a.s.call("source",level));
            Object block=a.s.call("lightLayer",engine,a.s.get("blockLayer",null)),sky=a.s.call("lightLayer",engine,a.s.get("skyLayer",null));
            long stop=System.nanoTime()+2_000_000L;int processed=0;
            while(!todo.isEmpty()&&processed++<4&&System.nanoTime()<stop&&building.size()<500){
                Section q=todo.removeFirst();if(!loaded.containsKey(key(q.x,q.z)))continue;
                Object pos=a.s.call("sectionPos",null,q.x,q.y,q.z);
                Object bd=a.s.call("lightData",block,pos),sd=a.s.call("lightData",sky,pos);if(bd==null)continue;
                byte[] b=((byte[])a.s.call("lightBytes",bd)).clone(),sk=sd==null?null:((byte[])a.s.call("lightBytes",sd)).clone();
                // Unknown sky data is not treated as darkness.
                if(sk==null)continue;
                for(int i=0;i<4096&&building.size()<500;++i){
                    int v=value(b,i);if(v<minimum||v<=value(sk,i))continue;
                    int x=i&15,z=(i>>4)&15,y=i>>8;boolean edge=false,strong=false;
                    for(int[] d:ChunkFinderDetector.SIDES){
                        int xx=x+d[0],yy=y+d[1],zz=z+d[2];
                        if(xx<0||xx>15||yy<0||yy>15||zz<0||zz>15){edge=true;continue;}
                        int j=yy*256+zz*16+xx,n=value(b,j);
                        if(n<minimum||n<=value(sk,j))edge=true;
                        if(n>v)strong=true;
                    }
                    if(!edge||denoise&&v<=minimum+2&&!strong)continue;
                    building.add(new ChunkFinderDetector.Point(q.x*16+x,q.y*16+y,q.z*16+z,28|(v<<8)));
                }
            }
            if(todo.isEmpty()||building.size()>=500){visible=List.copyOf(building);todo.clear();next=now+1_000_000_000L;}
        }
    }
    static final class Api {
        final BaseTextureBridge.Symbols s;final MethodHandle cell;
        final ConcurrentMap<Object,ChunkFinderDetector.Cell> decoded=new ConcurrentHashMap<>();
        Api(ClassLoader loader,String[][] table) throws ReflectiveOperationException {
            s=new BaseTextureBridge.Symbols(loader,table);cell=MethodHandles.lookup().unreflect(s.methods.get("cell")).asType(MethodType.methodType(Object.class,Object.class,int.class,int.class,int.class));
        }
        void chat(Object player,String message) throws ReflectiveOperationException {
            Object text=s.call("literal",null,message);if(s.methods.get("chat").getParameterCount()==2)s.call("chat",player,text,false);else s.call("chat",player,text);
        }
        Map<Long,int[]> entities(Object level) throws ReflectiveOperationException {
            Map<Long,int[]> result=new HashMap<>();int n=0;
            for(Object e:(Iterable<?>)s.call("entities",level)) {
                if(++n>16384) {limited=true;break;}
                double x=((Number)s.call("x",e)).doubleValue(),y=((Number)s.call("y",e)).doubleValue(),z=((Number)s.call("z",e)).doubleValue();
                if(!Double.isFinite(x+y+z))continue;
                int[] count=result.computeIfAbsent(key((int)Math.floor(x/16),(int)Math.floor(z/16)),k->new int[4]);
                if(y<0&&s.type("Item").isInstance(e))++count[0];if(s.type("Mob").isInstance(e))++count[1];
                if(y<0&&s.type("Bear").isInstance(e))++count[2];if(s.type("Bee").isInstance(e))++count[3];
            }return result;
        }
        Snapshot capture(Map<Long,Object> loaded,int cx,int cz) throws ReflectiveOperationException {
            Map<Long,Column> columns=new HashMap<>();Set<Long> hives=new HashSet<>();int min=0,max=0;
            for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
                Object chunk=loaded.get(key(cx+dx,cz+dz));if(chunk==null)continue;
                int bottom=((Number)s.call("minY",chunk)).intValue();Object[] sections=(Object[])s.call("sections",chunk);
                if(sections.length>256||bottom< -4096||bottom+sections.length*16>4096)throw new IllegalStateException("Unsupported dimension height");
                Object[] states=new Object[sections.length],biomes=new Object[sections.length];
                for(int i=0;i<sections.length;++i)if(sections[i]!=null) {states[i]=s.call("copy",s.call("states",sections[i]));biomes[i]=s.call("copy",s.call("biomes",sections[i]));}
                columns.put(key(cx+dx,cz+dz),new Column(bottom,states,biomes));if(dx==0&&dz==0){min=bottom;max=bottom+sections.length*16;}
                if(dx==0&&dz==0)for(var e:((Map<?,?>)s.call("blockEntities",chunk)).entrySet()) if(s.type("Hive").isInstance(e.getValue())&&((Number)s.call("bees",e.getValue())).intValue()>0) {
                    // BlockPos string has no stable namespace; get coordinates through the audited position methods.
                    Object pos=e.getKey();int xx=((Number)s.call("posX",pos)).intValue(),yy=((Number)s.call("posY",pos)).intValue(),zz=((Number)s.call("posZ",pos)).intValue();hives.add(ChunkFinderDetector.pack(xx,yy,zz));
                }
            }return new Snapshot(this,columns,hives,min,max);
        }
    }
    private record Column(int min,Object[] states,Object[] biomes) {}
    static final class Snapshot implements ChunkFinderDetector.Grid {
        final Api a;final Map<Long,Column> columns;final Set<Long> hives;final int minY,maxY;
        Snapshot(Api a,Map<Long,Column> columns,Set<Long> hives,int min,int max) {this.a=a;this.columns=columns;this.hives=hives;minY=min;maxY=max;}
        Object read(int x,int y,int z,boolean biome) {
            Column c=columns.get(key(Math.floorDiv(x,16),Math.floorDiv(z,16)));if(c==null||y<c.min||y>=c.min+c.states.length*16)return null;
            Object container=(biome?c.biomes:c.states)[(y-c.min)>>4];if(container==null)return null;
            try {return (Object)a.cell.invokeExact(container,biome?(x&15)>>2:x&15,biome?(y&15)>>2:y&15,biome?(z&15)>>2:z&15);}
            catch(Throwable e) {throw new IllegalStateException("Palette snapshot read",e);}
        }
        public ChunkFinderDetector.Cell at(int x,int y,int z) {
            Object state=read(x,y,z,false);return state==null?new ChunkFinderDetector.Cell(0,false):a.decoded.computeIfAbsent(state,k->ChunkFinderDetector.classify(k.toString()));
        }
        public String biome(int x,int y,int z) {Object b=read(x,y,z,true);return b==null?"":b.toString();}
    }
}
