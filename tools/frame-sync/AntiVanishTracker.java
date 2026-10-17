package mcov;
import java.util.*;
import java.util.regex.Pattern;

/** Evidence engine adapted from the supplied AntiVanishModule; independent of game classes. */
public final class AntiVanishTracker {
    public static final int MAX_KNOWN=512,MAX_ROWS=32;
    public static final int HIDDEN=1,NEARBY=2,INVISIBLE=4,SPECTATOR=8,TARGETABLE=16,SOUND=32,PARTICLE=64,CRITICAL=128,VISIBLE=256;
    private static final Set<String> STAFF=Set.of("fluffymaster07","archivepedro","munkerlich","frenk_btw","napooo_","auzzitech","cryptodaveyt","w1zox_","zeef69","showered","captainmoose35","bobisfound","noahvdaa","0gsummer","lzouzmp5","pastagamer08","u_vv","owen1212055","splaterd","fallerfly","bloodspulse");
    public record Player(UUID id,String name,boolean listed,boolean online,boolean nearby,boolean invisible,boolean spectator) {
        public Player(UUID id,String name,boolean listed,boolean online,boolean nearby,boolean invisible){this(id,name,listed,online,nearby,invisible,false);}
    }
    public record Detection(UUID id,String name,int evidence,String reason,int score,long expires) {}
    private static final class Known {String name;long listedSince=-1,lastSeen,hiddenSince=-1;boolean listed,spectator,suppressed;}
    private record Pending(UUID id,long due) {}
    private record Message(String text,long at) {}
    private record Signal(UUID id,String subject,int type,int weight,boolean nearby,long at) {}
    private final LinkedHashMap<UUID,Known> known=new LinkedHashMap<>();
    private final LinkedHashMap<String,Detection> detections=new LinkedHashMap<>();
    private final Map<UUID,Player> current=new HashMap<>();
    private final Map<UUID,Pending> pending=new LinkedHashMap<>();
    private final Map<UUID,Long> departures=new HashMap<>();
    private final Map<String,Long> cooldowns=new HashMap<>();
    private final Deque<Message> messages=new ArrayDeque<>();
    private final Deque<Signal> signals=new ArrayDeque<>();
    private boolean staffOnly=true,leaveMessages;private UUID self;private String selfName="";private long lastCritical=-12000;
    private static final Pattern NAME=Pattern.compile("[A-Za-z0-9_]{3,16}");
    public void configure(boolean onlyStaff){if(staffOnly!=onlyStaff){clear();staffOnly=onlyStaff;}}
    public void clear(){known.clear();current.clear();pending.clear();departures.clear();detections.clear();messages.clear();signals.clear();cooldowns.clear();leaveMessages=false;lastCritical=-12000;self=null;selfName="";}
    public int size(){return known.size();}
    public void local(UUID id,String name){self=id;selfName=name==null?"":name;}
    private boolean eligible(String name){return name!=null&&NAME.matcher(name).matches()&&(!staffOnly||STAFF.contains(name.toLowerCase(Locale.ROOT)));}
    private boolean trusted(UUID id,long now){Known k=known.get(id);return k!=null&&eligible(k.name)&&k.listedSince>=0&&now-k.listedSince>=1500&&!id.equals(self);}
    private boolean left(String name,long now){return messages.stream().anyMatch(m->now-m.at<=8000&&looksLikeLeave(m.text,name));}
    public static boolean looksLikeLeave(String text,String name){
        if(text==null||name==null)return false;
        text=java.text.Normalizer.normalize(text.replaceAll("(?i)\u00a7[0-9a-fk-orx]",""),java.text.Normalizer.Form.NFKC).replaceAll("[\\p{Cf}]","");
        return Pattern.compile("(?i)(?<![A-Za-z0-9_])"+Pattern.quote(name)+"(?![A-Za-z0-9_])\\s+(?:(?:has\\s+)?(?:left|quit)|disconnected|logged\\s+out|hat\\s+(?:das\\s+Spiel|den\\s+Server)\\s+verlassen)(?:\\b|$)").matcher(text).find();
    }
    public void message(String text,String departed,long now){
        if(text!=null&&!text.isBlank()){messages.addLast(new Message(text,now));while(messages.size()>64)messages.removeFirst();}
        if(departed!=null&&!departed.isBlank())leaveMessages=true;
        for(var entry:known.entrySet()){
            String name=entry.getValue().name;
            if(name.equalsIgnoreCase(departed==null?"":departed)||looksLikeLeave(text,name)){
                leaveMessages=true;departures.put(entry.getKey(),now+5000);pending.remove(entry.getKey());
                detections.values().removeIf(d->d.id.equals(entry.getKey())&&(d.evidence&(HIDDEN|TARGETABLE|CRITICAL))!=0);
                signals.removeIf(s->s.id.equals(entry.getKey()));
            }
        }
    }
    public void removed(Collection<UUID> ids,long now){
        if(ids.size()>=4){suppressBatch(ids);return;}
        for(UUID id:ids)if(id!=null&&trusted(id,now))pending.put(id,new Pending(id,now+1000));
    }
    public void tabHidden(Collection<UUID> ids,long now){
        if(ids.size()>=4){suppressBatch(ids);return;}
        for(UUID id:ids)if(id!=null&&trusted(id,now))hidden(id,known.get(id).name,HIDDEN,"Hidden from TAB",100,now);
    }
    private void suppressBatch(Collection<UUID> ids){for(UUID id:ids){Known k=known.get(id);if(k!=null)k.suppressed=true;pending.remove(id);}}
    public void mode(UUID id,boolean spectator,long now){
        if(id==null||!trusted(id,now))return;Known k=known.get(id);
        if(spectator)hidden(id,k.name,SPECTATOR,"Switched to spectator",100,now);
        else if(k.spectator)visible(id,k.name,now);k.spectator=spectator;
    }
    private void put(UUID id,String name,int flags,String reason,int score,long now,long ttl){
        String key=(flags==CRITICAL?"critical:":(flags&(SOUND|PARTICLE|INVISIBLE))!=0?"sensor:"+flags+":":"player:")+id;
        detections.put(key,new Detection(id,name,flags,reason,score,now+ttl));
        while(detections.size()>128)detections.remove(detections.keySet().iterator().next());
    }
    private void hidden(UUID id,String name,int flags,String reason,int score,long now){
        if(left(name,now)||departures.getOrDefault(id,0L)>now)return;
        Player p=current.get(id);boolean nearby=p!=null&&p.nearby;
        Detection previous=detections.get("player:"+id);
        put(id,name,flags|(nearby?NEARBY:0),reason,score,now,20000);
        if(nearby&&(previous==null||(previous.evidence&NEARBY)==0)&&now-lastCritical>=12000){lastCritical=now;put(id,name,CRITICAL,"Possible hidden observer nearby",100,now,7000);}
    }
    private void visible(UUID id,String name,long now){
        // Completion hints can precede the first UUID-bearing roster entry.
        boolean removed=detections.values().removeIf(d->(d.id.equals(id)||d.name.equalsIgnoreCase(name))&&(d.evidence&(HIDDEN|TARGETABLE|SPECTATOR|INVISIBLE|CRITICAL))!=0);
        signals.removeIf(s->s.id.equals(id)||s.subject.equalsIgnoreCase(name));
        if(removed)put(id,name,VISIBLE,"Visible again",5,now,4000);
    }
    public List<Detection> sample(List<Player> input,UUID local,long now,boolean includeInvisible){
        self=local;current.clear();int processed=0,hiddenTransitions=0;
        for(Player p:input){if(++processed>MAX_KNOWN)break;if(p.id!=null&&p.id.equals(self)){selfName=p.name;continue;}if(p.id==null||!eligible(p.name))continue;
            current.put(p.id,p);Known k=known.get(p.id);if(k!=null&&k.listed&&!p.listed)++hiddenTransitions;
        }
        for(Player p:current.values()){
            Known k=known.get(p.id);
            if(k==null){if(known.size()>=MAX_KNOWN)known.remove(known.keySet().iterator().next());k=new Known();k.spectator=p.spectator;known.put(p.id,k);}
            k.name=p.name;k.lastSeen=now;
            if(p.listed&&k.listedSince<0)k.listedSince=now;
            if(p.listed)k.suppressed=false;else if(k.listed&&hiddenTransitions>=4)k.suppressed=true;
            boolean trust=trusted(p.id,now);
            if(trust&&p.spectator&&!k.spectator)hidden(p.id,p.name,SPECTATOR,"Switched to spectator",100,now);
            if(!p.spectator&&k.spectator)visible(p.id,p.name,now);
            if(trust&&!p.listed&&(p.online||p.nearby)&&!k.suppressed){
                if(k.hiddenSince<0)k.hiddenSince=now;
                if(now-k.hiddenSince>=1000)hidden(p.id,p.name,HIDDEN,p.nearby?"Off TAB, entity still nearby":"Hidden from TAB",100,now);
            }else if(p.listed){k.hiddenSince=-1;if(!p.spectator&&!(includeInvisible&&p.invisible))visible(p.id,p.name,now);}
            if(trust&&includeInvisible&&p.nearby&&p.invisible)sensor(p.id,p.name,INVISIBLE,"Invisible entity: metadata flag",35,true,now,5000);
            k.listed=p.listed;k.spectator=p.spectator;
        }
        for(var entry:known.entrySet())if(!current.containsKey(entry.getKey()))entry.getValue().hiddenSince=-1;
        advance(now);return rows(now);
    }
    public void advance(long now){
        for(var it=pending.values().iterator();it.hasNext();){Pending p=it.next();if(now<p.due)continue;it.remove();
            Known k=known.get(p.id);Player player=current.get(p.id);
            if(k==null||player!=null&&player.online||departures.getOrDefault(p.id,0L)>now||left(k.name,now))continue;
            if(player!=null&&player.nearby)hidden(p.id,k.name,HIDDEN,"Off TAB, entity still nearby",100,now);
            else if(leaveMessages)hidden(p.id,k.name,HIDDEN,"Silent TAB disappearance",100,now);
        }
        known.values().removeIf(k->now-k.lastSeen>60000);
        detections.values().removeIf(d->d.expires<=now);departures.values().removeIf(until->until<=now);
        cooldowns.values().removeIf(at->now-at>60000);
        while(!messages.isEmpty()&&now-messages.peekFirst().at>8000)messages.removeFirst();
        while(!signals.isEmpty()&&now-signals.peekFirst().at>15000)signals.removeFirst();
    }
    public void completions(Collection<String> names,long now){
        LinkedHashSet<String> candidates=new LinkedHashSet<>();
        for(String name:names){if(candidates.size()>3)return;if(!eligible(name)||name.equalsIgnoreCase(selfName)||left(name,now))continue;
            boolean online=current.values().stream().anyMatch(p->p.name.equalsIgnoreCase(name)&&p.online);
            if(!online)candidates.add(name);
        }
        if(candidates.size()>3)return;
        for(String name:candidates){UUID id=known.entrySet().stream().filter(e->e.getValue().name.equalsIgnoreCase(name)).map(Map.Entry::getKey).findFirst().orElse(null);
            // Outside the source's fixed staff list, only previously trusted accounts are credible.
            if(!staffOnly&&(id==null||!trusted(id,now)))continue;
            if(id==null)id=UUID.nameUUIDFromBytes(("mcov:staff:"+name.toLowerCase(Locale.ROOT)).getBytes(java.nio.charset.StandardCharsets.UTF_8));
            if(!id.equals(self))hidden(id,name,TARGETABLE,"Off TAB, still targetable",90,now);
        }
    }
    public void sensor(UUID id,String name,int type,String reason,int weight,boolean nearby,long now,long cooldown){
        if(id!=null&&!trusted(id,now))return;
        if(id==null)id=UUID.nameUUIDFromBytes(("mcov:sensor:"+name).getBytes(java.nio.charset.StandardCharsets.UTF_8));
        String key=type+":"+id;Long last=cooldowns.get(key);if(last!=null&&now-last<cooldown)return;cooldowns.put(key,now);
        put(id,name,type|(nearby?NEARBY:0),reason,weight,now,20000);
        signals.addLast(new Signal(id,name,type,weight,nearby,now));while(signals.size()>256)signals.removeFirst();
        Map<Integer,Integer> strongest=new HashMap<>();Signal watcher=null;
        for(Signal s:signals)if(now-s.at<=15000){strongest.merge(s.type,s.weight,Math::max);if(s.nearby)watcher=s;}
        if(watcher!=null&&strongest.size()>=2&&strongest.values().stream().mapToInt(Integer::intValue).sum()>=35&&now-lastCritical>=12000){
            lastCritical=now;put(watcher.id,watcher.subject,CRITICAL,"Multiple hints near a hidden player",100,now,7000);
        }
    }
    public List<Detection> rows(long now){
        advance(now);
        return detections.values().stream().filter(d->d.expires>now).sorted(Comparator.<Detection>comparingInt(d->(d.evidence&CRITICAL)!=0?1000:d.score).reversed().thenComparing(Detection::name,String.CASE_INSENSITIVE_ORDER)).limit(MAX_ROWS).toList();
    }
}
