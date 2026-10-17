package mcov.sync;
import java.util.*;
import jdk.internal.org.objectweb.asm.*;
import mcov.AntiVanishProfiles;
import mcov.AntiVanishPackets;

/** Runs transformed bytecode and checks hook arguments as well as original delivery. */
public final class AntiVanishHookCheck implements Opcodes {
    static int deliveries;
    public static void delivered(){++deliveries;}
    static final class Loader extends ClassLoader {
        Class<?> put(String name,byte[] bytes){return defineClass(name.replace('/','.'),bytes,0,bytes.length);}
    }
    static ClassWriter type(String name){
        var w=new ClassWriter(ClassWriter.COMPUTE_MAXS);w.visit(V17,ACC_PUBLIC,name,null,"java/lang/Object",null);
        var m=w.visitMethod(ACC_PUBLIC,"<init>","()V",null,null);m.visitCode();m.visitVarInsn(ALOAD,0);
        m.visitMethodInsn(INVOKESPECIAL,"java/lang/Object","<init>","()V",false);m.visitInsn(RETURN);m.visitMaxs(0,0);m.visitEnd();return w;
    }
    static byte[] network(String[][] profile,boolean marker){
        var w=type(AntiVanishProfiles.row(profile,"class.Network")[1]);
        for(String key:List.of("packetIn","packetOut","networkMarker")){
            if(key.equals("networkMarker")&&!marker)continue;var row=AntiVanishProfiles.row(profile,key);
            var m=w.visitMethod(ACC_PUBLIC,row[2],row[3],null,null);m.visitCode();
            if(key.equals("networkMarker")){m.visitInsn(ACONST_NULL);m.visitInsn(ARETURN);}
            else {m.visitMethodInsn(INVOKESTATIC,"mcov/sync/AntiVanishHookCheck","delivered","()V",false);m.visitInsn(RETURN);}
            m.visitMaxs(0,0);m.visitEnd();
        }w.visitEnd();return w.toByteArray();
    }
    static void set(String key,Object value)throws Exception{var f=AntiVanishPackets.class.getDeclaredField(key);f.setAccessible(true);f.set(null,value);}
    static Object get(Object value,String key)throws Exception{var m=value.getClass().getDeclaredMethod(key);m.setAccessible(true);return m.invoke(value);}
    public static void main(String[] args)throws Exception{
        var q=AntiVanishPackets.class.getDeclaredField("queue");q.setAccessible(true);var queue=(Queue<?>)q.get(null);
        for(String[][] profile:AntiVanishProfiles.all()){
            var loader=new Loader();String name=AntiVanishProfiles.row(profile,"class.Network")[1];
            String packetName=AntiVanishProfiles.row(profile,"class.Packet")[1],context="io/netty/channel/ChannelHandlerContext";
            Class<?> packet=loader.put(packetName,type(packetName).toByteArray()),ctx=loader.put(context,type(context).toByteArray());
            var transformer=new AntiVanishTransformer(null,null);
            if(transformer.transform(null,loader,name,null,null,network(profile,false))!=null)throw new AssertionError("Version guard accepted missing marker");
            byte[] transformed=transformer.transform(null,loader,name,null,null,network(profile,true));
            if(transformed==null)throw new AssertionError("Hook missing: "+name);
            Class<?> net=loader.put(name,transformed);Object connection=net.getConstructor().newInstance(),message=packet.getConstructor().newInstance();
            set("incoming",Set.of(packet));set("outgoing",Set.of(packet));set("connection",connection);set("deadline",System.nanoTime()+5_000_000_000L);queue.clear();
            net.getMethod(AntiVanishProfiles.row(profile,"packetIn")[2],ctx,packet).invoke(connection,null,message);
            net.getMethod(AntiVanishProfiles.row(profile,"packetOut")[2],packet).invoke(connection,message);
            if(queue.size()!=2)throw new AssertionError("Hooks did not observe both directions");
            for(boolean outgoing:List.of(false,true)){Object e=queue.poll();
                if(get(e,"connection")!=connection||get(e,"packet")!=message||!get(e,"outbound").equals(outgoing))throw new AssertionError("Hook changed packet or direction");
            }
            set("deadline",0L);net.getMethod(AntiVanishProfiles.row(profile,"packetOut")[2],packet).invoke(connection,message);
            if(!queue.isEmpty())throw new AssertionError("Disabled hook retained packet");
            System.out.println("PASS packet hook "+name);
        }
        if(deliveries!=9)throw new AssertionError("Original network delivery changed");
    }
}
