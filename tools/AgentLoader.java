import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

import java.io.File;
import java.io.IOException;

/**
 * Loads the Java frame markers and mcov_agent.dll into an already-running JVM.
 *
 * <p>This is the sanctioned way to get native code into a live JVM. The
 * alternative - injecting a DLL with CreateRemoteThread and hunting for the
 * JavaVM* by hand - fights the VM instead of cooperating with it and breaks on
 * every JVM update. The attach handshake, by contrast, is a supported public API
 * that has been stable since Java 6.
 *
 * <p>What actually happens under the hood on Windows: VirtualMachine.attach()
 * signals the target JVM's attach listener, the JVM spins up its listener thread
 * and creates a named pipe, and loadAgentPath() sends a "load" command down it.
 * The target then calls LoadLibrary on our DLL and invokes Agent_OnAttach.
 *
 * <p>Requirements:
 * <ul>
 *   <li>Run on the same Java MAJOR as the target (17, 21 or 25). Mixed major versions are the usual
 *       cause of AttachNotSupportedException.</li>
 *   <li>jdk.attach must be present - it is, in a full JDK. A JRE-only
 *       install will not have it.</li>
 *   <li>Same user account as the target process.</li>
 * </ul>
 *
 * <p>Since JDK 21, attaching to a VM that did not opt in prints a warning to the
 * target's stderr ("WARNING: A JVM TI agent has been loaded dynamically"). That
 * is expected and harmless. To silence it, launch the game with
 * {@code -XX:+EnableDynamicAgentLoading}.
 *
 * <p>Usage: {@code java -cp . AgentLoader <pid> <absolute\path\to\mcov_agent.dll> [options]}
 */
public final class AgentLoader {

    private AgentLoader() {
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("usage: AgentLoader <pid> <path-to-agent.dll> [options]");
            System.exit(64);
        }

        final String pid = args[0];
        final File agent = new File(args[1]);
        final String options = args.length > 2 ? args[2] : null;

        if (!agent.isFile()) {
            System.err.println("[loader] agent not found: " + agent.getAbsolutePath());
            System.exit(66);
        }

        // loadAgentPath resolves the path inside the TARGET process, so a
        // relative path would be interpreted against the game's working
        // directory rather than ours.
        final String agentPath = agent.getAbsolutePath();
        final File frameAgent = new File(agent.getParentFile(), "tools/frame-sync-agent.jar");
        final File frameSignal = new File(agent.getParentFile(), "tools/frame-signal.jar");
        if (!frameAgent.isFile() || !frameSignal.isFile()) {
            System.err.println("[loader] frame synchronization helpers are missing; rebuild/copy the complete bin/tools folder");
            System.exit(66);
        }

        System.out.println("[loader] jdk       : " + System.getProperty("java.version"));
        System.out.println("[loader] target pid: " + pid);
        System.out.println("[loader] agent     : " + agentPath);

        VirtualMachine vm = null;
        int result = 0;
        try {
            vm = VirtualMachine.attach(pid);
            final String targetVersion = vm.getSystemProperties().getProperty("java.specification.version");
            final int helperMajor = Runtime.version().feature();
            if (!Integer.toString(helperMajor).equals(targetVersion)) {
                throw new IllegalArgumentException("target Java " + targetVersion +
                        " requires a matching attach JDK; helper is Java " + helperMajor);
            }
            System.out.println("[loader] target Java: " + targetVersion);
            vm.loadAgent(frameAgent.getAbsolutePath());
            System.out.println("[loader] frame synchronization installed");
            vm.loadAgentPath(agentPath, options);
            System.out.println("[loader] Agent_OnAttach returned successfully");
        } catch (AttachNotSupportedException e) {
            System.err.println("[loader] attach not supported: " + e.getMessage());
            System.err.println("[loader] check that this helper runs on the same JDK "
                    + "major version as the target, as the same user.");
            result = 70;
        } catch (AgentInitializationException e) {
            // Either the Java frame agent or Agent_OnAttach failed.
            System.err.println("[loader] agent initialization failed with code " + e.returnValue()
                    + " - see the game log and mcov_agent.log next to the DLL");
            result = 71;
        } catch (AgentLoadException e) {
            // Usually a missing dependency of the DLL itself, or a bitness mismatch.
            System.err.println("[loader] the JVM could not load an agent: " + e.getMessage());
            System.err.println("[loader] confirm the DLL is 64-bit and its CRT is available.");
            result = 72;
        } catch (IOException e) {
            System.err.println("[loader] I/O error talking to the target VM: " + e.getMessage());
            result = 73;
        } catch (IllegalArgumentException e) {
            System.err.println("[loader] " + e.getMessage());
            result = 74;
        } finally {
            if (vm != null) {
                try {
                    vm.detach();
                } catch (IOException ignored) {
                    // Detaching the *loader's* connection, not the agent. A
                    // failure here does not affect the agent already inside the
                    // target VM.
                }
            }
        }
        if (result != 0) System.exit(result);
    }
}
