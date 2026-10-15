package mcov;
import java.util.*;
public final class AntiVanishCheck {
 static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
 static UUID id(int n){return new UUID(1,n);}
 static AntiVanishTracker.Player p(int n,boolean listed,boolean online,boolean nearby,boolean invisible){return new AntiVanishTracker.Player(id(n),"Player"+n,listed,online,nearby,invisible);}
 static List<AntiVanishTracker.Detection> sample(AntiVanishTracker t,long now,AntiVanishTracker.Player... rows){return t.sample(List.of(rows),id(0),now,false);}
 public static final class Entity {
  final com.mojang.authlib.GameProfile profile;boolean invisible;
  Entity(int n){profile=new com.mojang.authlib.GameProfile(id(n),"Player"+n);}
  public UUID uuid(){return profile.id();}public com.mojang.authlib.GameProfile profile(){return profile;}
  public boolean invisible(){return invisible;}public double distance(Entity other){return 4;}
 }
 public static final class Info {final Entity entity;Info(Entity e){entity=e;}public com.mojang.authlib.GameProfile profile(){return entity.profile;}}
 public static final class World {final List<Entity> players=new ArrayList<>();public List<Entity> players(){return players;}}
 public static final class Connection {
  final Collection<Info> online=new ArrayList<>(),listed=new ArrayList<>();boolean fail;
  public Collection<Info> online(){if(fail)throw new IllegalStateException("fixture failure");return online;}
  public Collection<Info> listed(){return listed;}
 }
 public static final class Client {public World level=new World();public Entity player=new Entity(0);final Connection connection=new Connection();public Connection connection(){return connection;}}
 static String[] field(String alias,Class<?> owner,String name,Class<?> type){return new String[]{alias,owner.getName(),name,type.descriptorString()};}
 static String[] method(String alias,Class<?> owner,String name,Class<?> out,Class<?>... args){return new String[]{alias,owner.getName(),name,java.lang.invoke.MethodType.methodType(out,args).toMethodDescriptorString()};}
 static void next()throws Exception{var f=AntiVanishBridge.class.getDeclaredField("next");f.setAccessible(true);f.setLong(null,0);}
 static void bridgeChecks()throws Exception {
  Class<?> gp=com.mojang.authlib.GameProfile.class;
  String[][] table={field("level",Client.class,"level",World.class),field("player",Client.class,"player",Entity.class),method("connection",Client.class,"connection",Connection.class),
   method("online",Connection.class,"online",Collection.class),method("listed",Connection.class,"listed",Collection.class),method("profile",Info.class,"profile",gp),
   method("players",World.class,"players",List.class),method("playerProfile",Entity.class,"profile",gp),method("uuid",Entity.class,"uuid",UUID.class),
   method("invisible",Entity.class,"invisible",boolean.class),method("distance",Entity.class,"distance",double.class,Entity.class)};
  var api=new AntiVanishBridge.Api(AntiVanishCheck.class.getClassLoader(),table);var f=AntiVanishBridge.class.getDeclaredField("api");f.setAccessible(true);f.set(null,api);
  var client=new Client();var e=new Entity(1);client.level.players.add(e);var info=new Info(e);client.connection.online.add(info);client.connection.listed.add(info);
  AntiVanishBridge.update(true,false,64,500);check(FrameSignal.antiVanishEnabled,"Valid bridge did not enable client ticks");AntiVanishBridge.tick(client);
  check(AntiVanishBridge.view.status==3&&AntiVanishBridge.view.names.length==0,"Client roster sampling failed");
  for(int n=2;n<900;++n)client.connection.online.add(new Info(new Entity(n)));next();AntiVanishBridge.tick(client);check(AntiVanishBridge.view.limited,"Oversized roster was not capped/reported");
  client.connection.fail=true;next();AntiVanishBridge.tick(client);check(AntiVanishBridge.view.status==4&&!FrameSignal.antiVanishEnabled,"Sampling exception escaped containment");
  AntiVanishBridge.update(true,false,64,500);check(!FrameSignal.antiVanishEnabled&&AntiVanishBridge.view.status==4,"Failed sampler retried without a toggle");
  AntiVanishBridge.disable();client.connection.fail=false;AntiVanishBridge.update(true,false,64,500);next();AntiVanishBridge.tick(client);check(AntiVanishBridge.view.status==3,"Off/on did not recover sampler");
  client.level=null;next();AntiVanishBridge.tick(client);check(AntiVanishBridge.view.status==2&&AntiVanishBridge.view.names.length==0,"Disconnect retained old results");
  AntiVanishBridge.update(true,false,64,0);AntiVanishBridge.tick(client);check(!FrameSignal.antiVanishEnabled,"Expired lease retained sampling");
 }
 public static void main(String[] args)throws Exception{
  var t=new AntiVanishTracker();
  check(sample(t,0,p(1,false,true,false,false)).isEmpty(),"Untrusted/unlisted NPC produced a warning");
  sample(t,100,p(1,true,true,true,false));sample(t,1700,p(1,true,true,true,false));
  check(sample(t,1800,p(1,false,true,true,false)).isEmpty(),"No confirmation grace for tab hiding");
  var hidden=sample(t,2900,p(1,false,true,true,false));check(hidden.size()==1&&hidden.get(0).evidence()==3,"Hidden TAB entry/entity was not detected");
  check(sample(t,3000,p(1,true,true,true,false)).isEmpty(),"Relisted player stayed detected");
  check(sample(t,4000).isEmpty(),"Normal disconnect was detected as vanish");
  check(sample(t,5000,p(1,true,true,true,true)).isEmpty(),"Invisibility opt-in was ignored");
  t.sample(List.of(p(1,true,true,true,true)),id(0),6000,true);
  check(t.sample(List.of(p(1,true,true,true,true)),id(0),7100,true).get(0).evidence()==4,"Opt-in invisibility evidence missing");
  check(t.sample(List.of(p(0,true,true,true,true)),id(0),9000,true).isEmpty(),"Local player was reported");
  var flood=new ArrayList<AntiVanishTracker.Player>();for(int i=2;i<5000;++i)flood.add(p(i,true,true,true,false));
  t.sample(flood,id(0),10_000,false);check(t.size()<=512,"Player cache is unbounded");
  t.sample(List.of(),id(0),100_000,false);check(t.size()==0,"Expired player cache was retained");
  t.clear();sample(t,0,p(2,true,true,false,false));sample(t,2000,p(2,true,true,false,false));sample(t,2100,p(2,false,true,false,false));
  check(sample(t,3200,p(2,false,true,false,false)).get(0).evidence()==1,"Server-wide TAB hint missing");
  t.clear();check(sample(t,4000,p(2,false,true,true,false)).isEmpty(),"World reset retained trust");
  check(AntiVanishProfiles.get("intermediary-1.21.11")!=null&&AntiVanishProfiles.get("mojmap-1.20.1")==null,"Version guard incorrect");
  AntiVanishBridge.update(true,true,Float.NaN,500);check(!FrameSignal.antiVanishEnabled,"Invalid or unconfigured bridge enabled ticks");
  AntiVanishBridge.disable();AntiVanishBridge.tick(new Object());check(AntiVanishBridge.view.names.length==0&&!FrameSignal.antiVanishEnabled,"Disable did not clear runtime");
  bridgeChecks();
  System.out.println("PASS Anti Vanish: trusted TAB transitions, debounce, relisting, disconnect rejection, opt-in invisibility, self/world guards and bounded/expired caches");
 }
}
