import com.sun.jdi.ArrayReference;
import com.sun.jdi.Bootstrap;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.connect.LaunchingConnector;
import com.sun.jdi.event.BreakpointEvent;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.lang.ref.WeakReference;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Asset-independent probe of the actual controller's serialized mirror IDs. */
public final class ControllerMirrorIdentityRegression {
    static final int COUNT = 128, ARRAY_BYTES = 32, TRACE_BYTES = 16 * 1024 * 1024;
    static final long LIMIT_NANOS = TimeUnit.SECONDS.toNanos(90);
    static final class Probe {
        final List<Long> ids = new ArrayList<>(COUNT);
        final List<WeakReference<ObjectReference>> mirrors = new ArrayList<>(COUNT);
        void add(ObjectReference value, long id) {
            ids.add(id); mirrors.add(new WeakReference<>(value));
        }
        int cleared() {
            int count = 0;
            for (WeakReference<ObjectReference> value : mirrors) if (value.refersTo(null)) count++;
            return count;
        }
    }
    static void need(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    static final class TraceLog extends OutputStream {
        static final Pattern DISPOSE = Pattern.compile("Disposing object ([0-9]+) \\(ref count = ([0-9]+)\\)");
        static final Pattern SEND = Pattern.compile("Sending Command\\(id=([0-9]+)\\) .*VirtualMachine\\.DisposeObjects");
        static final Pattern RECEIVE = Pattern.compile("Receiving Command\\(id=([0-9]+)\\) .*VirtualMachine\\.DisposeObjects");
        final OutputStream file;
        final ByteArrayOutputStream line = new ByteArrayOutputStream(512);
        final List<Long> pending = new ArrayList<>();
        final Map<Long,List<Long>> sent = new HashMap<>();
        final Set<Long> completed = new HashSet<>();
        final List<Long> completedPackets = new ArrayList<>();
        long bytes;
        boolean failed;
        TraceLog(Path path) throws IOException { file = new BufferedOutputStream(Files.newOutputStream(path), 64 * 1024); }
        @Override public synchronized void write(int value) throws IOException {
            try {
                if (++bytes > TRACE_BYTES || line.size() > 65536) throw new IOException("trace bound");
                file.write(value);
                if (value == '\n') { accept(line.toString(StandardCharsets.UTF_8)); line.reset(); }
                else line.write(value);
            } catch (IOException error) { failed = true; throw error; }
        }
        void accept(String text) {
            Matcher dispose = DISPOSE.matcher(text);
            if (dispose.find()) { need(Integer.parseInt(dispose.group(2)) > 0, "positive disposal count"); pending.add(Long.parseLong(dispose.group(1))); }
            Matcher send = SEND.matcher(text);
            if (send.find()) {
                need(!pending.isEmpty(), "disposal command has traced object counts");
                need(sent.put(Long.parseLong(send.group(1)), List.copyOf(pending)) == null, "unique disposal packet");
                pending.clear();
            }
            Matcher receive = RECEIVE.matcher(text);
            if (receive.find()) {
                need(!text.contains("ERROR CODE="), "successful disposal reply");
                long packet = Long.parseLong(receive.group(1));
                List<Long> ids = sent.remove(packet); need(ids != null, "disposal reply matches send");
                completed.addAll(ids); completedPackets.add(packet);
            }
        }
        synchronized boolean completedAll(List<Long> ids) { return completed.containsAll(ids); }
        synchronized int completedCount(List<Long> ids) {
            int count = 0; for (long id : ids) if (completed.contains(id)) count++; return count;
        }
        synchronized List<Long> packets() { return List.copyOf(completedPackets); }
        @Override public synchronized void flush() throws IOException {
            try { file.flush(); } catch (IOException error) { failed = true; throw error; }
        }
        @Override public synchronized void close() throws IOException {
            try { file.close(); } catch (IOException error) { failed = true; throw error; }
        }
    }
    static final class Drain implements Runnable {
        final InputStream stream;
        final Path path;
        volatile boolean failed;
        Drain(InputStream stream, Path path) { this.stream = stream; this.path = path; }
        @Override public void run() {
            try (InputStream input = stream; OutputStream output = Files.newOutputStream(path)) {
                byte[] block = new byte[4096]; int total = 0;
                for (int n; (n = input.read(block)) >= 0;) {
                    total += n; if (total > 1024 * 1024) { failed = true; return; }
                    output.write(block, 0, n);
                }
            } catch (IOException error) { failed = true; }
        }
    }
    static Probe roots(ChunkLifecycleController.Session session, ArrayReference values) throws Exception {
        Probe result = new Probe();
        for (int i = 0; i < COUNT; i++) {
            ObjectReference value = (ObjectReference) values.getValue(i);
            Object serialized;
            if (value instanceof ArrayReference bytes) serialized = session.blob(bytes, ARRAY_BYTES);
            else serialized = scalar(session, value);
            need(serialized instanceof Map<?,?>, "actual serialized identity map");
            Map<?,?> row = (Map<?,?>) serialized;
            Object number = row.get(value instanceof ArrayReference ? "id" : "object_id");
            need(number instanceof Long, "actual serialized object ID");
            result.add(value, ((Long) number).longValue());
        }
        return result;
    }
    static Object scalar(ChunkLifecycleController.Session session, Value value) throws Exception {
        java.lang.reflect.Method method;
        try { method = ChunkLifecycleController.Session.class.getDeclaredMethod("scalar", Value.class); }
        catch (NoSuchMethodException absent) { method = ChunkLifecycleController.class.getDeclaredMethod("scalar", Value.class); }
        method.setAccessible(true);
        return method.invoke(Modifier.isStatic(method.getModifiers()) ? null : session, value);
    }
    static Probe controls(ArrayReference values) {
        Probe result = new Probe();
        for (int i = 0; i < COUNT; i++) {
            ObjectReference value = (ObjectReference) values.getValue(i);
            result.add(value, value.uniqueID());
        }
        return result;
    }
    static void deadline(long started) { need(System.nanoTime() - started < LIMIT_NANOS, "probe deadline"); }
    static void pressure() {
        // Only the debugger heap is pressured. No target method is invoked.
        byte[][] blocks = new byte[256][];
        byte[] reserve = new byte[1024 * 1024];
        try { for (int i = 0; i < blocks.length; i++) blocks[i] = new byte[512 * 1024]; }
        catch (OutOfMemoryError expected) { /* Release pressure immediately in this thread. */ }
        finally { blocks = null; reserve = null; }
        System.gc();
    }
    static Map<String,Object> exercise(ChunkLifecycleController.Session session, ReferenceType target,
                                       ArrayReference anchors, Probe original, ArrayReference disposable,
                                       TraceLog trace, long started) throws Exception {
        Probe control = controls(disposable);
        int rounds = 0;
        while (control.cleared() < COUNT && rounds < 8) { deadline(started); pressure(); rounds++; }
        need(control.cleared() == COUNT, "inconclusive: disposable mirrors did not clear");
        // Do not refresh anchor IDs before their disposal. An unrelated mirror
        // drives JDI's queue; its length request flushes pending17u disposals.
        ArrayReference pump = (ArrayReference) target.getValue(target.fieldByName("PUMP"));
        need(pump.length() == 4, "unrelated queue pump");
        for (int i = 0; i < 8 && !trace.completedAll(control.ids); i++) { deadline(started); pump.getValue(i % 4); }
        need(!trace.failed && trace.completedAll(control.ids), "inconclusive: disposal completion not proved");
        int anchorCleared = original.cleared(), anchorDisposed = trace.completedCount(original.ids);
        Probe current = roots(session, anchors), currentControl = controls(disposable);
        int changed = 0, controlChanged = 0;
        for (int i = 0; i < COUNT; i++) {
            if (!original.ids.get(i).equals(current.ids.get(i))) changed++;
            if (!control.ids.get(i).equals(currentControl.ids.get(i))) controlChanged++;
        }
        need(controlChanged == COUNT, "disposed static-root controls receive new IDs");
        need(changed == 0 || changed == COUNT && anchorCleared == COUNT && anchorDisposed == COUNT,
             "partial or unexplained anchor identity change");
        if (changed == 0) need(anchorCleared == 0 && anchorDisposed == 0, "stable anchors retain their original mirrors and IDs");
        return ChunkLifecycleController.object("pressure_rounds", rounds, "control_mirrors_cleared", control.cleared(),
            "control_disposals_completed", COUNT, "control_ids_changed", controlChanged,
            "anchor_mirrors_cleared", anchorCleared, "anchor_disposals_completed", anchorDisposed,
            "anchor_ids_changed", changed, "before_ids", original.ids, "after_ids", current.ids,
            "completed_dispose_packets", trace.packets(), "stable", changed == 0);
    }
    static void identity(ChunkLifecycleController.Session session, BreakpointEvent event) throws Exception {
        session.stopped(event); // Actual code under test, with real event/frame/arguments.
    }
    @SuppressWarnings("unchecked")
    static boolean capacity(ChunkLifecycleController.Session session, ArrayReference anchors,
                            ReferenceType target) throws Exception {
        java.lang.reflect.Method id;
        java.lang.reflect.Field member;
        try {
            id = ChunkLifecycleController.Session.class.getDeclaredMethod("id", ObjectReference.class);
            member = ChunkLifecycleController.Session.class.getDeclaredField("identityMirrors");
        } catch (NoSuchMethodException | NoSuchFieldException absent) { return false; }
        id.setAccessible(true); member.setAccessible(true);
        Map<Long,ObjectReference> table = (Map<Long,ObjectReference>) member.get(session);
        Map<Long,ObjectReference> saved = new LinkedHashMap<>(table);
        ObjectReference known = (ObjectReference) anchors.getValue(0);
        ArrayReference pump = (ArrayReference) target.getValue(target.fieldByName("PUMP"));
        ObjectReference unseen = (ObjectReference) pump.getValue(3);
        long knownID = known.uniqueID(), unseenID = unseen.uniqueID();
        need(table.containsKey(knownID) && !table.containsKey(unseenID), "capacity fixture identities");
        try {
            for (long key = Long.MIN_VALUE; table.size() < 32768; key++) table.put(key, known);
            need(((Long) id.invoke(session, known)).longValue() == knownID && table.size() == 32768, "known ID allowed at capacity");
            boolean rejected = false;
            try { id.invoke(session, unseen); }
            catch (java.lang.reflect.InvocationTargetException error) {
                need(error.getCause() instanceof IllegalStateException && error.getCause().getMessage().contains("recorded JDI identity mirror bound"), "exact capacity rejection");
                rejected = true;
            }
            need(rejected && table.size() == 32768 && !table.containsKey(unseenID), "unseen ID rejected before insertion");
        } finally { table.clear(); table.putAll(saved); }
        return true;
    }
    static Map<String,Object> probe(VirtualMachine vm, String mode, TraceLog trace, long started) throws Exception {
        ChunkLifecycleController.Session session = new ChunkLifecycleController.Session(vm, "dedicated", "unload101");
        ClassPrepareRequest prepare = vm.eventRequestManager().createClassPrepareRequest();
        prepare.addClassFilter("ControllerMirrorIdentityTarget"); prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); prepare.enable();
        ReferenceType target = null;
        ArrayReference anchors = null, disposable = null;
        Probe first = null;
        Map<String,Object> evidence = null;
        int duplicateRejections = 0;
        while (true) {
            deadline(started);
            EventSet set = vm.eventQueue().remove(1000);
            if (set == null) continue;
            boolean finished = false;
            for (Event event : set) {
                if (event instanceof ClassPrepareEvent ready) {
                    target = ready.referenceType();
                    for (String name : List.of("ready", "identity", "done")) {
                        List<com.sun.jdi.Method> methods = target.methodsByName(name);
                        need(methods.size() == 1, "one synthetic marker method");
                        session.stop(methods.get(0), 0, name, EventRequest.SUSPEND_ALL);
                    }
                } else if (event instanceof BreakpointEvent stop) {
                    need(set.suspendPolicy() == EventRequest.SUSPEND_ALL, "all target threads suspended");
                    String role = (String) stop.request().getProperty("role");
                    if (role.equals("ready")) {
                        session.setupSeen = true; session.measuredThread = stop.thread();
                        anchors = (ArrayReference) target.getValue(target.fieldByName("ROOTS"));
                        disposable = (ArrayReference) target.getValue(target.fieldByName("CONTROLS"));
                        need(anchors.length() == COUNT && disposable.length() == COUNT, "exact static-root fixtures");
                        if (mode.equals("delayed")) {
                            first = roots(session, anchors);
                            evidence = exercise(session, target, anchors, first, disposable, trace, started);
                        }
                    } else if (role.equals("identity")) {
                        if (session.identityIDs.size() < COUNT) identity(session, stop);
                        else {
                            int key = ((IntegerValue) stop.thread().frame(0).getArgumentValues().get(1)).value();
                            need(key == (duplicateRejections == 0 ? COUNT + 1 : 1), "ordered duplicate object/key controls");
                            int count = session.identityMarkers;
                            boolean rejected = false;
                            try { identity(session, stop); }
                            catch (IllegalStateException expected) {
                                need(expected.getMessage().contains("bijective helper identity label"), "exact duplicate rejection"); rejected = true;
                            } finally { session.identityMarkers = count; }
                            need(rejected && session.identityIDs.size() == COUNT, "duplicate helper identity remains rejected");
                            duplicateRejections++;
                        }
                    }
                    else if (role.equals("done")) { finished = true; }
                    else throw new IllegalStateException("unknown synthetic role");
                } else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) {
                    throw new IllegalStateException("target exited before probe completed");
                }
            }
            if (finished) {
                need(session.identityMarkers == COUNT && session.identityIDs.size() == COUNT && duplicateRejections == 2, "all actual identity markers and duplicate controls");
                if (mode.equals("marker")) {
                    first = roots(session, anchors);
                    for (int i = 0; i < COUNT; i++) need(first.ids.get(i).equals(session.identityIDs.get(i + 1)), "initial marker/serialized ID agreement");
                    evidence = exercise(session, target, anchors, first, disposable, trace, started);
                }
                need(first != null && evidence != null, "complete actual serialization observation");
                int markerChanged = 0;
                for (int i = 0; i < COUNT; i++) if (!first.ids.get(i).equals(session.identityIDs.get(i + 1))) markerChanged++;
                if (mode.equals("delayed")) need(markerChanged == 0 || markerChanged == COUNT, "complete delayed identity outcome");
                boolean stable = Boolean.TRUE.equals(evidence.get("stable")) && markerChanged == 0;
                evidence.put("marker_ids_changed_from_first_observation", markerChanged);
                evidence.put("actual_identity_markers", session.identityMarkers);
                evidence.put("duplicate_identity_controls_rejected", duplicateRejections);
                boolean capacity = capacity(session, anchors, target);
                evidence.put("capacity_contract", capacity);
                need(!stable || capacity, "stable candidate includes bounded actual id contract");
                evidence.put("stable", stable);
                return evidence; // Keep SUSPEND_ALL; teardown exits the target.
            }
            set.resume();
        }
    }
    public static void main(String[] args) throws Exception {
        need(args.length == 3 && Set.of("marker", "delayed").contains(args[0]), "mode classpath output arguments");
        String mode = args[0]; Path output = Path.of(args[2]); Files.createDirectories(output);
        Map<String,Object> report = new LinkedHashMap<>();
        report.put("schema", 1); report.put("scope", "synthetic actual-controller JDI mirror lifetime"); report.put("mode", mode);
        report.put("status", "fail"); report.put("target_gc_disabled", false); report.put("target_methods_invoked_by_debugger", false);
        report.put("controller_java_version", System.getProperty("java.runtime.version"));
        VirtualMachine vm = null; Process process = null; Thread outThread = null, errThread = null;
        Drain stdout = null, stderr = null; PrintStream originalErr = System.err;
        long started = System.nanoTime(); int exit = 2;
        TraceLog trace = new TraceLog(output.resolve("jdi-trace.txt"));
        PrintStream traced = new PrintStream(trace, false, StandardCharsets.UTF_8);
        try {
            System.setErr(traced);
            LaunchingConnector launcher = Bootstrap.virtualMachineManager().defaultConnector();
            Map<String,Connector.Argument> options = launcher.defaultArguments();
            options.get("home").setValue(System.getProperty("java.home"));
            options.get("main").setValue("ControllerMirrorIdentityTarget");
            options.get("options").setValue("-Xint -Xmx64m -cp \"" + args[1].replace("\"", "\\\"") + "\"");
            options.get("suspend").setValue("true"); vm = launcher.launch(options); process = vm.process();
            report.put("target_pid", process.pid()); report.put("target_java_version", vm.version());
            stdout = new Drain(process.getInputStream(), output.resolve("target.stdout"));
            stderr = new Drain(process.getErrorStream(), output.resolve("target.stderr"));
            outThread = new Thread(stdout, "target-stdout"); errThread = new Thread(stderr, "target-stderr");
            outThread.setDaemon(true); errThread.setDaemon(true); outThread.start(); errThread.start();
            vm.setDebugTraceMode(VirtualMachine.TRACE_OBJREFS | VirtualMachine.TRACE_SENDS | VirtualMachine.TRACE_RECEIVES);
            Map<String,Object> evidence = probe(vm, mode, trace, started);
            report.put("evidence", evidence);
            exit = Boolean.TRUE.equals(evidence.get("stable")) ? 0 : 10;
            report.put("status", exit == 0 ? "pass" : "fail-stale-serialized-identity");
        } catch (Throwable error) {
            report.put("error", error.getClass().getName() + ": " + error.getMessage());
        } finally {
            List<String> cleanup = new ArrayList<>();
            if (vm != null) try { vm.exit(0); } catch (VMDisconnectedException ignored) { }
            catch (Throwable error) { cleanup.add("VM exit: " + error); }
            try {
                if (process != null) {
                    if (!process.waitFor(5, TimeUnit.SECONDS)) process.destroyForcibly();
                    need(process.waitFor(5, TimeUnit.SECONDS), "target process reaped");
                    report.put("target_exit", process.exitValue()); report.put("target_reaped", true);
                    need(process.exitValue() == 0, "successful target exit");
                }
                if (outThread != null) outThread.join(5000);
                if (errThread != null) errThread.join(5000);
                need((outThread == null || !outThread.isAlive()) && (errThread == null || !errThread.isAlive()), "target streams closed");
                need(stdout == null || !stdout.failed && !stderr.failed, "bounded target streams");
                need(!trace.failed, "complete bounded JDI trace");
            } catch (Throwable error) { cleanup.add("process/stream cleanup: " + error); }
            System.setErr(originalErr);
            try { traced.flush(); need(!traced.checkError() && !trace.failed, "complete flushed trace sink"); trace.close(); }
            catch (Throwable error) { cleanup.add("trace close: " + error); }
            if (!cleanup.isEmpty()) { report.put("cleanup_errors", cleanup); report.put("status", "fail-cleanup"); exit = 2; }
            report.put("seconds", (System.nanoTime() - started) / 1_000_000_000.0);
            report.put("exit_code", exit);
            Files.write(output.resolve("report.json"), ChunkLifecycleObservation.encoded(report));
        }
        System.out.println("mirror identity " + mode + ": " + report.get("status"));
        System.exit(exit);
    }
}

final class ControllerMirrorIdentityTarget {
    static final Object[] ROOTS = roots(), CONTROLS = controls();
    static final Object[] PUMP = {new Object(), new Object(), new Object(), new Object()};
    static final Object EXTRA = new Object();
    static Object[] roots() {
        Object[] values = new Object[ControllerMirrorIdentityRegression.COUNT];
        for (int i = 0; i < values.length; i++) values[i] = i % 2 == 0
            ? new byte[ControllerMirrorIdentityRegression.ARRAY_BYTES] : new ArrayList<>();
        return values;
    }
    static Object[] controls() {
        Object[] values = new Object[ControllerMirrorIdentityRegression.COUNT];
        for (int i = 0; i < values.length; i++) values[i] = new Object();
        return values;
    }
    static void ready() { }
    static void identity(Object value, int key) { if (value == null || key <= 0) throw new AssertionError(); }
    static void done() { }
    public static void main(String[] args) {
        ready();
        for (int i = 0; i < ROOTS.length; i++) identity(ROOTS[i], i + 1);
        identity(ROOTS[0], ROOTS.length + 1);
        identity(EXTRA, 1);
        done();
    }
}
