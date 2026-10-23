package mcov;

import java.util.*;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.function.BiConsumer;

/** Shares the audited incoming packet tap, independently of Anti Vanish being enabled. */
final class ChunkActivity {
    private record Event(Object network,Object packet,long generation) {}
    private static final ArrayBlockingQueue<Event> queue=new ArrayBlockingQueue<>(128);
    private static volatile Object network;
    private static volatile long generation,deadline;
    private static volatile boolean overflow;
    private static volatile Set<Class<?>> types=Set.of();
    static final ChunkActivityTracker tracker=new ChunkActivityTracker();
    static boolean available;
    static int status(){return !available||!AntiVanishPackets.installed?0:network==null?1:2;}
    static void configure(ChunkFinderBridge.Api a) {
        Set<Class<?>> found=new HashSet<>();
        for(String name:List.of("Light","Chunk","Block","Section","Respawn","Bundle")) {
            Class<?> c=a.s.type("Activity"+name);if(c!=null)found.add(c);
        }
        types=Set.copyOf(found);available=found.size()==6;
    }
    static void observe(Object source,Object packet,boolean outbound) {
        long token=generation;
        if(outbound||source!=network||packet==null||System.nanoTime()>deadline)return;
        boolean relevant=false;for(Class<?> c:types)if(c.isInstance(packet)){relevant=true;break;}
        if(relevant&&!queue.offer(new Event(source,packet,token)))overflow=true;
    }
    static void clear(){network=null;deadline=0;++generation;queue.clear();tracker.clear();overflow=false;}
    static void bind(Object source,long lease){if(source!=network){clear();network=source;}deadline=lease;}
    static boolean limited(){return overflow||tracker.limited;}
    static List<ChunkActivityTracker.Hit> drain(ChunkFinderBridge.Api a,Map<Long,Object> loaded,int px,int pz,boolean sky,boolean redstone,long now)throws ReflectiveOperationException {
        List<ChunkActivityTracker.Hit> hits=new ArrayList<>();
        if(overflow){queue.clear();tracker.clear();overflow=false;tracker.limited=true;return hits;}
        tracker.retain(loaded.keySet());
        // Cache palette references only for this client tick. No live objects reach worker threads.
        Map<Long,Object[]> sections=new HashMap<>();Map<Long,Integer> bottoms=new HashMap<>();
        ChunkActivityTracker.Reader reader=(x,y,z)->{
            long key=ChunkFinderBridge.key(Math.floorDiv(x,16),Math.floorDiv(z,16));Object chunk=loaded.get(key);if(chunk==null)return null;
            try {
                Object[] palettes=sections.get(key);
                if(palettes==null){Object[] ss=(Object[])a.s.call("sections",chunk);palettes=new Object[ss.length];for(int i=0;i<ss.length;++i)if(ss[i]!=null)palettes[i]=a.s.call("states",ss[i]);sections.put(key,palettes);bottoms.put(key,((Number)a.s.call("minY",chunk)).intValue());}
                int index=Math.floorDiv(y-bottoms.get(key),16);if(index<0||index>=palettes.length||palettes[index]==null)return null;
                return ((Object)a.cell.invokeExact(palettes[index],x&15,y&15,z&15)).toString();
            }catch(Throwable e){throw new IllegalStateException("Activity block context",e);}
        };
        long end=System.nanoTime()+2_000_000L;int remaining=32;
        while(remaining-->0&&System.nanoTime()<end) {
            Event e=queue.poll();if(e==null)break;if(e.network!=network||e.generation!=generation)continue;
            if(is(a,"Bundle",e.packet)) {
                int count=0;for(Object inner:(Iterable<?>)a.s.call("activityBundle",e.packet)){
                    if(++count>512){tracker.clear();tracker.limited=true;break;}
                    packet(a,inner,loaded,px,pz,sky,redstone,now,reader,hits);
                }
            } else packet(a,e.packet,loaded,px,pz,sky,redstone,now,reader,hits);
        }
        return hits;
    }
    private static boolean is(ChunkFinderBridge.Api a,String type,Object packet){Class<?> c=a.s.type("Activity"+type);return c!=null&&c.isInstance(packet);}
    @SuppressWarnings("unchecked")
    private static void packet(ChunkFinderBridge.Api a,Object packet,Map<Long,Object> loaded,int px,int pz,boolean sky,boolean redstone,long now,ChunkActivityTracker.Reader reader,List<ChunkActivityTracker.Hit> hits)throws ReflectiveOperationException {
        if(is(a,"Respawn",packet)){queue.clear();tracker.clear();return;}
        boolean initial=is(a,"Chunk",packet);
        if(initial||is(a,"Light",packet)) {
            String kind=initial?"Chunk":"Light";
            int cx=((Number)a.s.call("activity"+kind+"X",packet)).intValue(),cz=((Number)a.s.call("activity"+kind+"Z",packet)).intValue();
            Object chunk=loaded.get(ChunkFinderBridge.key(cx,cz));if(chunk==null)return;
            int bottom=Math.floorDiv(((Number)a.s.call("minY",chunk)).intValue(),16)-1;
            if(initial)tracker.baseline(cx,cz);
            Object data=a.s.call("activity"+kind+"Data",packet);
            for(String layer:sky?List.of("Block","Sky"):List.of("Block")) {
                var hit=tracker.light(cx,cz,bottom,(BitSet)a.s.call("activity"+layer+"YMask",data),(BitSet)a.s.call("activityEmpty"+layer,data),
                    (List<byte[]>)a.s.call("activity"+layer+"Updates",data),layer.equals("Sky"),initial,px,pz,reader,now);
                if(hit!=null)hits.add(hit);
            }
        } else if(is(a,"Block",packet)||is(a,"Section",packet)) {
            BiConsumer<Object,Object> consume=(pos,state)->{
                try {
                    int x=((Number)a.s.call("posX",pos)).intValue(),y=((Number)a.s.call("posY",pos)).intValue(),z=((Number)a.s.call("posZ",pos)).intValue();
                    if(!loaded.containsKey(ChunkFinderBridge.key(Math.floorDiv(x,16),Math.floorDiv(z,16))))return;
                    var hit=tracker.block(x,y,z,state.toString(),px,pz,redstone,now);if(hit!=null&&hits.size()<256)hits.add(hit);
                }catch(ReflectiveOperationException e){throw new IllegalStateException(e);}
            };
            if(is(a,"Block",packet))consume.accept(a.s.call("activityBlockPos",packet),a.s.call("activityBlockState",packet));
            else a.s.call("activitySectionBlocks",packet,consume);
        }
    }
}
