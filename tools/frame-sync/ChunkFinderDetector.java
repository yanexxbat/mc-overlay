package mcov;

import java.util.*;

/** Pure analysis of owned chunk snapshots. Never reads a live world. */
final class ChunkFinderDetector {
    static final int PLACED=0,EXPOSED=1,BASE=2,COBBLE=3,MASONRY=4,OBSIDIAN=5,PISTONS=6,STAIRS=7,SHAFTS=8,ROOMS=9,ORE=10,STORAGE=11,ITEMS=12,MOBS=13,FARM=14,REPEATERS=15,BEES=16,FLOORS=17,PLANTS=18,WORKSHOPS=19,GEODES=20;
    static final int UNKNOWN=0,AIR=1,FLUID=2,SOLID=3,SLATE=4,ROTATED=5,COBBLED=6,END=7,COBBLE_BLOCK=8,MASONRY_BLOCK=9,OBSIDIAN_BLOCK=10,PISTON=11,STAIR=12,ORE_BLOCK=13,CONTAINER=14,REPEATER=15,POWERED=16,HIVE=17,NEST=18,FLOWER=19,VINE=20,WORKSHOP=21,AMETHYST=22,BUDDING=23,SPAWNER=24,SCULK=25,VILLAGE=26,RAIL=27,WEB=28,PORTAL=29,AMETHYST_CLUSTER=30;
    record Cell(int kind,boolean open) {}
    static Cell classify(String state) {
        int start=state.indexOf("minecraft:");
        if(start<0) return new Cell(SOLID,false);
        start+=10; int end=start;
        while(end<state.length()&&(Character.isLetterOrDigit(state.charAt(end))||state.charAt(end)=='_')) ++end;
        String id=state.substring(start,end);
        boolean water=state.contains("waterlogged=true");
        int kind=switch(id) {
            case "air","cave_air","void_air" -> AIR;
            case "water","lava","bubble_column" -> FLUID;
            case "deepslate" -> state.contains("axis=x")||state.contains("axis=z")?ROTATED:SLATE;
            case "cobbled_deepslate" -> COBBLED;
            case "end_stone" -> END;
            case "cobblestone" -> COBBLE_BLOCK;
            case "polished_deepslate","deepslate_bricks","cracked_deepslate_bricks","deepslate_tiles","cracked_deepslate_tiles","chiseled_deepslate" -> MASONRY_BLOCK;
            case "obsidian" -> OBSIDIAN_BLOCK;
            case "piston","sticky_piston","piston_head","moving_piston" -> PISTON;
            case "chest","trapped_chest","barrel","hopper" -> CONTAINER;
            case "repeater" -> state.contains("powered=true")?POWERED:REPEATER;
            case "beehive" -> HIVE;
            case "bee_nest" -> NEST;
            case "vine" -> VINE;
            case "furnace","blast_furnace","smoker","anvil","chipped_anvil","damaged_anvil","enchanting_table","crafting_table" -> WORKSHOP;
            case "amethyst_block" -> AMETHYST;
            case "budding_amethyst" -> BUDDING;
            case "amethyst_cluster" -> AMETHYST_CLUSTER;
            case "spawner","trial_spawner" -> SPAWNER;
            case "sculk","sculk_sensor","sculk_shrieker","reinforced_deepslate" -> SCULK;
            case "bell","dirt_path" -> VILLAGE;
            case "rail","powered_rail","detector_rail","activator_rail" -> RAIL;
            case "cobweb" -> WEB;
            case "crying_obsidian","nether_portal" -> PORTAL;
            case "dandelion","poppy","blue_orchid","allium","azure_bluet","red_tulip","orange_tulip","white_tulip","pink_tulip","oxeye_daisy","cornflower","lily_of_the_valley","wither_rose","torchflower","pink_petals","sunflower","lilac","rose_bush","peony","closed_eyeblossom","open_eyeblossom" -> FLOWER;
            default -> id.endsWith("_stairs")?STAIR:id.endsWith("_ore")?ORE_BLOCK:id.endsWith("shulker_box")?CONTAINER:SOLID;
        };
        return new Cell(kind,water||kind==AIR||kind==FLUID);
    }
    interface Grid {
        Cell at(int x,int y,int z);
        String biome(int x,int y,int z);
    }
    record Point(int x,int y,int z,int signal,int kind) {
        Point(int x,int y,int z,int signal) { this(x,y,z,signal,UNKNOWN); }
    }
    record Result(int x,int z,int categories,int score,int chance,int minY,int maxY,int evidence,List<Point> blocks) {}
    static boolean on(int[] settings,int flag) { return (settings[0]&(1<<flag))!=0; }
    static final int[][] SIDES={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static boolean solid(Cell c) { return c.kind!=UNKNOWN&&!c.open&&c.kind!=FLOWER&&c.kind!=VINE; }
    static boolean buried(Grid g,int x,int y,int z) {
        for(int[] d:SIDES) { Cell c=g.at(x+d[0],y+d[1],z+d[2]); if(c.kind==UNKNOWN||c.open) return false; }
        return true;
    }
    static boolean walls(Grid g,int x,int y,int z) { return solid(g.at(x-1,y,z))&&solid(g.at(x+1,y,z))&&solid(g.at(x,y,z-1))&&solid(g.at(x,y,z+1)); }
    static boolean near(List<Point> points,int x,int y,int z,int radius) {
        for(Point p:points) if(Math.abs(p.x-x)<=radius&&Math.abs(p.y-y)<=radius&&Math.abs(p.z-z)<=radius) return true;
        return false;
    }
    static void add(List<Point> out,int x,int y,int z,int signal) {
        // Per-signal quotas keep common evidence from hiding rarer detectors.
        int n=0; for(Point p:out) if(p.signal==signal) ++n;
        if(n<64) out.add(new Point(x,y,z,signal));
    }
    static Result analyze(Grid g,int cx,int cz,int minY,int maxY,boolean end,int[] settings,int[] entities,Set<Long> occupiedHives) {
        int x0=cx*16,z0=cz*16;
        List<Point> spawners=new ArrayList<>(),cities=new ArrayList<>(),villages=new ArrayList<>(),mines=new ArrayList<>(),portals=new ArrayList<>();
        // Nearby loaded context, including neighbouring chunks, suppresses structure clues.
        for(int y=minY;y<maxY;++y) for(int z=z0-16;z<z0+32;++z) for(int x=x0-16;x<x0+32;++x) {
            if(Thread.currentThread().isInterrupted()) throw new java.util.concurrent.CancellationException();
            int k=g.at(x,y,z).kind;
            if(k==SPAWNER&&spawners.size()<4096) spawners.add(new Point(x,y,z,0));
            else if(k==SCULK&&cities.size()<4096) cities.add(new Point(x,y,z,0));
            else if(k==VILLAGE&&villages.size()<4096) villages.add(new Point(x,y,z,0));
            else if((k==RAIL||k==WEB)&&mines.size()<4096) mines.add(new Point(x,y,z,0));
            else if(k==PORTAL&&portals.size()<256) portals.add(new Point(x,y,z,0));
        }
        int slate=0,rotated=0,cobbled=0,endstone=0; int[] counts=new int[21];
        Set<Integer> floorColumns=new HashSet<>(); List<Point> evidence=new ArrayList<>();
        boolean geode=false;
        for(int y=minY;y<maxY;++y) for(int z=z0;z<z0+16;++z) for(int x=x0;x<x0+16;++x) {
            Cell c=g.at(x,y,z); int k=c.kind;
            if(k==UNKNOWN) continue;
            boolean placed=on(settings,PLACED)&&(!on(settings,EXPOSED)||buried(g,x,y,z));
            if(placed) {
                if((k==SLATE||k==ROTATED)&&y>8) {++slate;add(evidence,x,y,z,PLACED);}
                if(k==ROTATED&&settings[2]>0) {++rotated;add(evidence,x,y,z,PLACED);}
                if(k==COBBLED&&y>0) {++cobbled;add(evidence,x,y,z,PLACED);}
                if(k==END&&!end) {++endstone;add(evidence,x,y,z,PLACED);}
            }
            int signal=-1;
            if(on(settings,BASE)) {
                if(k==COBBLE_BLOCK&&y<0&&!near(spawners,x,y,z,10)) {
                    boolean line=true,lineZ=true;
                    for(int i=1;i<4;++i) {line&=g.at(x+i,y,z).kind==COBBLE_BLOCK;lineZ&=g.at(x,y,z+i).kind==COBBLE_BLOCK;}
                    if(line||lineZ) signal=COBBLE;
                } else if(k==MASONRY_BLOCK&&y<0&&!near(cities,x,y,z,24)) signal=MASONRY;
                else if(k==OBSIDIAN_BLOCK&&y<20&&!near(portals,x,y,z,12)&&buried(g,x,y,z)) signal=OBSIDIAN;
                else if(k==PISTON&&y<40) signal=PISTONS;
                else if(k==STAIR&&y<0&&!near(cities,x,y,z,24)&&!near(mines,x,y,z,12)) signal=STAIRS;
                else if(k==CONTAINER&&!near(spawners,x,y,z,10)&&!near(mines,x,y,z,12)&&!near(villages,x,y,z,24)&&!near(cities,x,y,z,24)) {signal=STORAGE;counts[STORAGE]+=y<0?2:0;}
                else if(k==ORE_BLOCK&&on(settings,ORE)) {
                    boolean column=true;for(int i=1;i<4;++i) column&=g.at(x,y+i,z).kind==ORE_BLOCK;
                    if(column) signal=ORE;
                }
                if(k==AIR&&y<0&&on(settings,SHAFTS)&&g.at(x,y-1,z).kind!=AIR) {
                    int depth=0;while(y+depth<0&&g.at(x,y+depth,z).kind==AIR&&walls(g,x,y+depth,z)) ++depth;
                    if(depth>=8) {++counts[SHAFTS];add(evidence,x,y,z,SHAFTS);}
                }
                if(k==AIR&&y<0&&on(settings,ROOMS)&&solid(g.at(x,y-1,z))) {
                    boolean room=true;
                    for(int dz=0;dz<3&&room;++dz) for(int dx=0;dx<3;++dx) {
                        if(!solid(g.at(x+dx,y-1,z+dz))) {room=false;break;}
                        int h=0;while(h<=4&&g.at(x+dx,y+h,z+dz).kind==AIR) ++h;
                        if(h<2||h>4||!solid(g.at(x+dx,y+h,z+dz))) {room=false;break;}
                    }
                    if(room) {++counts[ROOMS];add(evidence,x,y,z,ROOMS);}
                }
            }
            if(signal>=0&&on(settings,signal)) {++counts[signal];add(evidence,x,y,z,signal);}
            signal=-1;
            if(on(settings,FARM)) {
                if(k==REPEATER||k==POWERED) {signal=REPEATERS;if(k==POWERED) counts[REPEATERS]+=2;}
                else if((k==HIVE||k==NEST)&&(occupiedHives.contains(pack(x,y,z))||entities[3]>0)) {signal=BEES;counts[BEES]+=k==HIVE?3:0;}
                else if(k==COBBLED&&y>=0&&y<=20) {floorColumns.add((z-z0)*16+x-x0);signal=FLOORS;}
                else if((k==FLOWER&&y<40)||(k==VINE&&y<50)) {
                    String biome=g.biome(x,y,z);
                    boolean natural=biome.isEmpty()||biome.contains("lush_caves")||biome.contains("swamp")||biome.contains("jungle")||biome.contains("mangrove");
                    // Sky-connected terrain may dip below the thresholds.
                    boolean roof=false;for(int h=1;h<=16&&y+h<maxY;++h) if(solid(g.at(x,y+h,z))) {roof=true;break;}
                    if(!natural&&roof) signal=PLANTS;
                } else if(k==WORKSHOP&&!near(villages,x,y,z,24)&&!near(cities,x,y,z,24)) signal=WORKSHOPS;
            }
            if(signal>=0&&on(settings,signal)) {++counts[signal];add(evidence,x,y,z,signal);}
            if(k==AMETHYST_CLUSTER&&on(settings,GEODES)) {
                geode=true;add(evidence,x,y,z,GEODES);
            }
        }
        int score=0,bits=0;int[] weights=new int[21];
        weights[COBBLE]=counts[COBBLE]>0?3:0;weights[MASONRY]=Math.min(5,counts[MASONRY]);
        weights[OBSIDIAN]=Math.min(2,counts[OBSIDIAN]);weights[PISTONS]=Math.min(8,counts[PISTONS]*4);
        weights[STAIRS]=Math.min(3,counts[STAIRS]);weights[SHAFTS]=counts[SHAFTS]>0?6:0;
        weights[ROOMS]=counts[ROOMS]>=4?3:0;weights[ORE]=counts[ORE]>0?1:0;
        weights[STORAGE]=Math.min(12,counts[STORAGE]);weights[ITEMS]=entities[0]>0?2:0;
        weights[MOBS]=entities[1]>=20?6:0;if(entities[2]>0)weights[MOBS]+=4;
        if(on(settings,BASE)) for(int i=COBBLE;i<=MOBS;++i) if(on(settings,i)&&weights[i]>0) {score+=weights[i];bits|=1<<i;}
        double none=1; int[] chance=new int[21];
        chance[REPEATERS]=counts[REPEATERS]>=3?75:counts[REPEATERS]>0?45:0;
        chance[BEES]=Math.min(85,counts[BEES]*15);chance[FLOORS]=floorColumns.size()>=12?65:floorColumns.size()>=4?30:0;
        chance[PLANTS]=Math.min(65,counts[PLANTS]*15);chance[WORKSHOPS]=Math.min(80,counts[WORKSHOPS]*25);
        if(on(settings,FARM)) for(int i=REPEATERS;i<=WORKSHOPS;++i) if(on(settings,i)&&chance[i]>0) {none*=1-chance[i]/100.;bits|=1<<i;}
        int probability=Math.min(99,(int)Math.round(100*(1-none))),categories=0;
        if(on(settings,PLACED)) {
            // Extra evidence bits identify the exact placed rule; settings[2]==0 disables rotation.
            if(slate>=settings[1]) bits|=1<<21;
            if(settings[2]>0&&rotated>=settings[2]) bits|=1<<22;
            if(cobbled>=settings[3]) bits|=1<<23;
            if(endstone>=settings[4]) bits|=1<<24;
            if((bits&(15<<21))!=0) {categories|=1;bits|=1<<PLACED;}
        }
        if(on(settings,BASE)&&score>=settings[5]) categories|=2;
        if(on(settings,FARM)&&probability>=settings[6]) categories|=4;
        if(geode) {categories|=8;bits|=1<<GEODES;}
        final int selected=categories,signals=bits;
        evidence.removeIf(p->(signals&(1<<p.signal))==0||(p.signal==PLACED?(selected&1)==0:p.signal==GEODES?(selected&8)==0:p.signal>=REPEATERS?(selected&4)==0:(selected&2)==0));
        // Preserve material from this owned snapshot, not from a later live-world read.
        evidence.replaceAll(p->new Point(p.x,p.y,p.z,p.signal,g.at(p.x,p.y,p.z).kind));
        return new Result(cx,cz,categories,score,probability,minY,maxY,bits,List.copyOf(evidence));
    }
    static long pack(int x,int y,int z) {return ((long)x&0x3ffffffL)<<38|((long)z&0x3ffffffL)<<12|((long)y&4095);}
}
