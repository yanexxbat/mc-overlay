package mcov;

import java.util.*;

/** Stateful, bounded analysis of received updates. Initial data is never activity. */
final class ChunkActivityTracker {
    static final int LIGHT=25, SKY=26, REDSTONE=27;
    record Pos(int x,int y,int z) {}
    record Hit(int x,int y,int z,int signal) {}
    interface Reader {
        String state(int x,int y,int z);
        // Require terrain above the sample, not merely a negative Y coordinate.
        // Unknown/unloaded columns fail closed; leaves and constructed roofs do not count.
        default boolean underground(int x,int y,int z) {
            for(int above=y+1;above<=y+512;++above) {
                String block=state(x,above,z);if(block==null)return false;
                if(TERRAIN.contains(id(block)))return true;
            }
            return false;
        }
    }
    private static final Set<String> TERRAIN=Set.of("stone","deepslate","dirt","grass_block","coarse_dirt","rooted_dirt","podzol","mycelium","sand","red_sand","sandstone","red_sandstone","gravel","clay","terracotta","mud","packed_mud","snow_block","netherrack","end_stone","bedrock","granite","diorite","andesite","tuff","calcite");
    private static final Set<String> REDSTONE_IDS=Set.of("redstone_wire","repeater","comparator","redstone_torch","redstone_wall_torch","redstone_lamp","piston","sticky_piston","piston_head","moving_piston","observer","daylight_detector","lever","tripwire_hook","target","dispenser","dropper","note_block");
    private static final Set<String> LAMP_IDS=Set.of("torch","wall_torch","soul_torch","soul_wall_torch","lantern","soul_lantern","campfire","soul_campfire");
    private static final byte[] DARK=new byte[2048];
    private static final class Chunk {
        final Map<Integer,byte[]> block=new HashMap<>(),sky=new HashMap<>();
        final Map<Pos,String> states=new HashMap<>();
    }
    private final LinkedHashMap<Long,Chunk> chunks=new LinkedHashMap<>(64,.75f,true);
    private final Map<Pos,Long> removedLamps=new HashMap<>();
    boolean limited;
    int size(){return chunks.size();}
    void clear(){chunks.clear();removedLamps.clear();limited=false;}
    void retain(Set<Long> loaded){chunks.keySet().retainAll(loaded);}
    void baseline(int x,int z){chunks.remove(ChunkFinderBridge.key(x,z));}
    private Chunk chunk(int x,int z) {
        long key=ChunkFinderBridge.key(x,z);Chunk c=chunks.get(key);
        if(c==null){if(chunks.size()>=512){chunks.remove(chunks.keySet().iterator().next());limited=true;}c=new Chunk();chunks.put(key,c);}
        return c;
    }
    static String id(String state) {
        if(state==null)return "";int start=state.indexOf("minecraft:");if(start<0)return "";
        start+=10;int end=start;while(end<state.length()&&(Character.isLetterOrDigit(state.charAt(end))||state.charAt(end)=='_'))++end;
        return state.substring(start,end);
    }
    static boolean redstone(String state) {
        String id=id(state);
        return REDSTONE_IDS.contains(id)
            ||id.endsWith("_button")||id.endsWith("_pressure_plate")||id.endsWith("_trapdoor")||id.endsWith("_door");
    }
    static boolean lamp(String state) {
        return LAMP_IDS.contains(id(state))
            ||id(state).endsWith("candle")||id(state).endsWith("candle_cake");
    }
    Hit block(int x,int y,int z,String state,int playerX,int playerZ,boolean enabled,long now) {
        if(y>=0||state==null)return null;
        Chunk c=chunk(Math.floorDiv(x,16),Math.floorDiv(z,16));Pos pos=new Pos(x,y,z);
        if(c.states.size()>=512&&!c.states.containsKey(pos)){c.states.clear();limited=true;}
        String old=c.states.put(pos,state);
        if(old!=null&&lamp(old)&&!lamp(state)){if(removedLamps.size()>=512)removedLamps.clear();removedLamps.put(pos,now+5_000_000_000L);}
        if(!enabled||old==null||old.equals(state)||Math.floorDiv(x,16)==playerX&&Math.floorDiv(z,16)==playerZ)return null;
        return redstone(old)||redstone(state)?new Hit(x,y,z,REDSTONE):null;
    }
    private static boolean dark(byte[] data){for(byte b:data)if(b!=0)return false;return true;}
    private static int nibble(byte[] data,int index){return (data[index>>1]>>((index&1)*4))&15;}
    private int light(int x,int y,int z,Map<Integer,byte[]> previous,int cx,int cz) {
        int xx=Math.floorDiv(x,16),zz=Math.floorDiv(z,16),section=Math.floorDiv(y,16);
        Chunk c=chunks.get(ChunkFinderBridge.key(xx,zz));if(c==null)return 0;
        int index=(y&15)*256+(z&15)*16+(x&15);
        int value=nibble(c.block.getOrDefault(section,DARK),index);
        if(xx==cx&&zz==cz&&previous.containsKey(section))value=Math.max(value,nibble(previous.get(section),index));
        return value;
    }
    // Follow a local light gradient to known natural/ordinary sources. Bounded per candidate.
    private boolean ordinarySource(Pos start,int value,Reader reader,Map<Integer,byte[]> previous,int cx,int cz,long now) {
        for(var e:removedLamps.entrySet())if(e.getValue()>now&&Math.abs(e.getKey().x-start.x)+Math.abs(e.getKey().y-start.y)+Math.abs(e.getKey().z-start.z)<=14-value)return true;
        long stop=System.nanoTime()+1_000_000L;
        ArrayDeque<Pos> todo=new ArrayDeque<>();Set<Pos> seen=new HashSet<>();todo.add(start);
        while(!todo.isEmpty()&&seen.size()<128) {
            if(System.nanoTime()>stop){limited=true;return true;}
            Pos p=todo.removeFirst();if(!seen.add(p))continue;
            String state=reader.state(p.x,p.y,p.z),id=id(state);
            if(value<=5&&(id.contains("amethyst")))return true;
            if(value<=9&&(id.equals("redstone_ore")||id.equals("deepslate_redstone_ore")))return true;
            if(value<=14&&lamp(state))return true;
            int level=light(p.x,p.y,p.z,previous,cx,cz);
            if(level>=15)continue;
            for(int[] d:ChunkFinderDetector.SIDES) {
                Pos next=new Pos(p.x+d[0],p.y+d[1],p.z+d[2]);
                if(!seen.contains(next)&&light(next.x,next.y,next.z,previous,cx,cz)>level)todo.add(next);
            }
        }
        return false;
    }
    Hit light(int cx,int cz,int bottomSection,BitSet mask,BitSet empty,List<byte[]> updates,
              boolean sky,boolean initial,int playerX,int playerZ,Reader reader,long now) {
        if(mask.length()>258||empty.length()>258||updates.size()!=mask.cardinality()) {limited=true;return null;}
        for(byte[] b:updates)if(b==null||b.length!=2048){limited=true;return null;}
        Chunk c=chunk(cx,cz);Map<Integer,byte[]> target=sky?c.sky:c.block;
        Map<Integer,byte[]> previous=new HashMap<>(),changed=new LinkedHashMap<>();int n=0;
        for(int bit=mask.nextSetBit(0);bit>=0;bit=mask.nextSetBit(bit+1)) {
            byte[] data=updates.get(n++);int sy=bottomSection+bit;
            if(sy< -32||sy>=32){limited=true;continue;}
            byte[] old=target.put(sy,data.clone());if(old!=null)previous.put(sy,old);changed.put(sy,target.get(sy));
        }
        for(int bit=empty.nextSetBit(0);bit>=0;bit=empty.nextSetBit(bit+1)) {
            int sy=bottomSection+bit;if(sy< -32||sy>=32){limited=true;continue;}
            byte[] old=target.put(sy,DARK);if(old!=null)previous.put(sy,old);
        }
        removedLamps.entrySet().removeIf(e->e.getValue()<=now);
        int radius=sky?1:0;
        if(initial||Math.abs((long)cx-playerX)<=radius&&Math.abs((long)cz-playerZ)<=radius)return null;
        int inspected=0;long stop=System.nanoTime()+2_000_000L;
        for(var e:changed.entrySet()) {
            byte[] old=previous.get(e.getKey()),data=e.getValue();
            if(old==null||dark(old)||dark(data)||Arrays.equals(old,data))continue;
            for(int i=0;i<4096;++i) {
                int was=nibble(old,i),value=nibble(data,i);if(was==value)continue;
                if(System.nanoTime()>stop||++inspected>128){limited=true;return null;}
                Pos p=new Pos(cx*16+(i&15),e.getKey()*16+(i>>8),cz*16+((i>>4)&15));
                if(reader.state(p.x,p.y,p.z)==null)continue;
                if(!reader.underground(p.x,p.y,p.z))continue;
                if(!sky&&ordinarySource(p,Math.max(was,value),reader,previous,cx,cz,now))continue;
                return new Hit(p.x,p.y,p.z,sky?SKY:LIGHT);
            }
        }
        return null;
    }
}
