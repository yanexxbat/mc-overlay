package mcov;
import java.lang.invoke.MethodType;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicReferenceArray;

/** Client-tick/worker integration with mutable fixtures, without a Minecraft process. */
public final class ChunkFinderBridgeCheck {
    public static final class Palette {
        Object[] cells=new Object[4096];
        Palette(String value){Arrays.fill(cells,value);}
        public Object get(int x,int y,int z){return cells[y*256+z*16+x];}
        public Palette copy(){Palette p=new Palette("");p.cells=cells.clone();return p;}
    }
    public static final class Section {
        Palette blocks=new Palette("Block{minecraft:stone}"),biomes=new Palette("minecraft:plains");
        public Palette states(){return blocks;}public Palette biomes(){return biomes;}
    }
    public static final class Pos {public int x,z;public int getX(){return x;}public int getY(){return 0;}public int getZ(){return z;}}
    public static final class Chunk {
        Section[] sections={new Section()};Pos pos=new Pos();
        public Section[] sections(){return sections;}public Pos position(){return pos;}public int min(){return 0;}
        public Map<Pos,Hive> blockEntities(){return Map.of();}
    }
    public static final class Hive {public int bees(){return 1;}}
    public static final class Storage {public AtomicReferenceArray<Chunk> chunks=new AtomicReferenceArray<>(1);}
    public static final class Source {public Storage storage=new Storage();}
    public static final class World {
        Source source=new Source();public Source source(){return source;}public Iterable<Entity> entities(){return List.of();}
        public String dimension(){return "minecraft:overworld";}
    }
    public static class Entity {public double x(){return 4;}public double y(){return 9;}public double z(){return 4;}}
    public static final class Mob extends Entity {}public static final class Item extends Entity {}public static final class Bear extends Entity {}public static final class Bee extends Entity {}
    public static final class Player extends Entity {int messages;public void chat(String s){++messages;}}
    public static final class Client {public World level=new World();public Player player=new Player();}
    public static String literal(String s){return s;}
    static void m(List<String[]> t,String key,Class<?> owner,String name,Class<?>... args)throws Exception{Method m=owner.getDeclaredMethod(name,args);t.add(new String[]{key,owner.getName(),name,MethodType.methodType(m.getReturnType(),args).toMethodDescriptorString()});}
    static void f(List<String[]> t,String key,Class<?> owner,String name)throws Exception{t.add(new String[]{key,owner.getName(),name,owner.getDeclaredField(name).getType().descriptorString()});}
    static void check(boolean b,String s){if(!b)throw new AssertionError(s);}
    static void waitScan(Client client,int clear)throws Exception{
        long until=System.nanoTime()+5_000_000_000L;
        do{ChunkFinderBridge.update(true,ChunkFinderCheck.defaults(),2,3,6,true,clear,500);ChunkFinderBridge.tick(client);
            if(ChunkFinderBridge.view.scanned>0)return;if(ChunkFinderBridge.view.status==4)throw new AssertionError("Bridge failed");Thread.sleep(10);
        }while(System.nanoTime()<until);throw new AssertionError("Worker did not complete");
    }
    public static void main(String[] args)throws Exception{
        List<String[]> t=new ArrayList<>();f(t,"level",Client.class,"level");f(t,"player",Client.class,"player");m(t,"source",World.class,"source");f(t,"storage",Source.class,"storage");f(t,"chunks",Storage.class,"chunks");
        m(t,"sections",Chunk.class,"sections");m(t,"position",Chunk.class,"position");f(t,"cx",Pos.class,"x");f(t,"cz",Pos.class,"z");m(t,"states",Section.class,"states");m(t,"biomes",Section.class,"biomes");
        m(t,"copy",Palette.class,"copy");m(t,"cell",Palette.class,"get",int.class,int.class,int.class);m(t,"entities",World.class,"entities");m(t,"dimension",World.class,"dimension");m(t,"minY",Chunk.class,"min");m(t,"blockEntities",Chunk.class,"blockEntities");
        for(String axis:new String[]{"x","y","z"}){m(t,axis,Entity.class,axis);m(t,"pos"+axis.toUpperCase(),Pos.class,"get"+axis.toUpperCase());}
        m(t,"bees",Hive.class,"bees");m(t,"chat",Player.class,"chat",String.class);m(t,"literal",ChunkFinderBridgeCheck.class,"literal",String.class);
        for(Class<?> c:new Class<?>[]{Mob.class,Item.class,Bear.class,Bee.class,Hive.class})t.add(new String[]{"class."+c.getSimpleName(),c.getName(),"",""});
        Field api=ChunkFinderBridge.class.getDeclaredField("api");api.setAccessible(true);api.set(null,new ChunkFinderBridge.Api(ChunkFinderBridgeCheck.class.getClassLoader(),t.toArray(String[][]::new)));
        Client c=new Client();Chunk chunk=new Chunk();c.level.source.storage.chunks.set(0,chunk);chunk.sections[0].blocks.cells[9*256+4*16+4]="Block{minecraft:deepslate}[axis=y]";
        ChunkFinderBridge.update(true,ChunkFinderCheck.defaults(),2,3,6,true,0,500);ChunkFinderBridge.tick(c);
        // Mutate live data immediately: the queued worker must still see its owned copy.
        Arrays.fill(chunk.sections[0].blocks.cells,"Block{minecraft:stone}");waitScan(c,0);
        check(ChunkFinderBridge.view.rows.length==8&&ChunkFinderBridge.view.rows[2]==1,"Worker used mutable live palette");
        check(ChunkFinderBridge.view.blocks.length==4&&ChunkFinderBridge.view.blocks[3]==(4<<8),"Snapshot material missing from evidence wire format");
        check(c.player.messages==1&&ChunkFinderBridge.view.alerts[0]>0,"Local chat/alert missing");
        check(ChunkFinderBridge.view.scannedChunks.length==1,"Region coverage missing");
        long world=ChunkFinderBridge.view.world;
        ChunkFinderBridge.update(true,ChunkFinderCheck.defaults(),2,3,6,true,1,500);ChunkFinderBridge.tick(c);waitScan(c,1);
        check(ChunkFinderBridge.view.world>world&&ChunkFinderBridge.view.rows.length==0,"Clear retained previous analysis");
        check(c.player.messages==1,"Rescan duplicated local chat");
        ChunkFinderBridge.disable();ChunkFinderBridge.tick(c);check(ChunkFinderBridge.view.status==1&&!FrameSignal.chunkFinderEnabled,"Disable did not release workers");
        System.out.println("Chunkfinder snapshot ownership, worker lifecycle, alerts and coverage passed");
    }
}
