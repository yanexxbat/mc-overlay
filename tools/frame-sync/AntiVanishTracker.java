package mcov;
import java.util.*;
/** Bounded passive evidence tracking; disappearance alone is not a detection. */
public final class AntiVanishTracker {
    public static final int MAX_KNOWN=512,MAX_ROWS=32;
    public record Player(UUID id,String name,boolean listed,boolean online,boolean nearby,boolean invisible) {}
    public record Detection(UUID id,String name,int evidence) {}
    private static final class Known { String name;long listedSince=-1,lastSeen,hiddenSince=-1; }
    private static final java.util.regex.Pattern NAME=java.util.regex.Pattern.compile("[A-Za-z0-9_]{3,16}");
    private final LinkedHashMap<UUID,Known> known=new LinkedHashMap<>();
    public void clear() { known.clear(); }
    public int size() { return known.size(); }
    public List<Detection> sample(List<Player> input,UUID self,long now,boolean includeInvisible) {
        ArrayList<Detection> rows=new ArrayList<>();int processed=0;
        for(Player p:input) {
            if(++processed>MAX_KNOWN)break;
            if(p.id==null||p.id.equals(self)||p.name==null||!NAME.matcher(p.name).matches())continue;
            Known k=known.get(p.id);
            if(k==null) { if(known.size()>=MAX_KNOWN)known.remove(known.keySet().iterator().next());k=new Known();known.put(p.id,k); }
            k.name=p.name;k.lastSeen=now;
            if(p.listed&&k.listedSince<0)k.listedSince=now;
            boolean trusted=k.listedSince>=0&&now-k.listedSince>=1500;
            int evidence=trusted&&!p.listed&&(p.online||p.nearby)?1:0;
            if(evidence!=0&&p.nearby)evidence|=2;
            if(trusted&&includeInvisible&&p.nearby&&p.invisible)evidence|=4;
            if(evidence==0) { k.hiddenSince=-1;continue; }
            if(k.hiddenSince<0)k.hiddenSince=now;
            if(now-k.hiddenSince>=1000&&rows.size()<MAX_ROWS)rows.add(new Detection(p.id,k.name,evidence));
        }
        for(Known k:known.values())if(k.lastSeen<now)k.hiddenSince=-1;
        known.values().removeIf(k->now-k.lastSeen>60_000);
        return rows;
    }
}
