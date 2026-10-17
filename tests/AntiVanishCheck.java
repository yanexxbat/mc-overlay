package mcov;
import java.util.*;
public final class AntiVanishCheck {
 static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
 static UUID id(int n){return new UUID(1,n);}
 static String staff(int n){return switch(n){case 0->"LocalPlayer";case 1->"FluffyMaster07";case 2->"archivePedro";case 3->"Munkerlich";case 4->"Frenk_Btw";default->"Player"+n;};}
 static boolean has(List<AntiVanishTracker.Detection> rows,int flag){return rows.stream().anyMatch(r->(r.evidence()&flag)!=0);}
 static AntiVanishTracker trusted(boolean nearby){var t=new AntiVanishTracker();sample(t,0,p(1,true,true,nearby,false));sample(t,1600,p(1,true,true,nearby,false));return t;}
 static AntiVanishTracker.Player p(int n,boolean listed,boolean online,boolean nearby,boolean invisible){return new AntiVanishTracker.Player(id(n),staff(n),listed,online,nearby,invisible);}
 static List<AntiVanishTracker.Detection> sample(AntiVanishTracker t,long now,AntiVanishTracker.Player... rows){return t.sample(List.of(rows),id(0),now,false);}
 public static final class Entity {
  final com.mojang.authlib.GameProfile profile;boolean invisible;
  Entity(int n){profile=new com.mojang.authlib.GameProfile(id(n),staff(n));}
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
 static void next()throws Exception{for(String key:List.of("next","nextRoster")){var f=AntiVanishBridge.class.getDeclaredField(key);f.setAccessible(true);f.setLong(null,0);}}
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
 static void trackerChecks(){
  var t=new AntiVanishTracker();
  check(sample(t,0,p(1,false,true,false,false)).isEmpty(),"Untrusted entry produced warning");
  sample(t,100,p(1,true,true,true,false));t.tabHidden(List.of(id(1)),1000);
  check(t.rows(1000).isEmpty(),"Trust grace ignored");
  sample(t,1700,p(1,true,true,true,false));
  check(sample(t,1800,p(1,false,true,true,false)).isEmpty(),"No confirmation grace");
  var hidden=sample(t,2900,p(1,false,true,true,false));
  check(has(hidden,AntiVanishTracker.HIDDEN)&&has(hidden,AntiVanishTracker.CRITICAL),"Nearby hidden player missing");
  var visible=sample(t,3000,p(1,true,true,true,false));
  check(!has(visible,AntiVanishTracker.HIDDEN)&&has(visible,AntiVanishTracker.VISIBLE),"Relisted player not cleared");
  check(sample(t,7100).isEmpty(),"Return notice did not expire");
  t=trusted(false);t.tabHidden(List.of(id(1)),1700);check(has(t.rows(1700),1),"Listed=false event missing");
  check(!has(t.rows(1700),128),"Distant staff generated nearby warning");check(t.rows(21700).isEmpty(),"Detection TTL wrong");
  t=trusted(false);t.removed(List.of(id(1)),1700);sample(t,1800);check(t.rows(3000).isEmpty(),"Ordinary leave treated as vanish");
  t=trusted(false);t.message("", "UnrelatedPlayer",1650);t.removed(List.of(id(1)),1700);sample(t,1800);
  check(t.rows(2699).isEmpty()&&has(t.rows(2700),1),"Silent removal delay/leave-message heuristic failed");
  for(boolean before:List.of(false,true)){
   t=trusted(false);if(before)t.message("FluffyMaster07 left the game",null,1650);
   t.removed(List.of(id(1)),1700);sample(t,1800);if(!before)t.message("FluffyMaster07 left the game",null,2100);
   check(t.rows(3000).isEmpty(),"Normal leave cancellation failed: "+before);
  }
  check(AntiVanishTracker.looksLikeLeave("[Server] FluffyMaster07 has left the game","FluffyMaster07"),"English leave parsing");
  check(AntiVanishTracker.looksLikeLeave("FluffyMaster07 hat den Server verlassen","FluffyMaster07"),"German leave parsing");
  check(!AntiVanishTracker.looksLikeLeave("NotFluffyMaster07 left the game","FluffyMaster07"),"Name substring matched");
  t=new AntiVanishTracker();for(long now:new long[]{0,1600})sample(t,now,p(1,true,true,false,false),p(2,true,true,false,false),p(3,true,true,false,false),p(4,true,true,false,false));
  t.tabHidden(List.of(id(1),id(2),id(3),id(4)),1700);
  sample(t,1800,p(1,false,true,false,false),p(2,false,true,false,false),p(3,false,true,false,false),p(4,false,true,false,false));
  check(sample(t,5000,p(1,false,true,false,false),p(2,false,true,false,false),p(3,false,true,false,false),p(4,false,true,false,false)).isEmpty(),"Bulk TAB hiding not suppressed");
  t=trusted(true);t.tabHidden(List.of(id(1),id(50),id(51),id(52)),1700);sample(t,1800,p(1,false,true,true,false));
  check(sample(t,5000,p(1,false,true,true,false)).isEmpty(),"Bulk packet containing only one staff bypassed roster filter");
  t=trusted(false);t.mode(id(1),true,1700);check(has(t.rows(1700),8),"Spectator event missing");
  t.mode(id(1),false,1800);check(!has(t.rows(1800),8)&&has(t.rows(1800),256),"Spectator return missing");
  t=new AntiVanishTracker();sample(t,0,p(9,true,true,true,false));sample(t,1700,p(9,true,true,true,false));t.tabHidden(List.of(id(9)),1800);
  t.completions(List.of("Player9"),1800);check(t.rows(1800).isEmpty(),"Non-staff was monitored");
  t=trusted(false);t.completions(List.of(staff(1)),1700);check(t.rows(1700).isEmpty(),"Online staff reported by completion");
  sample(t,1800);t.completions(List.of(staff(1)),1900);check(has(t.rows(1900),16),"Missing staff completion not detected");
  t.clear();t.completions(List.of(staff(1),staff(2),staff(3),staff(4)),2000);check(t.rows(2000).isEmpty(),"Bulk completion hints not suppressed");
  t.completions(List.of(staff(1)),2100);check(has(t.rows(2100),16),"Unknown UUID from fixed staff list not detected");
  check(!has(sample(t,2200,p(1,true,true,false,false)),16),"Synthetic completion identity not cleared on return");
  t.clear();t.sample(List.of(p(1,true,true,true,true)),id(1),0,true);t.completions(List.of(staff(1)),2000);check(t.rows(2000).isEmpty(),"Self reported");
  t.clear();t.local(id(1),staff(1));t.completions(List.of(staff(1)),2000);check(t.rows(2000).isEmpty(),"Self missing from capped roster reported");
  t=trusted(true);sample(t,1700,p(1,true,true,true,true));check(t.rows(1700).isEmpty(),"Invisibility toggle ignored");
  var invis=t.sample(List.of(p(1,true,true,true,true)),id(0),1800,true);check(has(invis,4)&&!has(invis,128),"Invisible entity alone should be a hint");
  t.sensor(null,"12m N",32,"sound",14,false,1900,3000);check(has(t.rows(1900),128),"Combined categories with named proof missing");
  sample(t,2000,p(1,true,true,true,false));t.sensor(null,"15m N",64,"particle",16,false,15000,3000);
  check(!has(t.rows(15000),128),"Cleared named proof generated another alert");
  t.clear();t.sensor(null,"12m N",32,"sound",14,false,0,3000);t.sensor(null,"12m N",64,"particle",16,false,1,3000);
  check(!has(t.rows(1),128),"Anonymous sensors alone identified staff");
  t.clear();check(sample(t,4000,p(1,false,true,true,false)).isEmpty(),"World reset retained trust");
  t.configure(false);var flood=new ArrayList<AntiVanishTracker.Player>();for(int i=5;i<5000;++i)flood.add(p(i,true,true,true,false));
  t.sample(flood,id(0),10000,false);check(t.size()<=512,"Unbounded player cache");t.sample(List.of(),id(0),100000,false);check(t.size()==0,"Expired cache retained");
 }
 public record Probe(int id,String text){}
 public record Suggestion(String text){}
 public record Suggestions(int id,List<Suggestion> entries){}
 public record Remove(List<UUID> ids){}
 public record Bundle(List<Object> packets){}
 public static final class Sender {public Object sent;public void send(Object packet){sent=packet;}}
 static String[] type(String alias,Class<?> type){return new String[]{"class."+alias,type.getName(),"",""};}
 static void packetChecks()throws Exception{
  String[][] table={type("Probe",Probe.class),type("Suggestions",Suggestions.class),type("Remove",Remove.class),type("Bundle",Bundle.class),
   method("send",Sender.class,"send",void.class,Object.class),method("suggestionId",Suggestions.class,"id",int.class),
   method("suggestions",Suggestions.class,"entries",List.class),method("suggestionText",Suggestion.class,"text",String.class),
   method("removed",Remove.class,"ids",List.class),method("bundle",Bundle.class,"packets",List.class)};
  var a=new AntiVanishBridge.Api(AntiVanishCheck.class.getClassLoader(),table);var sender=new Sender();a.probe(sender,1000);
  check(sender.sent instanceof Probe p&&p.text.equals("minecraft:msg "),"Probe sent something other than command suggestions");
  int probe=((Probe)sender.sent).id;var t=new AntiVanishTracker();
  a.observe(new Suggestions(probe-1,List.of(new Suggestion(staff(1)))),false,null,null,t,1100,false);
  check(t.rows(1100).isEmpty(),"Unrelated completion response consumed");
  a.observe(new Suggestions(probe,List.of(new Suggestion(staff(1)))),false,null,null,t,1100,false);check(has(t.rows(1100),16),"Own probe not consumed");
  t.clear();a.observe(new Suggestions(probe,List.of(new Suggestion(staff(1)))),false,null,null,t,1200,false);check(t.rows(1200).isEmpty(),"Replayed probe accepted");
  a.probe(sender,1300);probe=((Probe)sender.sent).id;a.observe(new Suggestions(probe,List.of(new Suggestion(staff(1)))),false,null,null,t,17000,false);check(t.rows(17000).isEmpty(),"Stale probe accepted");
  Object network=new Object();AntiVanishPackets.bind(network,System.nanoTime()+5_000_000_000L);
  var q=AntiVanishPackets.class.getDeclaredField("queue");q.setAccessible(true);var queue=(java.util.Queue<?>)q.get(null);
  AntiVanishPackets.observe(new Object(),new Remove(List.of(id(1))),false);AntiVanishPackets.observe(network,new Object(),false);
  check(queue.isEmpty(),"Unrelated connection/packet retained");
  t=trusted(false);t.message("", "OtherPlayer",1650);sample(t,1700);
  AntiVanishPackets.observe(network,new Bundle(List.of(new Remove(List.of(id(1))))),false);
  AntiVanishPackets.drain(a,null,null,t,1800,false);check(has(t.rows(2800),1),"Bundled packet not handled");
  for(int i=0;i<5000;++i)AntiVanishPackets.observe(network,new Remove(List.of()),false);
  check(queue.size()==4096&&AntiVanishPackets.overflowed(),"Packet queue not bounded");AntiVanishPackets.clear();check(queue.isEmpty(),"Reset retained packets");
  AntiVanishPackets.bind(network,System.nanoTime()-1);AntiVanishPackets.observe(network,new Remove(List.of()),false);check(queue.isEmpty(),"Expired lease captured packets");AntiVanishPackets.clear();
  check(AntiVanishBridge.Api.suspiciousSound("minecraft:block.chest.open")&&!AntiVanishBridge.Api.suspiciousSound("minecraft:ambient.cave"),"Sound filter");
  check(AntiVanishBridge.Api.suspiciousParticle("crit")&&!AntiVanishBridge.Api.suspiciousParticle("rain"),"Particle filter");
 }
 public static void main(String[] args)throws Exception{
  trackerChecks();packetChecks();
  check(AntiVanishProfiles.get("intermediary-1.21.11")!=null&&AntiVanishProfiles.get("mojmap-1.20.1")==null,"Version guard incorrect");
  AntiVanishBridge.update(true,true,Float.NaN,500);check(!FrameSignal.antiVanishEnabled,"Invalid bridge enabled ticks");
  AntiVanishBridge.disable();AntiVanishBridge.tick(new Object());check(AntiVanishBridge.view.names.length==0&&!FrameSignal.antiVanishEnabled,"Disable retained runtime");
  bridgeChecks();System.out.println("PASS Anti Vanish: staff-only evidence, departure/mass-event filters, spectator, completion correlation, combined hints, TTL, packet bounds, reset and bridge recovery");
 }
}
