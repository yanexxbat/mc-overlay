package mcov;
import java.util.*;
final class ChunkActivityCheck {
    static void check(boolean b,String text){if(!b)throw new AssertionError(text);}
    static byte[] data(int value){byte[] b=new byte[2048];Arrays.fill(b,(byte)(value|(value<<4)));return b;}
    static final BitSet ONE=BitSet.valueOf(new long[]{1});
    static final ChunkActivityTracker.Reader MASKED=(x,y,z)->"Block{minecraft:deepslate}[axis=y]";
    static ChunkActivityTracker.Hit light(ChunkActivityTracker t,byte[] b,boolean initial,boolean sky,int px,int pz,ChunkActivityTracker.Reader reader){
        return t.light(-2,-3,-1,ONE,new BitSet(),List.of(b),sky,initial,px,pz,reader,1);
    }
    static void run() {
        var t=new ChunkActivityTracker();byte[] first=data(7);
        check(light(t,first,true,false,0,0,MASKED)==null,"chunk load must not flag masked deepslate");
        first[0]=0; // Ownership: changing the packet's buffer must not modify the baseline.
        check(light(t,data(7),false,false,0,0,MASKED)==null,"light packet was not copied");
        var hit=light(t,data(8),false,false,0,0,MASKED);
        check(hit!=null&&hit.x()==-32&&hit.y()==-16&&hit.z()==-48&&hit.signal()==25,"light update / negative coordinates");
        check(light(t,data(9),false,false,-2,-3,MASKED)==null,"own chunk must be excluded");
        check(light(t,data(10),false,false,0,0,(x,y,z)->"Block{minecraft:torch}")==null,"ordinary torch false positive");
        t.clear();check(light(t,data(3),false,false,0,0,MASKED)==null,"first incremental update is only a baseline");
        check(light(t,data(4),false,false,0,0,(x,y,z)->"Block{minecraft:amethyst_cluster}")==null,"amethyst false positive");
        check(light(t,data(5),false,false,0,0,(x,y,z)->"Block{minecraft:deepslate_redstone_ore}[lit=true]")==null,"ore glow false positive");
        check(light(t,data(0),false,false,0,0,MASKED)==null,"dark transition is not activity");
        check(light(t,data(8),false,false,0,0,MASKED)==null,"initial illumination is not activity");
        check(light(t,data(8),true,true,0,0,MASKED)==null,"sky baseline");
        check(light(t,data(9),false,true,-1,-2,MASKED)==null,"near-own sky chunk excluded");
        check(light(t,data(10),false,true,0,0,MASKED).signal()==26,"sky update");
        t.retain(Set.of());check(t.size()==0,"unloaded baselines retained");
        check(light(t,data(11),false,false,0,0,MASKED)==null,"reload produced false activity");
        String off="Block{minecraft:repeater}[powered=false]",on="Block{minecraft:repeater}[powered=true]";
        check(t.block(-31,-10,-47,off,0,0,true,1)==null,"first block observation is baseline");
        check(t.block(-31,-10,-47,off,0,0,true,1)==null,"duplicate block update");
        check(t.block(-31,-10,-47,on,0,0,true,1).signal()==27,"redstone transition missing");
        check(t.block(-31,-10,-47,off,0,0,false,1)==null,"redstone toggle ignored");
        check(t.block(-31,-10,-47,on,-2,-3,true,1)==null,"own redstone update");
        check(t.block(-31,10,-47,on,0,0,true,1)==null,"aboveground redstone");
        check(t.light(1,1,-1,ONE,new BitSet(),List.of(new byte[1]),false,false,0,0,MASKED,1)==null&&t.limited,"malformed light update");
        t.clear();for(int x=0;x<600;++x)t.light(x,0,-1,ONE,new BitSet(),List.of(data(7)),false,true,0,0,MASKED,1);
        check(t.size()==512&&t.limited,"light baseline cache is unbounded");
        System.out.println("Activity light/redstone, baseline ownership, natural filters, own-chunk, reload and bounds passed");
        ChunkActivityTracker.Reader terrain=(x,y,z)->y==70?"Block{minecraft:dirt}":"Block{minecraft:air}";
        check(terrain.underground(0,40,0),"terrain above positive Y must count as underground");
        check(!terrain.underground(0,71,0),"above terrain must be excluded");
        check(!((ChunkActivityTracker.Reader)(x,y,z)->"Block{minecraft:air}").underground(0,-20,0),"negative Y alone is not underground");
        check(!((ChunkActivityTracker.Reader)(x,y,z)->null).underground(0,-20,0),"unknown terrain must fail closed");
        check(!((ChunkActivityTracker.Reader)(x,y,z)->"Block{minecraft:oak_leaves}").underground(0,40,0),"tree canopy is not terrain");
        t.clear();
        t.light(-2,-3,2,ONE,new BitSet(),List.of(data(7)),false,true,0,0,terrain,1);
        check(t.light(-2,-3,2,ONE,new BitSet(),List.of(data(8)),false,false,0,0,terrain,1)!=null,"buried light above Y zero missing");
    }
}
