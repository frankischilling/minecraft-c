import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.ByteArrayOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Modification watchpoints observe original stores without writing target state. */
public final class NwOffsetJdi {
    static String bits(Value value) { return String.format(java.util.Locale.ROOT, "%016x", Double.doubleToRawLongBits(((DoubleValue)value).value())); }
    static String state(ObjectReference receiver) {
        ReferenceType type = receiver.referenceType();
        return bits(receiver.getValue(type.fieldByName("s"))) + "," + bits(receiver.getValue(type.fieldByName("t"))) + "," + bits(receiver.getValue(type.fieldByName("u")));
    }
    static int helper(ReferenceType type, String name) { return ((IntegerValue)type.getValue(type.fieldByName(name))).value(); }
    static Thread drain(java.io.InputStream input, ByteArrayOutputStream output) {
        Thread result = new Thread(() -> {try {input.transferTo(output);} catch (Exception e) {throw new RuntimeException(e);}});
        result.start(); return result;
    }
    public static void main(String[] args) throws Exception {
        Path output = Path.of(args[0]);
        LaunchingConnector connector = Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options = connector.defaultArguments();
        options.get("main").setValue("NwOffsetObservation \"" + args[2] + "\" \"" + args[3] + "\" \"" + args[4] + "\"");
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -cp \"" + args[1] + "\"");
        options.get("suspend").setValue("true");
        VirtualMachine vm = connector.launch(options);
        if (!vm.canWatchFieldModification()) throw new AssertionError("modification watchpoints unavailable");
        Process process = vm.process();
        ByteArrayOutputStream stdout = new ByteArrayOutputStream(), stderr = new ByteArrayOutputStream();
        Thread out = drain(process.getInputStream(), stdout), err = drain(process.getErrorStream(), stderr);
        EventRequestManager requests = vm.eventRequestManager();
        ClassPrepareRequest prepare = requests.createClassPrepareRequest();
        prepare.addClassFilter("nw"); prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); prepare.enable();
        boolean done = false;
        int events = 0;
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(60);
        while (!done && System.nanoTime() < deadline) {
            EventSet set;
            try {set = vm.eventQueue().remove(1000);} catch (VMDisconnectedException e) {break;}
            if (set == null) continue;
            for (Event event : set) {
                if (event instanceof ClassPrepareEvent) {
                    ReferenceType type = ((ClassPrepareEvent)event).referenceType();
                    for (String name : new String[]{"s","t","u"}) {
                        ModificationWatchpointRequest request = requests.createModificationWatchpointRequest(type.fieldByName(name));
                        request.setSuspendPolicy(EventRequest.SUSPEND_ALL); request.enable();
                    }
                    for (Method method : type.methodsByName("c", "(FFF)V")) {
                        byte[] code = method.bytecodes();
                        System.out.println("NW_LIVE_CODE\t" + type.name() + "\t" + method.name() + "\t" + method.signature() + "\t" + code.length + "\t" + HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code)));
                    }
                } else if (event instanceof ModificationWatchpointEvent) {
                    ModificationWatchpointEvent modification = (ModificationWatchpointEvent)event;
                    Method method = modification.location().method();
                    if (method.name().equals("c") && method.signature().equals("(FFF)V")) {
                        ReferenceType helper = vm.classesByName("NwOffsetObservation").get(0);
                        ObjectReference singleton = (ObjectReference)modification.object().referenceType().getValue(modification.object().referenceType().fieldByName("a"));
                        if (!singleton.equals(modification.object())) throw new AssertionError("different original singleton");
                        System.out.println("NW_STORE\t" + helper(helper,"call") + "\t" + helper(helper,"profile") + "\t" + helper(helper,"step") + "\t" + modification.location().codeIndex() + "\t" + modification.field().name() + "\t" + state(modification.object()) + "\t" + bits(modification.valueCurrent()) + "\t" + bits(modification.valueToBe()));
                        ++events;
                    }
                } else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) done = true;
            }
            try {set.resume();} catch (VMDisconnectedException ignored) { }
        }
        if (!process.waitFor(5, TimeUnit.SECONDS)) {process.destroyForcibly();throw new AssertionError("target timeout");}
        out.join(2000);err.join(2000);
        if (out.isAlive() || err.isAlive() || process.exitValue() != 0) throw new AssertionError("target/pipe failure");
        Files.write(output.resolve("target.stdout"), stdout.toByteArray());
        Files.write(output.resolve("target.stderr"), stderr.toByteArray());
        System.out.println("NW_WATCH_COMPLETE events=" + events + " target-exit=" + process.exitValue() + " source-writes=0");
    }
}
