package mcov;

import java.util.*;
import java.util.concurrent.ArrayBlockingQueue;

/** Packet hooks only enqueue references. All game access happens on the client tick. */
public final class AntiVanishPackets {
    private record Event(Object connection,Object packet,boolean outbound,long generation) {}
    private static final ArrayBlockingQueue<Event> queue=new ArrayBlockingQueue<>(4096);
    private static volatile Object connection;
    private static volatile long generation;
    private static volatile long deadline;
    private static volatile boolean overflow;
    private static volatile Set<Class<?>> incoming=Set.of(),outgoing=Set.of();
    static void configure(BaseTextureBridge.Symbols symbols) {
        Set<Class<?>> in=new HashSet<>(),out=new HashSet<>();
        for(String key:List.of("Remove","Update","Chat","Suggestions","Sound","EntitySound","Particles","Explosion","Bundle"))if(symbols.type(key)!=null)in.add(symbols.type(key));
        for(String key:List.of("UseOn","Use","Dig","Interact","Swing"))if(symbols.type(key)!=null)out.add(symbols.type(key));
        incoming=Set.copyOf(in);outgoing=Set.copyOf(out);
    }
    public static volatile boolean installed;
    static boolean overflowed() { boolean result=overflow;overflow=false;return result; }
    public static void observe(Object network,Object packet,boolean outbound) {
        ChunkActivity.observe(network,packet,outbound);
        long token=generation;
        if(network!=connection||System.nanoTime()>deadline||packet==null)return;
        boolean relevant=false;for(Class<?> type:outbound?outgoing:incoming)if(type.isInstance(packet)){relevant=true;break;}
        if(!relevant)return;
        if(!queue.offer(new Event(network,packet,outbound,token)))overflow=true;
    }
    static void bind(Object network,long lease) {
        if(connection!=network){clear();connection=network;}
        deadline=lease;
    }
    static void clear(){connection=null;deadline=0;++generation;queue.clear();overflow=false;}
    static void drain(AntiVanishBridge.Api a,Object world,Object local,AntiVanishTracker tracker,long now,boolean sensors)throws ReflectiveOperationException {
        int remaining=512,sensorCount=0;long sensorDeadline=System.nanoTime()+4_000_000L;
        while(remaining>0){Event e=queue.poll();if(e==null)break;--remaining;
            if(e.connection!=connection||e.generation!=generation)continue;
            if(!e.outbound&&a.is("Bundle",e.packet)) {
                for(Object inner:(Iterable<?>)a.s.call("bundle",e.packet)){
                    if(remaining--<=0){overflow=true;break;}
                    boolean sensor=a.is("Sound",inner)||a.is("EntitySound",inner)||a.is("Particles",inner);
                    boolean inspect=!sensor||++sensorCount<=16&&System.nanoTime()<sensorDeadline;
                    if(!inspect)overflow=true;
                    a.observe(inner,false,world,local,tracker,now,sensors&&inspect);
                }
            }else {
                boolean sensor=a.is("Sound",e.packet)||a.is("EntitySound",e.packet)||a.is("Particles",e.packet);
                boolean inspect=!sensor||++sensorCount<=16&&System.nanoTime()<sensorDeadline;
                if(!inspect)overflow=true;
                a.observe(e.packet,e.outbound,world,local,tracker,now,sensors&&inspect);
            }
        }
    }
}
