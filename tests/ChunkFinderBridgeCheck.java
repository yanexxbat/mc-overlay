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
        public Section[] sections(){return sections;}public Pos position(){return pos;}public int bottom;public int min(){return bottom;}
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
    public static final class Client {public World level=new World();public Player player=new Player();public Connection connection(){return connection;}final Connection connection=new Connection();}
    public static final class Connection {final Object wire=new Object();public Object network(){return wire;}}
    public static String literal(String s){return s;}
    static void m(List<String[]> t,String key,Class<?> owner,String name,Class<?>... args)throws Exception{Method m=owner.getDeclaredMethod(name,args);t.add(new String[]{key,owner.getName(),name,MethodType.methodType(m.getReturnType(),args).toMethodDescriptorString()});}
    static void f(List<String[]> t,String key,Class<?> owner,String name)throws Exception{t.add(new String[]{key,owner.getName(),name,owner.getDeclaredField(name).getType().descriptorString()});}

    public static class LightPacket { final Data data;LightPacket(int value){data=new Data(value);}public int x(){return 2;}public int z(){return 0;}public Data data(){return data;} }
    public static final class ChunkPacket extends LightPacket {ChunkPacket(int value){super(value);}}
    public static final class Data {
        final byte[] data;Data(int value){data=ChunkActivityCheck.data(value);}
        public BitSet mask(){return BitSet.valueOf(new long[]{2});}public BitSet empty(){return new BitSet();}public List<byte[]> updates(){return List.of(data);}
    }
    public static final class BlockPacket {}
    public static final class SectionPacket {}
    public static final class RespawnPacket {}
    public static final class BundlePacket {}
    public static final class LightFixture {
        public static int BLOCK=0,SKY=1;
        public byte[] bytes=new byte[2048];
        public byte[] bytes(){return bytes;}
        public static Object pos(int x,int y,int z){return new Object();}
        public LightFixture engine(){return this;}
        public LightFixture layer(int layer){return layer==0?this:skyFixture;}
        public LightFixture data(Object pos){return this;}
        static final LightFixture skyFixture=new LightFixture();
    }
    public static final class LightSource {
        public LightFixture engine(){return fixture;}
        static LightFixture fixture=new LightFixture();
    }
    public static final class LightWorld {public LightSource source(){return new LightSource();}}
    static void lightScanTest()throws Exception{
        List<String[]> t=new ArrayList<>();
        m(t,"source",LightWorld.class,"source");m(t,"lightEngine",LightSource.class,"engine");
        m(t,"lightLayer",LightFixture.class,"layer",int.class);f(t,"blockLayer",LightFixture.class,"BLOCK");f(t,"skyLayer",LightFixture.class,"SKY");
        m(t,"sectionPos",LightFixture.class,"pos",int.class,int.class,int.class);
        m(t,"lightData",LightFixture.class,"data",Object.class);m(t,"lightBytes",LightFixture.class,"bytes");
        m(t,"minY",Chunk.class,"min");m(t,"cell",Palette.class,"get",int.class,int.class,int.class);
        var a=new ChunkFinderBridge.Api(ChunkFinderBridgeCheck.class.getClassLoader(),t.toArray(String[][]::new));
        var scan=new ChunkFinderBridge.LightScan();Chunk chunk=new Chunk();chunk.bottom=-16;
        LightSource.fixture.bytes[0]=12;LightFixture.skyFixture.bytes[0]=0;
        var loaded=Map.<Long,Object>of(0L,chunk);
        scan.tick(a,new LightWorld(),loaded,0,0,8,5,false,1);
        check(scan.visible.size()==1&&scan.visible.get(0).y()==-16,"existing light not found without packets");
        check((scan.visible.get(0).signal()>>8)==12,"light level not preserved");
        scan.clear();LightFixture.skyFixture.bytes[0]=12;
        scan.tick(a,new LightWorld(),loaded,0,0,8,5,false,1);check(scan.visible.isEmpty(),"sky light dominated candidate");
        scan.clear();LightFixture.skyFixture.bytes[0]=0;chunk.bottom=0;
        scan.tick(a,new LightWorld(),loaded,0,0,8,5,false,1);check(scan.visible.isEmpty(),"Y zero scanned");
        scan.clear();chunk.bottom=-16;LightSource.fixture.bytes[0]=6;
        scan.tick(a,new LightWorld(),loaded,0,0,8,5,true,1);check(scan.visible.isEmpty(),"isolated weak noise survived");
    }
    static void check(boolean b,String s){if(!b)throw new AssertionError(s);}
    static void waitScan(Client client,int clear)throws Exception{
        long until=System.nanoTime()+5_000_000_000L;
        do{ChunkFinderBridge.update(true,ChunkFinderCheck.defaults(),2,3,6,true,clear,500);ChunkFinderBridge.tick(client);
            if(ChunkFinderBridge.view.scanned>0)return;if(ChunkFinderBridge.view.status==4)throw new AssertionError("Bridge failed");Thread.sleep(10);
        }while(System.nanoTime()<until);throw new AssertionError("Worker did not complete");
    }
    public static void main(String[] args)throws Exception{
        lightScanTest();List<String[]> t=new ArrayList<>();f(t,"level",Client.class,"level");f(t,"player",Client.class,"player");m(t,"source",World.class,"source");f(t,"storage",Source.class,"storage");f(t,"chunks",Storage.class,"chunks");
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
        // Exercise the actual shared packet tap while Anti Vanish itself is disabled.
        m(t,"activityConnection",Client.class,"connection");m(t,"activityNetwork",Connection.class,"network");
        for(String tag:List.of("Light","Chunk")) {m(t,"activity"+tag+"X",LightPacket.class,"x");m(t,"activity"+tag+"Z",LightPacket.class,"z");m(t,"activity"+tag+"Data",LightPacket.class,"data");}
        for(String tag:List.of("Block","Sky")){m(t,"activity"+tag+"YMask",Data.class,"mask");m(t,"activityEmpty"+tag,Data.class,"empty");m(t,"activity"+tag+"Updates",Data.class,"updates");}
        Class<?>[] packetClasses={LightPacket.class,ChunkPacket.class,BlockPacket.class,SectionPacket.class,RespawnPacket.class,BundlePacket.class};
        String[] tags={"Light","Chunk","Block","Section","Respawn","Bundle"};
        for(int i=0;i<tags.length;++i)t.add(new String[]{"class.Activity"+tags[i],packetClasses[i].getName(),"",""});
        var activityApi=new ChunkFinderBridge.Api(ChunkFinderBridgeCheck.class.getClassLoader(),t.toArray(String[][]::new));api.set(null,activityApi);ChunkActivity.configure(activityApi);
        boolean hook=AntiVanishPackets.installed;AntiVanishPackets.installed=true;
        c=new Client();chunk=new Chunk();chunk.bottom=-16;chunk.pos.x=2;c.level.source.storage.chunks.set(0,chunk);
        int[] settings=Arrays.copyOf(ChunkFinderCheck.defaults(),11);settings[8]=1;settings[9]=1;settings[10]=1;
        ChunkFinderBridge.update(true,settings,2,3,6,true,0,500);ChunkFinderBridge.tick(c);
        AntiVanishPackets.observe(c.connection.wire,new ChunkPacket(7),false);ChunkFinderBridge.tick(c);
        check(ChunkFinderBridge.view.rows.length==0,"initial chunk packet produced activity");
        AntiVanishPackets.observe(c.connection.wire,new LightPacket(8),false);ChunkFinderBridge.tick(c);
        check(ChunkFinderBridge.view.rows.length==8&&(ChunkFinderBridge.view.rows[2]&16)!=0,"shared packet hook did not publish activity");
        check(ChunkFinderBridge.view.scanned==0,"activity-only mode scanned static block clues");
        check(c.player.messages==1,"activity alert not delivered");
        AntiVanishPackets.observe(c.connection.wire,new LightPacket(9),true);ChunkFinderBridge.tick(c);
        check(c.player.messages==1,"outbound packet became activity");
        ChunkFinderBridge.update(true,settings,2,3,6,true,2,500);ChunkFinderBridge.tick(c);
        AntiVanishPackets.observe(new Object(),new ChunkPacket(7),false);ChunkFinderBridge.tick(c);
        check(ChunkActivity.tracker.size()==0,"foreign connection became a baseline");
        AntiVanishPackets.observe(c.connection.wire,new ChunkPacket(7),false);ChunkFinderBridge.tick(c);
        for(int i=0;i<140;++i)AntiVanishPackets.observe(c.connection.wire,new LightPacket(8),false);
        ChunkFinderBridge.tick(c);check(ChunkActivity.tracker.size()==0&&ChunkFinderBridge.view.rows.length==0,"overflow compared an incomplete light history");
        AntiVanishPackets.observe(c.connection.wire,new LightPacket(9),false);ChunkFinderBridge.tick(c);
        check(ChunkFinderBridge.view.rows.length==0,"first packet after overflow became activity");
        ChunkFinderBridge.disable();ChunkFinderBridge.tick(c);check(ChunkActivity.tracker.size()==0,"disabled activity retained baselines");
        AntiVanishPackets.installed=hook;
        System.out.println("Chunkfinder snapshot ownership, worker lifecycle, alerts and coverage passed");
    }
}
