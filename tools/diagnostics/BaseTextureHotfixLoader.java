package mcov.diagnostics;
import com.sun.tools.attach.VirtualMachine;
public final class BaseTextureHotfixLoader {
    public static void main(String[] args) throws Exception {
        if(args.length!=3) throw new IllegalArgumentException("PID agent.jar classes");
        VirtualMachine vm=VirtualMachine.attach(args[0]);
        try { vm.loadAgent(args[1],args[2]); System.out.println("Repaired loaded mcov mesh consumer."); }
        finally { vm.detach(); }
    }
}
