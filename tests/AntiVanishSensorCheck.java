package mcov;
import java.util.*;
import static mcov.AntiVanishCheck.*;

/** Exercises the actual reflection adapter with controlled causes in the world. */
public final class AntiVanishSensorCheck {
 public static class Entity {
  public Object inventoryMenu=new Object(),menu=inventoryMenu;boolean invisible;double x,y=64,z;
  public double x(){return x;}public double y(){return y;}public double z(){return z;}
  public boolean invisible(){return invisible;}
 }
 public static class Player extends Entity {}
 public static class Projectile extends Entity {}
 public static class Villager extends Entity {}
 public record Pos(int x,int y,int z){}
 public static class State {
  String key="block.minecraft.air";boolean powered;
  public State block(){return this;}public String key(){return key;}
  public boolean has(Object p){return true;}public Comparable<?> value(Object p){return powered;}
 }
 public static class World {
  List<Entity> entities=new ArrayList<>();State state=new State();boolean loaded=true;
  public Iterable<Entity> entities(){return entities;}public boolean loaded(Pos p){return loaded;}
  public State state(Pos p){return state;}public boolean powered(Pos p){return state.powered;}
 }
 public record Sound(String id) {
  public Sound holder(){return this;}public Sound value(){return this;}
  public double x(){return 10;}public double y(){return 64;}public double z(){return 0;}
 }
 public record Particle(String id) {
  public Particle particle(){return this;}public Particle type(){return this;}
  public double x(){return 10;}public double y(){return 64;}public double z(){return 0;}
 }
 public static class Registry {public String key(Object p){return ((Particle)p).id;}}
 public static Registry registry=new Registry();public static Object property=new Object();
 public static class Swing {}
 public static class Vector {public double x=10,y=64,z=0;}
 public record Explosion(Vector center,float radius){}
 static AntiVanishBridge.Api api()throws Exception{
  var table=new ArrayList<String[]>();
  table.addAll(List.of(type("Sound",Sound.class),type("Particles",Particle.class),type("Player",Player.class),type("Projectile",Projectile.class),type("Villager",Villager.class),type("Pos",Pos.class),type("Swing",Swing.class),type("Explosion",Explosion.class)));
  table.addAll(List.of(field("menu",Entity.class,"menu",Object.class),field("inventoryMenu",Entity.class,"inventoryMenu",Object.class),field("poweredProperty",AntiVanishSensorCheck.class,"property",Object.class),field("particleRegistry",AntiVanishSensorCheck.class,"registry",Registry.class)));
  for(String axis:List.of("X","Y","Z")){
   table.add(method("entity"+axis,Entity.class,axis.toLowerCase(),double.class));table.add(method("sound"+axis,Sound.class,axis.toLowerCase(),double.class));table.add(method("particles"+axis,Particle.class,axis.toLowerCase(),double.class));table.add(field("vec"+axis,Vector.class,axis.toLowerCase(),double.class));
  }
  table.addAll(List.of(method("sound",Sound.class,"holder",Sound.class),method("holder",Sound.class,"value",Sound.class),method("soundId",Sound.class,"id",String.class),
   method("particle",Particle.class,"particle",Particle.class),method("particleType",Particle.class,"type",Particle.class),method("registryKey",Registry.class,"key",String.class,Object.class),
   method("entities",World.class,"entities",Iterable.class),method("invisible",Entity.class,"invisible",boolean.class),method("loaded",World.class,"loaded",boolean.class,Pos.class),
   method("state",World.class,"state",State.class,Pos.class),method("block",State.class,"block",State.class),method("blockKey",State.class,"key",String.class),
   method("powered",World.class,"powered",boolean.class,Pos.class),method("hasProperty",State.class,"has",boolean.class,Object.class),method("property",State.class,"value",Comparable.class,Object.class),
   method("explosionPos",Explosion.class,"center",Vector.class),method("explosionRadius",Explosion.class,"radius",float.class)));
  return new AntiVanishBridge.Api(AntiVanishSensorCheck.class.getClassLoader(),table.toArray(String[][]::new));
 }
 public static void main(String[] args)throws Exception {
  var a=api();var world=new World();var local=new Player();var t=new AntiVanishTracker();var chest=new Sound("minecraft:block.chest.open");long now=10000;
  a.observe(chest,false,world,local,t,now,true);check(has(t.rows(now),32),"Unexplained chest sound missing");t.clear();
  var visible=new Player();visible.x=10;world.entities.add(visible);a.observe(chest,false,world,local,t,now,true);check(t.rows(now).isEmpty(),"Visible cause not suppressed");world.entities.clear();
  var projectile=new Projectile();projectile.x=10;world.entities.add(projectile);a.observe(new Particle("crit"),false,world,local,t,now,true);check(t.rows(now).isEmpty(),"Projectile particles not suppressed");world.entities.clear();
  a.observe(new Swing(),true,world,local,t,now,true);a.observe(chest,false,world,local,t,now+500,true);check(t.rows(now+500).isEmpty(),"Own action not suppressed");a.clear();
  local.menu=new Object();a.observe(chest,false,world,local,t,now,true);local.menu=local.inventoryMenu;a.observe(chest,false,world,local,t,now+2000,true);check(t.rows(now+2000).isEmpty(),"Container grace ignored");a.clear();
  a.observe(new Explosion(new Vector(),4),false,world,local,t,now,true);a.observe(new Particle("crit"),false,world,local,t,now+100,true);check(t.rows(now+100).isEmpty(),"Explosion not suppressed");a.clear();
  var door=new Sound("minecraft:block.wooden_door.open");world.state.powered=true;a.observe(door,false,world,local,t,now,true);world.state.powered=false;a.observe(door,false,world,local,t,now+100,true);check(t.rows(now+100).isEmpty(),"Powered door memory ignored");a.clear();
  var villager=new Villager();villager.x=10;world.entities.add(villager);a.observe(door,false,world,local,t,now,true);check(t.rows(now).isEmpty(),"Villager door not suppressed");world.entities.clear();
  world.state.key="block.minecraft.campfire";a.observe(new Particle("smoke"),false,world,local,t,now,true);a.observe(new Particle("smoke"),false,world,local,t,now+100,true);check(t.rows(now+100).isEmpty(),"Natural smoke not suppressed");world.state.key="block.minecraft.air";
  a.observe(new Particle("smoke"),false,world,local,t,now+200,true);check(t.rows(now+200).isEmpty(),"Weak particle did not require burst");a.observe(new Particle("smoke"),false,world,local,t,now+300,true);check(has(t.rows(now+300),64),"Repeated unexplained particles missing");t.clear();a.clear();
  world.loaded=false;a.observe(new Particle("smoke"),false,world,local,t,now+400,true);a.observe(new Particle("smoke"),false,world,local,t,now+500,true);check(t.rows(now+500).isEmpty(),"Unloaded cause scan did not fail closed");
  System.out.println("PASS Anti Vanish sensors: visible causes, projectiles, own actions, containers, explosions, redstone, villagers, natural smoke and weak bursts");
 }
}
