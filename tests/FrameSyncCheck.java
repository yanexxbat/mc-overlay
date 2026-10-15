package mcov.sync;

import java.net.URLClassLoader;
import java.nio.file.Path;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import mcov.FrameSignal;

/** Actual instrumented Java fixture bytecode; no game code or render JNI. */
public final class FrameSyncCheck {
    private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    private static void checkFreecam(URLClassLoader loader,FrameProfiles.Profile p,int index) throws Exception {
        Class<?> local=Class.forName(p.localPlayer().replace('/','.'),true,loader);
        Object self=local.getConstructor().newInstance(),other=local.getConstructor().newInstance();
        Class<?> camera=Class.forName(p.camera().replace('/','.'),true,loader),client=Class.forName(p.client().replace('/','.'),true,loader);
        Object view=camera.getConstructor().newInstance(),owner=client.getConstructor().newInstance();
        require((FrameSignal.freecamProfiles&(1L<<index))!=0,"Missing freecam hooks "+p.id());
        Method update=camera.getDeclaredMethod(p.update(),java.lang.invoke.MethodType.fromMethodDescriptorString(p.updateDesc(),loader).parameterArray());
        Object[] args=new Object[update.getParameterCount()];
        for(int i=0;i<args.length;i++) {
            Class<?> t=update.getParameterTypes()[i];
            if(t==boolean.class) args[i]=false; else if(t==float.class) args[i]=.5f;
        }
        FrameSignal.silentPlayer=self; FrameSignal.viewYaw=57; FrameSignal.viewPitch=-23;
        try {
            FrameSignal.updateFreecam(123,81,-46); update.invoke(view,args);
            Object pos=camera.getField(p.positionField()).get(view);
            require(pos!=null,"Camera setter not executed "+p.id());
            // The synthetic camera records actual setter arguments independently of view getters.
            require(camera.getField("testFreeYaw").getFloat(view)==57&&camera.getField("testFreePitch").getFloat(view)==-23,"F5 changed freecam rotation "+p.id());
            require(camera.getField(p.detached()).getBoolean(view),"Freecam kept first-person hand/model mode "+p.id());
            for(java.lang.reflect.Field f:pos.getClass().getFields()) if(f.getType()==double.class)
                require(java.util.Set.of(123.0,81.0,-46.0).contains(f.getDouble(pos)),"Freecam did not reach the render camera "+p.id());
            java.lang.reflect.Field movement=local.getField("testMovementYaw"); movement.setFloat(self,-777);
            local.getMethod(p.localStep()).invoke(self);
            require(movement.getFloat(self)==-777,"Freecam moved the real player "+p.id());
            local.getMethod(p.localStep()).invoke(other);
            require(movement.getFloat(other)!=-777,"Freecam froze another entity "+p.id());
            int uses=client.getField("testUses").getInt(null),clicks=client.getField("testClicks").getInt(null),breaks=client.getField("testBreaks").getInt(null);
            client.getMethod(p.use()).invoke(owner); client.getMethod(p.attack()).invoke(owner); client.getMethod(p.continueAttack(),boolean.class).invoke(owner,true);
            require(client.getField("testUses").getInt(null)==uses&&client.getField("testClicks").getInt(null)==clicks&&client.getField("testBreaks").getInt(null)==breaks,"Freecam allowed interactions "+p.id());
            java.lang.reflect.Field at=FrameSignal.class.getDeclaredField("freecamAt"); at.setAccessible(true); at.setLong(null,System.nanoTime()-300_000_000L);
            require(!FrameSignal.freecamActive()&&!FrameSignal.freecamFrame(),"Freecam outlived its lease "+p.id());
            FrameSignal.stopFreecam(); local.getMethod(p.localStep()).invoke(self);
            require(movement.getFloat(self)!=-777,"Freecam did not restore player movement "+p.id());
            client.getMethod(p.use()).invoke(owner); client.getMethod(p.attack()).invoke(owner); client.getMethod(p.continueAttack(),boolean.class).invoke(owner,true);
            require(client.getField("testUses").getInt(null)==uses+1&&client.getField("testClicks").getInt(null)==clicks+1&&client.getField("testBreaks").getInt(null)==breaks+1,"Freecam did not restore interactions "+p.id());
        } finally { FrameSignal.stopFreecam(); FrameSignal.silentPlayer=null; }
    }
    private static void checkMovement(URLClassLoader loader, FrameProfiles.Profile p, int index) throws Exception {
        Class<?> entity=Class.forName(p.entity().replace('/','.'),true,loader);
        Class<?> living=Class.forName(p.living().replace('/','.'),true,loader);
        Class<?> player=Class.forName(p.player().replace('/','.'),true,loader);
        Class<?> local=Class.forName(p.localPlayer().replace('/','.'),true,loader);
        Class<?> vector=Class.forName(p.travelDesc().substring(2,p.travelDesc().indexOf(';')).replace('/','.'),true,loader);
        Object self=local.getConstructor().newInstance(),other=local.getConstructor().newInstance(),input=vector.getConstructor().newInstance();
        Method move=entity.getMethod(p.move(),float.class,vector),yaw=entity.getMethod(p.yaw());
        Method[] scopes={living.getMethod(p.travel(),vector),living.getMethod(p.jump()),
            player.getMethod(p.playerTravel(),vector),local.getMethod(p.localStep())};
        if(!p.playerJump().isEmpty()) player.getMethod(p.playerJump()).invoke(self);
        entity.getField("testYaw").setFloat(self,127);
        entity.getField("testPitch").setFloat(self,-78);
        entity.getField("testYaw").setFloat(other,-43);
        FrameSignal.silentPlayer=self; FrameSignal.movementEnabled=true;
        try {
            require((FrameSignal.movementProfiles & (1L<<index))!=0,"Missing movement scope "+p.id());
            for(float angle : new float[]{-179,-90,0,60,179}) {
                FrameSignal.viewYaw=angle; FrameSignal.viewPitch=23;
                for(Method method:scopes) {
                    method.invoke(self,method.getParameterCount()==0 ? new Object[0] : new Object[]{input});
                    require(entity.getField("testMovementYaw").getFloat(self)==angle,"Movement followed spin instead of view "+p.id());
                    require(entity.getField("testMovementPitch").getFloat(self)==23,"Flight/swim pitch followed spin "+p.id());
                    require((float)yaw.invoke(self)==127,"Physics scope leaked into model/network getter "+p.id());
                }
            }
            // Ein Callee wirft: der äußerste und alle inneren Scopes müssen enden.
            entity.getField("testMovementGuard").set(self,null);
            try { scopes[3].invoke(self); throw new AssertionError("Fixture exception missing"); }
            catch(InvocationTargetException error) { require(error.getCause() instanceof NullPointerException,"Wrong fixture exception"); }
            require((float)yaw.invoke(self)==127,"Exception left movement override active "+p.id());
            entity.getField("testMovementGuard").set(self,new Object());
            move.invoke(other,1f,input);
            require(entity.getField("testMovementYaw").getFloat(other)==-43,"Movement changed another entity "+p.id());
            FrameSignal.movementEnabled=false; scopes[3].invoke(self);
            require(entity.getField("testMovementYaw").getFloat(self)==127,"Disabling left movement override active "+p.id());
        } finally { FrameSignal.movementEnabled=false; FrameSignal.silentPlayer=null; }
    }
    public static void main(String[] args) throws Exception {
        require(FrameSignal.enabled, "Bootstrap signals not installed");
        for (FrameProfiles.Profile profile : FrameProfiles.PROFILES) {
          for (String suffix : new String[]{"", "-field-camera"}) {
            Path jar = Path.of(args[0], profile.id() + suffix + ".jar");
            try (URLClassLoader loader = new URLClassLoader(new java.net.URL[]{jar.toUri().toURL()}, null)) {
                Class<?> client = Class.forName(profile.client().replace('/', '.'), true, loader);
                Class<?> camera = Class.forName(profile.camera().replace('/', '.'), true, loader);
                Object owner = client.getConstructor().newInstance(), view = camera.getConstructor().newInstance();
                Method frame = client.getDeclaredMethod(profile.frame(), boolean.class);
                Method update=camera.getDeclaredMethod(profile.update(),java.lang.invoke.MethodType.fromMethodDescriptorString(profile.updateDesc(),loader).parameterArray());
                require(update != null, "Missing camera method");
                Class<?>[] types = update.getParameterTypes();
                checkMovement(loader,profile,java.util.Arrays.asList(FrameProfiles.PROFILES).indexOf(profile));
                checkFreecam(loader,profile,java.util.Arrays.asList(FrameProfiles.PROFILES).indexOf(profile));
                for (int iteration = 0; iteration < 200; iteration++) {
                    frame.invoke(owner, true);
                    require((FrameSignal.epoch & 1) != 0, "New entity tick accepted with stale camera");
                    Object[] values = new Object[types.length];
                    for (int i = 0; i < types.length; i++) {
                        if (types[i] == boolean.class) values[i] = false;
                        else if (types[i] == float.class) values[i] = .5f;
                    }
                    if ((iteration & 1) != 0) values[0] = types[0].getConstructor().newInstance();
                    update.invoke(view, values);
                    long completed = FrameSignal.epoch;
                    require((completed & 1) == 0, "Camera return did not complete the frame");
                    frame.invoke(owner, true);
                    require(FrameSignal.epoch != completed && (FrameSignal.epoch & 1) != 0, "Mixed capture was accepted");
                }
                System.out.println("PASS frame markers " + profile.id() + suffix);
            }
          }
        }
    }
}
