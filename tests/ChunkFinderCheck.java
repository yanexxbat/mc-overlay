package mcov;
import java.util.*;
public final class ChunkFinderCheck {
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    static final class Grid implements ChunkFinderDetector.Grid {
        final Map<String,ChunkFinderDetector.Cell> cells=new HashMap<>();String biome="minecraft:plains";
        void put(int x,int y,int z,int kind){cells.put(x+","+y+","+z,new ChunkFinderDetector.Cell(kind,kind==1||kind==2));}
        public ChunkFinderDetector.Cell at(int x,int y,int z){return x< -16||x>=32||z< -16||z>=32||y< -16||y>=48?new ChunkFinderDetector.Cell(0,false):cells.getOrDefault(x+","+y+","+z,new ChunkFinderDetector.Cell(3,false));}
        public String biome(int x,int y,int z){return biome;}
    }
    static int[] defaults(){return new int[]{((1<<21)-1)&~(1<<10),1,2,4,2,8,50,5};}
    static ChunkFinderDetector.Result run(Grid g,int[] c){return ChunkFinderDetector.analyze(g,0,0,-16,48,false,c,new int[4],Set.of());}
    public static void main(String[] args){
        ChunkActivityCheck.run();
        var g=new Grid();var c=defaults();
        check(ChunkFinderDetector.classify("Block{minecraft:deepslate}[axis=z]").kind()==5,"rotated state");
        check(ChunkFinderDetector.classify("Block{minecraft:deepslate_diamond_ore}").kind()==13,"ore is not raw slate");
        check(ChunkFinderDetector.classify("Block{minecraft:oak_stairs}[waterlogged=true]").open(),"waterlogged exposure");
        g.put(4,9,4,4);check((run(g,c).categories()&1)!=0,"high slate");
        check(run(g,c).blocks().get(0).kind()==4,"evidence retains deepslate material");
        g.put(5,9,4,1);check(run(g,c).categories()==0,"exposed high slate excluded");
        c[0]&=~(1<<1);check((run(g,c).categories()&1)!=0,"ignore exposed off");
        g=new Grid();c=defaults();for(int y=-16;y< -2;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x)g.put(x,y,z,4);
        check(run(g,c).categories()==0,"solid natural deepslate below Y -2 is not placed evidence");
        g=new Grid();c=defaults();g.put(15,-5,4,5);check(run(g,c).categories()==0,"rotated minimum");g.put(15,-4,4,5);check((run(g,c).categories()&1)!=0,"rotated border uses neighbour context");
        check((run(g,c).evidence()&(1<<22))!=0,"rotated reason reported");
        c[2]=0;check(run(g,c).categories()==0,"rotated rule can be disabled without matching zero blocks");c=defaults();
        g.put(16,-5,4,2);check(run(g,c).categories()==0,"fluid across chunk boundary");
        g=new Grid();g.put(2,10,2,7);g.put(3,10,2,7);check((run(g,c).categories()&1)!=0,"end stone outside End");
        check(ChunkFinderDetector.analyze(g,0,0,-16,48,true,c,new int[4],Set.of()).categories()==0,"natural End stone");
        g=new Grid();g.put(4,-5,4,11);g.put(5,-5,4,11);check((run(g,c).categories()&2)!=0,"piston base score");
        c[0]&=~(1<<6);check(run(g,c).categories()==0,"piston toggle");
        g=new Grid();c=defaults();for(int x=2;x<8;++x)g.put(x,-5,4,9);check((run(g,c).evidence()&(1<<4))!=0,"masonry evidence");g.put(16,-5,4,25);check((run(g,c).evidence()&(1<<4))==0,"neighbour ancient city exclusion");
        g=new Grid();for(int x=2;x<8;++x)g.put(x,-5,4,8);check((run(g,c).evidence()&(1<<3))!=0,"cobble row");g.put(8,-5,4,24);check((run(g,c).evidence()&(1<<3))==0,"dungeon exclusion");
        g=new Grid();for(int y=-12;y< -3;++y)g.put(4,y,4,1);check((run(g,c).evidence()&(1<<8))!=0,"walled shaft");g.put(5,-8,4,1);check((run(g,c).evidence()&(1<<8))==0,"open cave is not shaft");
        g=new Grid();g.put(4,20,4,16);check((run(g,c).categories()&4)!=0,"powered repeater farm");
        g=new Grid();g.put(4,20,4,21);g.put(5,20,4,21);g.put(6,20,4,21);check((run(g,c).categories()&4)!=0,"workshop");g.put(16,20,4,26);check(run(g,c).categories()==0,"village exclusion across chunks");
        g=new Grid();for(int x=2;x<7;++x)g.put(x,20,4,19);check((run(g,c).categories()&4)!=0,"underground flowers");g.biome="minecraft:lush_caves";check(run(g,c).categories()==0,"natural biome excluded");
        g=new Grid();for(int x=4;x<8;++x)for(int y=4;y<8;++y)for(int z=4;z<8;++z)g.put(x,y,z,22);
        check((run(g,c).categories()&8)==0,"amethyst shell alone ignored");
        g.put(5,5,5,23);check((run(g,c).categories()&8)==0,"budding amethyst ignored");
        for(String id:List.of("small_amethyst_bud","medium_amethyst_bud","large_amethyst_bud")) {
            g.put(4,4,4,ChunkFinderDetector.classify("minecraft:"+id).kind());
            check((run(g,c).categories()&8)==0,"bud stage ignored: "+id);
        }
        g=new Grid();g.put(15,-5,4,ChunkFinderDetector.classify("minecraft:amethyst_cluster[facing=east,waterlogged=true]").kind());
        var cluster=run(g,c);check((cluster.categories()&8)!=0,"fully grown cluster detected at chunk edge");
        check(cluster.blocks().size()==1&&cluster.blocks().get(0).kind()==30&&cluster.blocks().get(0).x()==15,"only actual cluster highlighted");
        c[7]=1;check((run(g,c).categories()&8)!=0,"legacy sensitivity does not suppress cluster");
        c[0]&=~(1<<20);check((run(g,c).categories()&8)==0,"cluster toggle disables detection");
        g=new Grid();c=defaults();c[5]=6;check((ChunkFinderDetector.analyze(g,0,0,-16,48,false,c,new int[]{0,20,0,0},Set.of()).categories()&2)!=0,"mob cluster");
        c[0]=0;check(run(g,c).categories()==0,"all detectors disabled");
        check(ChunkFinderBridge.key(-1,-2)!=ChunkFinderBridge.key(-2,-1),"negative chunk identity");
        System.out.println("Chunkfinder detector regressions passed");
    }
}
