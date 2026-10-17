import com.sun.jdi.Bootstrap;
import com.sun.jdi.BooleanValue;
import com.sun.jdi.ClassLoaderReference;
import com.sun.jdi.Field;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.Location;
import com.sun.jdi.Method;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.StackFrame;
import com.sun.jdi.ThreadReference;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.BreakpointEvent;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.MethodExitEvent;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.request.BreakpointRequest;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import com.sun.jdi.request.EventRequestManager;
import com.sun.jdi.request.MethodExitRequest;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Reads actual entry arguments and returned values through a loopback debugger. */
public final class HeightTraceController {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final int CALLS_PER_PASS = 1024;
    private static final int MAX_THREADS = 64;
    private static final int MAX_FRAMES_PER_THREAD = 64;

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static Map<String, Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }

    private static String hash(byte[] value) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));
    }

    private static String json(Object value) throws Exception {
        java.lang.reflect.Method encoder = Class.forName("ChunkObservation").getDeclaredMethod("json", Object.class);
        require(encoder.getReturnType() == String.class && encoder.getModifiers() == 10, "shared JSON descriptor/modifiers");
        encoder.setAccessible(true);
        try { return (String) encoder.invoke(null, value); }
        catch (InvocationTargetException failure) { throw new IllegalStateException(failure.getCause()); }
    }

    private static long remainingMillis(long deadline) {
        long remaining = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(remaining > 0, "90-second controller bound");
        return Math.min(10000, remaining);
    }

    private static void newOutput(Path path) {
        require(!Files.exists(path, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(path)
                && Files.isDirectory(path.getParent()), "new output beneath existing parent required: " + path);
    }

    private static byte[] capture(InputStream source, CompletableFuture<String> startup, AtomicLong total) throws Exception {
        ByteArrayOutputStream result = new ByteArrayOutputStream();
        ByteArrayOutputStream firstLine = startup == null ? null : new ByteArrayOutputStream();
        byte[] block = new byte[8192];
        try {
            int count;
            while ((count = source.read(block)) >= 0) {
                require(total.addAndGet(count) <= MAX_BYTES, "combined child stdout/stderr budget");
                result.write(block, 0, count);
                if (firstLine != null) {
                    for (int i = 0; i < count; ++i) {
                        if (block[i] == '\n') {
                            String line = firstLine.toString(StandardCharsets.US_ASCII);
                            if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
                            startup.complete(line);
                            firstLine = null;
                            break;
                        }
                        require(firstLine.size() < 1024, "bounded JDWP startup line");
                        firstLine.write(block[i]);
                    }
                }
            }
            if (firstLine != null) startup.completeExceptionally(new IllegalStateException("missing JDWP startup line"));
            return result.toByteArray();
        } catch (Throwable failure) {
            if (startup != null) startup.completeExceptionally(failure);
            throw failure;
        }
    }

    private static Method oneMethod(ReferenceType type, String name, String signature, int modifiers) {
        List<Method> matches = type.methodsByName(name, signature);
        require(matches.size() == 1 && matches.get(0).declaringType().equals(type)
                && matches.get(0).modifiers() == modifiers, "exact method binding: " + type.name() + "." + name + signature);
        return matches.get(0);
    }

    private static int integer(Value value) {
        require(value instanceof IntegerValue, "integer JDI value");
        return ((IntegerValue) value).value();
    }

    private static final class Session {
        final VirtualMachine vm;
        final EventRequestManager manager;
        final Field daemonField, holderField;
        final String daemonLayout;
        final boolean client;
        final List<Object> passes = new ArrayList<>();
        final ClassPrepareRequest prepare, setupPrepare;
        BreakpointRequest spawnEntry, spawnRandomStep, heightEntry;
        MethodExitRequest heightExits;
        ObjectReference spawnWorld, world, chunk;
        ThreadReference spawnThread, thread;
        Method worldHeight, directCaller;
        Map<String, Object> identity;
        Map<String, Object> beginThreads;
        List<Object> records;
        int spawnEntries, randomWalkIterations, activePass = -1, ignoredEntries, pendingDepth;
        int beginMarkersSeen, endMarkersSeen;
        int[] pending;

        Session(VirtualMachine vm, boolean client) {
            this.vm = vm;
            this.client = client;
            manager = vm.eventRequestManager();
            List<ReferenceType> threadTypes = vm.classesByName("java.lang.Thread");
            require(threadTypes.size() == 1 && threadTypes.get(0).classLoader() == null, "bootstrap Java Thread class binding");
            Field directDaemon = threadTypes.get(0).fieldByName("daemon");
            if (directDaemon != null) {
                require(directDaemon.declaringType().equals(threadTypes.get(0))
                        && directDaemon.signature().equals("Z") && directDaemon.modifiers() == 2,
                        "direct Java Thread daemon field descriptor/modifiers");
                daemonField = directDaemon;
                holderField = null;
                daemonLayout = "thread-daemon";
            } else {
                holderField = threadTypes.get(0).fieldByName("holder");
                require(holderField != null && holderField.declaringType().equals(threadTypes.get(0))
                        && holderField.signature().equals("Ljava/lang/Thread$FieldHolder;") && holderField.modifiers() == 18,
                        "Java Thread holder field descriptor/modifiers");
                List<ReferenceType> holders = vm.classesByName("java.lang.Thread$FieldHolder");
                require(holders.size() == 1 && holders.get(0).classLoader() == null, "bootstrap Thread FieldHolder class binding");
                daemonField = holders.get(0).fieldByName("daemon");
                require(daemonField != null && daemonField.declaringType().equals(holders.get(0))
                        && daemonField.signature().equals("Z") && daemonField.modifiers() == 64,
                        "Thread FieldHolder daemon field descriptor/modifiers");
                daemonLayout = "thread-holder-daemon";
            }
            prepare = manager.createClassPrepareRequest();
            prepare.addClassFilter("HeightTraceObservation");
            prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);
            prepare.enable();
            if (!client) {
                setupPrepare = manager.createClassPrepareRequest();
                setupPrepare.addClassFilter("dj");
                setupPrepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                setupPrepare.enable();
            } else setupPrepare = null;
        }

        BreakpointRequest breakpoint(Location location, String marker, int policy) {
            BreakpointRequest request = manager.createBreakpointRequest(location);
            request.putProperty("marker", marker);
            request.setSuspendPolicy(policy);
            request.enable();
            return request;
        }

        void prepared(ClassPrepareEvent event) {
            ReferenceType type = event.referenceType();
            if (type.name().equals("dj")) {
                require(!client, "server setup trace only");
                Method spawn = oneMethod(type, "c", "()V", 4);
                byte[] code = spawn.bytecodes();
                require(code.length == 87 && (code[24] & 255) == 27
                        && (code[31] & 255) == 182 && (code[62] & 255) == 182,
                        "original server spawn random-step instruction binding");
                spawnEntry = breakpoint(spawn.location(), "spawn_entry", EventRequest.SUSPEND_EVENT_THREAD);
                Location randomStep = spawn.locationOfCodeIndex(24);
                require(randomStep != null && randomStep.codeIndex() == 24, "original spawn random-step location");
                spawnRandomStep = breakpoint(randomStep, "spawn_random_step", EventRequest.SUSPEND_EVENT_THREAD);
                setupPrepare.disable();
                return;
            }
            require(type.name().equals("HeightTraceObservation"), "known prepared helper class");
            breakpoint(oneMethod(type, "begin", "(Ljava/lang/Object;Ljava/lang/Object;I)V", 10).location(),
                    "begin", EventRequest.SUSPEND_ALL);
            breakpoint(oneMethod(type, "end", "(I)V", 10).location(), "end", EventRequest.SUSPEND_ALL);
            prepare.disable();
        }

        boolean direct(ThreadReference candidate) throws Exception {
            return candidate.frameCount() >= 2 && world.equals(candidate.frame(0).thisObject())
                    && directCaller.equals(candidate.frame(1).location().method()) && chunk.equals(candidate.frame(1).thisObject());
        }

        Map<String, Object> inventory(BreakpointEvent event, String marker, int pass) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL && event.thread().equals(thread),
                    "thread inventory requires matched SUSPEND_ALL marker");
            require(thread.name().equals("main"), "measured thread must be original main thread");
            List<ThreadReference> actualThreads = vm.allThreads();
            require(!actualThreads.isEmpty() && actualThreads.size() <= MAX_THREADS, "complete Java thread inventory budget");
            ClassLoaderReference originalLoader = world.referenceType().classLoader();
            List<Object> entries = new ArrayList<>();
            int measuredCount = 0;
            boolean allSuspended = true, allOtherDaemon = true, noOtherOriginalFrames = true;
            for (ThreadReference candidate : actualThreads) {
                boolean measured = candidate.equals(thread);
                if (measured) ++measuredCount;
                ObjectReference daemonOwner = candidate;
                if (holderField != null) {
                    Value holder = candidate.getValue(holderField);
                    require(holder instanceof ObjectReference, "inventoried thread must have verified platform Thread FieldHolder");
                    daemonOwner = (ObjectReference) holder;
                    require(daemonOwner.referenceType().equals(daemonField.declaringType()), "actual Thread FieldHolder instance binding");
                }
                Value daemonValue = daemonOwner.getValue(daemonField);
                require(daemonValue instanceof BooleanValue, "actual Java Thread daemon boolean value");
                boolean daemon = ((BooleanValue) daemonValue).value();
                boolean suspended = candidate.isSuspended();
                allSuspended &= suspended;
                require(suspended, "every inventoried Java thread must be suspended");
                int frameCount = candidate.frameCount();
                require(frameCount >= 0 && frameCount <= MAX_FRAMES_PER_THREAD, "complete per-thread stack frame budget");
                List<StackFrame> actualFrames = candidate.frames(0, frameCount);
                require(actualFrames.size() == frameCount, "complete suspended Java thread stack");
                List<Object> frames = new ArrayList<>();
                boolean originalFrame = false;
                for (StackFrame frame : actualFrames) {
                    Location location = frame.location();
                    ReferenceType owner = location.declaringType();
                    ClassLoaderReference frameLoader = owner.classLoader();
                    boolean original = originalLoader.equals(frameLoader);
                    originalFrame |= original;
                    frames.add(object("ordinal", frames.size(), "class", owner.name(), "method", location.method().name(),
                            "signature", location.method().signature(), "code_index", location.codeIndex(),
                            "class_loader_id", frameLoader == null ? null : frameLoader.uniqueID(),
                            "class_loader_class", frameLoader == null ? null : frameLoader.referenceType().name(),
                            "original_frame", original));
                }
                if (measured) require(!daemon, "measured main thread must be non-daemon");
                else {
                    allOtherDaemon &= daemon;
                    noOtherOriginalFrames &= !originalFrame;
                }
                entries.add(object("ordinal", entries.size(), "id", candidate.uniqueID(), "name", candidate.name(),
                        "object_class", candidate.referenceType().name(), "status", candidate.status(),
                        "daemon", daemon, "measured", measured, "suspended", suspended,
                        "frame_count", frameCount, "original_frame", originalFrame, "frames", frames));
            }
            require(measuredCount == 1 && allSuspended && allOtherDaemon && noOtherOriginalFrames,
                    "finite headless marker inventory requires only main plus unrelated daemon threads");
            return object("marker", marker, "pass", pass, "suspend_policy", EventRequest.SUSPEND_ALL,
                    "measured_thread_id", thread.uniqueID(), "original_loader_id", originalLoader.uniqueID(),
                    "thread_limit", MAX_THREADS, "frames_per_thread_limit", MAX_FRAMES_PER_THREAD,
                    "daemon_layout", daemonLayout,
                    "daemon_field", object("owner", daemonField.declaringType().name(), "signature", "Z", "modifiers", daemonField.modifiers()),
                    "holder_field", holderField == null ? null : object("owner", "java.lang.Thread",
                            "signature", holderField.signature(), "modifiers", holderField.modifiers()),
                    "thread_count", entries.size(), "measured_main_thread", measuredCount == 1,
                    "all_threads_suspended", allSuspended, "all_other_threads_daemon", allOtherDaemon,
                    "no_other_original_frames", noOtherOriginalFrames, "threads", entries);
        }

        void begin(BreakpointEvent event) throws Exception {
            List<Value> arguments = event.thread().frame(0).getArgumentValues();
            int pass = integer(arguments.get(2));
            require(activePass == -1 && pass == passes.size() && pass < 2, "ordered nonoverlapping measured passes");
            ObjectReference actualWorld = (ObjectReference) arguments.get(0);
            ObjectReference actualChunk = (ObjectReference) arguments.get(1);
            if (pass == 0) {
                world = actualWorld;
                chunk = actualChunk;
                thread = event.thread();
                if (!client) {
                    require(spawnEntries == 1 && randomWalkIterations == 0 && world.equals(spawnWorld)
                            && thread.equals(spawnThread), "genuine server setup used immediate origin without random walk");
                    spawnEntry.disable();
                    spawnRandomStep.disable();
                }
                ReferenceType worldType = world.referenceType(), chunkType = chunk.referenceType();
                require(worldType.name().equals(client ? "fd" : "dj") && chunkType.name().equals(client ? "lm" : "hi"),
                        "actual measured original class identities");
                require(worldType.classLoader() != null && worldType.classLoader().equals(chunkType.classLoader())
                        && worldType.classLoader().referenceType().name().equals("java.net.URLClassLoader"),
                        "same isolated original URL class loader");
                worldHeight = oneMethod(worldType, "d", "(II)I", 1);
                directCaller = oneMethod(chunkType, "f", "(III)V", 2);
                heightEntry = manager.createBreakpointRequest(worldHeight.location());
                heightEntry.addThreadFilter(thread);
                heightEntry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
                heightExits = manager.createMethodExitRequest();
                heightExits.addClassFilter(worldType);
                heightExits.addThreadFilter(thread);
                heightExits.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
                identity = object("thread_id", thread.uniqueID(), "thread_name", thread.name(),
                        "world_object_id", world.uniqueID(), "chunk_object_id", chunk.uniqueID(),
                        "world_class", worldType.name(), "chunk_class", chunkType.name(),
                        "world_loader_id", worldType.classLoader().uniqueID(), "chunk_loader_id", chunkType.classLoader().uniqueID());
            } else {
                require(world.equals(actualWorld) && chunk.equals(actualChunk) && thread.equals(event.thread()),
                        "repeat pass retains measured world/chunk/thread");
            }
            activePass = pass;
            beginThreads = inventory(event, "begin", pass);
            records = new ArrayList<>();
            ignoredEntries = 0;
            require(pending == null, "no pending call at pass entry");
            heightEntry.enable();
        }

        void stopped(BreakpointEvent event) throws Exception {
            Object marker = event.request().getProperty("marker");
            if ("spawn_entry".equals(marker)) {
                require(passes.isEmpty() && activePass == -1 && spawnEntries == 0, "single constructor spawn entry before measurement");
                ++spawnEntries;
                spawnWorld = event.thread().frame(0).thisObject();
                spawnThread = event.thread();
            } else if ("spawn_random_step".equals(marker)) {
                require(spawnEntries == 1 && event.thread().equals(spawnThread)
                        && event.thread().frame(0).thisObject().equals(spawnWorld), "matched spawn random-step receiver/thread");
                require(++randomWalkIterations <= 65536, "bounded setup random-step events");
            } else if ("begin".equals(marker)) {
                ++beginMarkersSeen;
                begin(event);
            } else if ("end".equals(marker)) {
                ++endMarkersSeen;
                int pass = integer(event.thread().frame(0).getArgumentValues().get(0));
                require(activePass == pass && pass == passes.size() && thread.equals(event.thread()), "matching pass end/thread");
                require(pending == null && records.size() == CALLS_PER_PASS,
                        "exact 1024 complete direct-call pairs per pass: " + records.size());
                Map<String, Object> endThreads = inventory(event, "end", pass);
                heightEntry.disable();
                heightExits.disable();
                passes.add(object("pass", pass, "identity", identity, "begin_marker", true, "end_marker", true,
                        "direct_call_count", records.size(), "ignored_entries", ignoredEntries, "records", records,
                        "begin_threads", beginThreads, "end_threads", endThreads));
                activePass = -1;
            } else if (event.request().equals(heightEntry)) {
                require(activePass >= 0 && activePass < 2 && thread.equals(event.thread()), "height entry inside measured pass/thread");
                if (!direct(event.thread())) {
                    require(++ignoredEntries <= 32768, "bounded unmatched height entries");
                    return;
                }
                require(pending == null && records.size() < CALLS_PER_PASS, "bounded nonnested measured height call");
                List<Value> arguments = event.thread().frame(0).getArgumentValues();
                List<Value> callerArguments = event.thread().frame(1).getArgumentValues();
                int x = integer(arguments.get(0)), z = integer(arguments.get(1));
                require(x == integer(callerArguments.get(0)) && z == integer(callerArguments.get(1)),
                        "direct original caller coordinates");
                pending = new int[] {records.size(), x, z, integer(callerArguments.get(2))};
                pendingDepth = event.thread().frameCount();
                heightExits.enable();
            } else throw new IllegalStateException("unknown breakpoint request");
        }

        void returned(MethodExitEvent event) throws Exception {
            if (!event.method().equals(worldHeight) || pending == null || event.thread().frameCount() != pendingDepth
                    || !direct(event.thread())) return;
            require(activePass >= 0 && thread.equals(event.thread()), "height exit inside measured pass/thread");
            List<Value> arguments = event.thread().frame(0).getArgumentValues();
            List<Value> callerArguments = event.thread().frame(1).getArgumentValues();
            require(integer(arguments.get(0)) == pending[1] && integer(arguments.get(1)) == pending[2]
                    && integer(callerArguments.get(2)) == pending[3], "entry/exit frame and caller arguments retained");
            records.add(object("ordinal", pending[0], "x", pending[1], "z", pending[2],
                    "local_height", pending[3], "result", integer(event.returnValue())));
            pending = null;
            heightExits.disable();
        }

        Map<String, Object> setupTrace() {
            return object("enabled", !client, "spawn_entries", spawnEntries, "random_walk_iterations", randomWalkIterations,
                    "random_step_code_index", 24, "same_world", client ? null : world.equals(spawnWorld),
                    "same_thread", client ? null : thread.equals(spawnThread));
        }

        Map<String, Object> measurementEvidence() {
            return object("begin_markers", beginMarkersSeen, "end_markers", endMarkersSeen,
                    "completed_passes", passes.size(), "active_pass", activePass,
                    "pending_call", pending != null, "record_count", records == null ? 0 : records.size(),
                    "height_requests_created", heightEntry != null || heightExits != null,
                    "measured_identity_created", world != null || chunk != null || thread != null || identity != null);
        }

        boolean beforeMeasurement() {
            return beginMarkersSeen == 0 && endMarkersSeen == 0 && passes.isEmpty() && activePass == -1
                    && pending == null && records == null && heightEntry == null && heightExits == null
                    && world == null && chunk == null && thread == null && identity == null;
        }
    }

    private static void saveLog(Path path, Future<byte[]> output) {
        if (path == null || output == null || Files.exists(path, LinkOption.NOFOLLOW_LINKS)) return;
        try { Files.write(path, output.get(2, TimeUnit.SECONDS), StandardOpenOption.CREATE_NEW); }
        catch (Exception failure) { System.err.println("Child log capture failed: " + failure); }
    }

    public static void main(String[] args) {
        Process process = null;
        VirtualMachine vm = null;
        Future<byte[]> stdout = null, stderr = null;
        Path stdoutPath = null, stderrPath = null;
        ExecutorService readers = Executors.newFixedThreadPool(2);
        long started = System.nanoTime();
        long deadline = started + TimeUnit.SECONDS.toNanos(90);
        int status = 1;
        try {
            require(args.length == 9,
                    "arguments: jar side new_world_root snapshot_json initial_file repeat_file chunk_x chunk_z trace_json");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            Path tracePath = Path.of(args[8]).toAbsolutePath().normalize();
            Path unavailablePath = Path.of(tracePath + ".unavailable.json");
            stdoutPath = Path.of(tracePath + ".child-stdout.txt");
            stderrPath = Path.of(tracePath + ".child-stderr.txt");
            newOutput(tracePath);
            newOutput(unavailablePath);
            newOutput(stdoutPath);
            newOutput(stderrPath);
            require(!tracePath.equals(Path.of(args[3]).toAbsolutePath().normalize()), "separate trace and snapshot outputs required");
            String jarHash = hash(Files.readAllBytes(Path.of(args[0]).toRealPath()));
            Path java = Path.of(System.getProperty("java.home"), "bin",
                    System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            List<String> launch = new ArrayList<>(Arrays.asList(java.toString(), "-Xmx512m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                    "-cp", System.getProperty("java.class.path"), "HeightTraceObservation"));
            launch.addAll(Arrays.asList(Arrays.copyOf(args, 8)));
            process = new ProcessBuilder(launch).start();
            Process child = process;
            AtomicLong capturedBytes = new AtomicLong();
            CompletableFuture<String> startup = new CompletableFuture<>();
            stdout = readers.submit(() -> capture(child.getInputStream(), startup, capturedBytes));
            stderr = readers.submit(() -> capture(child.getErrorStream(), null, capturedBytes));
            String listening = startup.get(remainingMillis(deadline), TimeUnit.MILLISECONDS);
            require(listening.matches("Listening for transport dt_socket at address: [0-9]+"), "loopback JDWP startup line: " + listening);
            String port = listening.substring(listening.lastIndexOf(' ') + 1);
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments();
            connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(port);
            connection.get("timeout").setValue(Long.toString(remainingMillis(deadline)));
            vm = connector.attach(connection);
            require(vm.canGetMethodReturnValues() && vm.canGetBytecodes(), "VM must expose actual return values and method bytecodes");
            String vmName = vm.name(), vmVersion = vm.version();
            Session session = new Session(vm, args[1].equals("client"));
            boolean alive = true;
            vm.resume();
            while (alive && System.nanoTime() < deadline) {
                EventSet events;
                try { events = vm.eventQueue().remove(Math.min(1000, remainingMillis(deadline))); }
                catch (VMDisconnectedException disconnected) { break; }
                if (events == null) continue;
                for (Event event : events) {
                    if (event instanceof ClassPrepareEvent) session.prepared((ClassPrepareEvent) event);
                    else if (event instanceof BreakpointEvent) session.stopped((BreakpointEvent) event);
                    else if (event instanceof MethodExitEvent) session.returned((MethodExitEvent) event);
                    else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) alive = false;
                }
                try { events.resume(); } catch (VMDisconnectedException disconnected) { alive = false; }
            }
            require(process.waitFor(remainingMillis(deadline), TimeUnit.MILLISECONDS), "debuggee exit bound");
            byte[] output = stdout.get(remainingMillis(deadline), TimeUnit.MILLISECONDS);
            byte[] errors = stderr.get(remainingMillis(deadline), TimeUnit.MILLISECONDS);
            Files.write(stdoutPath, output, StandardOpenOption.CREATE_NEW);
            Files.write(stderrPath, errors, StandardOpenOption.CREATE_NEW);
            if (process.exitValue() == 77) {
                Path snapshotPath = Path.of(args[3]).toAbsolutePath().normalize();
                require(session.beforeMeasurement() && !Files.exists(snapshotPath, LinkOption.NOFOLLOW_LINKS)
                        && !Files.isSymbolicLink(snapshotPath),
                        "child exit77 is valid only before all measurement evidence and without a snapshot: "
                                + json(session.measurementEvidence()));
                Map<String, Object> unavailable = object("schema", 2, "status", "unavailable", "side", args[1],
                        "stage", "original_setup", "child_exit", 77, "jar_sha256", jarHash,
                        "measurement_evidence", session.measurementEvidence(), "snapshot_exists", false,
                        "child_stdout", object("file", stdoutPath.getFileName().toString(), "bytes", output.length, "sha256", hash(output)),
                        "child_stderr", object("file", stderrPath.getFileName().toString(), "bytes", errors.length, "sha256", hash(errors)),
                        "elapsed_ms", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started));
                byte[] encoded = (json(unavailable) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "unavailable report output budget");
                Files.write(unavailablePath, encoded, StandardOpenOption.CREATE_NEW);
                System.err.println("UNAVAILABLE: original setup; child exit77 before measurement; " + unavailablePath.getFileName());
                status = 77;
            } else {
                require(process.exitValue() == 0, "debuggee exit " + process.exitValue() + ":\n" + new String(errors, StandardCharsets.UTF_8));
                require(session.passes.size() == 2 && session.activePass == -1 && session.pending == null,
                        "both full measured passes completed");
                require(Files.isRegularFile(Path.of(args[3]), LinkOption.NOFOLLOW_LINKS)
                        && Files.size(Path.of(args[3])) <= MAX_BYTES, "complete bounded snapshots saved");
                Map<String, Object> record = object("schema", 2, "side", args[1], "seed_hex", "0000000000000017",
                        "coordinates", Arrays.asList(Integer.parseInt(args[6]), Integer.parseInt(args[7])), "jar_sha256", jarHash,
                        "can_get_method_return_values", true, "can_get_bytecodes", true,
                        "vm_name", vmName, "vm_version", vmVersion, "listen_address", "127.0.0.1", "port", Integer.parseInt(port),
                        "setup_trace", session.setupTrace(), "passes", session.passes, "child_exit", process.exitValue(),
                        "child_stdout", object("file", stdoutPath.getFileName().toString(), "bytes", output.length, "sha256", hash(output)),
                        "child_stderr", object("file", stderrPath.getFileName().toString(), "bytes", errors.length, "sha256", hash(errors)),
                        "elapsed_ms", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started));
                byte[] encoded = (json(record) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "complete trace output budget");
                Files.write(tracePath, encoded, StandardOpenOption.CREATE_NEW);
                System.out.println(args[1] + " passes=2 calls=2048 elapsed_ms=" + record.get("elapsed_ms"));
                status = 0;
            }
        } catch (Throwable failure) {
            System.err.println("FAILURE: height trace controller");
            failure.printStackTrace(System.err);
        } finally {
            if (vm != null) try { vm.dispose(); } catch (RuntimeException ignored) {}
            if (process != null && process.isAlive()) {
                process.destroyForcibly();
                try { process.waitFor(2, TimeUnit.SECONDS); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            }
            saveLog(stdoutPath, stdout);
            saveLog(stderrPath, stderr);
            readers.shutdownNow();
        }
        System.exit(status);
    }
}
