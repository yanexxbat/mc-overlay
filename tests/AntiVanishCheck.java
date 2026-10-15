package mcov;
import java.util.*;
public final class AntiVanishCheck {
 static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
 static UUID id(int n){return new UUID(1,n);}
 static AntiVanishTracker.Player p(int n,boolean listed,boolean online,boolean nearby,boolean invisible){return new AntiVanishTracker.Player(id(n),"Player"+n,listed,online,nearby,invisible);}
 static List<AntiVanishTracker.Detection> sample(AntiVanishTracker t,long now,AntiVanishTracker.Player... rows){return t.sample(List.of(rows),id(0),now,false);}
 public static void main(String[] args){
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
  System.out.println("PASS Anti Vanish: trusted TAB transitions, debounce, relisting, disconnect rejection, opt-in invisibility, self/world guards and bounded/expired caches");
 }
}
