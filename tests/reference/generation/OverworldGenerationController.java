import com.sun.jdi.Bootstrap;
import com.sun.jdi.BooleanValue;
import com.sun.jdi.ClassLoaderReference;
import com.sun.jdi.Field;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.Location;
import com.sun.jdi.LongValue;
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
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Observes actual original stages and all finishing height returns through JDI. */
public final class OverworldGenerationController {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final int CALLS = 9;
    private static final int QUERIES = 1024;
    private static final int MAX_THREADS = 64;
    private static final int MAX_FRAMES = 64;
    private static final int[][] COORDINATES = {{1, 0}, {0, 0}, {1, 0}, {-1, -1}, {32768, 0},
            {2000000, 0}, {-2000000, 0}, {Integer.MIN_VALUE + 8, 0}, {Integer.MAX_VALUE - 9, 0}};
    private static final String[] STAGE_KINDS = {"generator", "generator", "constructor", "generator", "generator",
            "generator", "base", "base", "generator", "surface", "surface", "generator", "generator",
            "finish", "finish", "generator", "generator"};
    private static final int[] STAGE_INDICES = {0, 20, 52, 41, 71, 83, 0, 637, 96, 0, 604, 107, 122, 0, 221, 127, 129};

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private static Map<String, Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }
    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    private static String json(Object value) throws Exception {
        java.lang.reflect.Method encode = Class.forName("ChunkObservation").getDeclaredMethod("json", Object.class);
        encode.setAccessible(true); return (String) encode.invoke(null, value);
    }
    private static long remaining(long deadline) {
        long result = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(result > 0, "180-second debuggee/controller deadline"); return Math.min(result, 10000);
    }
    private static void newOutput(Path path) {
        require(!Files.exists(path, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(path)
                && Files.isDirectory(path.getParent()), "new output beneath existing parent: " + path);
    }
    private static byte[] capture(InputStream source, CompletableFuture<String> startup, AtomicLong total) throws Exception {
        ByteArrayOutputStream result = new ByteArrayOutputStream(), first = startup == null ? null : new ByteArrayOutputStream();
        byte[] block = new byte[8192];
        try {
            for (int count; (count = source.read(block)) >= 0;) {
                require(total.addAndGet(count) <= MAX_BYTES, "combined child stdout/stderr bound");
                result.write(block, 0, count);
                if (first != null) for (int i = 0; i < count; ++i) {
                    if (block[i] == '\n') {
                        String line = first.toString(StandardCharsets.US_ASCII);
                        if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
                        startup.complete(line); first = null; break;
                    }
                    require(first.size() < 1024, "bounded JDWP startup line"); first.write(block[i]);
                }
            }
            if (first != null) startup.completeExceptionally(new IllegalStateException("missing JDWP startup line"));
            return result.toByteArray();
        } catch (Throwable failure) {
            if (startup != null) startup.completeExceptionally(failure); throw failure;
        }
    }
    private static Method method(ReferenceType owner, String name, String signature, int modifiers) {
        List<Method> methods = owner.methodsByName(name, signature);
        require(methods.size() == 1 && methods.get(0).declaringType().equals(owner) && methods.get(0).modifiers() == modifiers,
                "exact original method " + owner.name() + "." + name + signature);
        return methods.get(0);
    }
    private static Field field(ReferenceType owner, String name, String signature, int modifiers) {
        Field result = owner.fieldByName(name);
        require(result != null && result.declaringType().equals(owner) && result.signature().equals(signature)
                && result.modifiers() == modifiers, "exact field " + owner.name() + "." + name);
        return result;
    }
    private static int integer(Value value) {
        require(value instanceof IntegerValue, "actual integer JDI value"); return ((IntegerValue) value).value();
    }
    private static ObjectReference reference(Value value) {
        require(value instanceof ObjectReference, "actual nonnull JDI object"); return (ObjectReference) value;
    }
    private static long randomSeed(ObjectReference random) {
        ReferenceType owner = random.referenceType();
        require(owner.name().equals("java.util.Random") && owner.classLoader() == null, "bootstrap java.util.Random");
        ObjectReference seed = reference(random.getValue(field(owner, "seed", "Ljava/util/concurrent/atomic/AtomicLong;", 18)));
        require(seed.referenceType().name().equals("java.util.concurrent.atomic.AtomicLong") && seed.referenceType().classLoader() == null,
                "bootstrap atomic seed owner");
        Value value = seed.getValue(field(seed.referenceType(), "value", "J", 66));
        require(value instanceof LongValue, "actual long seed"); long result = ((LongValue) value).value();
        require(result >= 0 && result < (1L << 48), "actual 48-bit Java Random state"); return result;
    }

    private static final class Session {
        final VirtualMachine vm;
        final EventRequestManager manager;
        final boolean client;
        final Field daemonField, holderField;
        final String daemonLayout;
        final List<Object> calls = new ArrayList<>();
        final Set<Long> chunks = new HashSet<>(), blocks = new HashSet<>();
        final ClassPrepareRequest helperPrepare, spawnPrepare;
        final List<BreakpointRequest> stageRequests = new ArrayList<>();
        BreakpointRequest spawnEntry, spawnStep, heightEntry;
        MethodExitRequest heightExits;
        ObjectReference spawnWorld, world, generator, chunk;
        ThreadReference spawnThread, thread;
        ReferenceType chunkType;
        Method worldHeight, directCaller;
        Field generatorRandom, caveField, caveRandom;
        int spawnEntries, randomSteps, active = -1, pendingDepth, ignoredEntries;
        int beginMarkersSeen, endMarkersSeen, stageMarkersSeen, heightEntriesSeen, heightReturnsSeen;
        boolean started, finishing;
        int[] pending;
        List<Object> stages, queries;
        Map<String, Object> beginThreads, constructorRandom;

        Session(VirtualMachine vm, boolean client) {
            this.vm = vm; this.client = client; manager = vm.eventRequestManager();
            List<ReferenceType> threads = vm.classesByName("java.lang.Thread");
            require(threads.size() == 1 && threads.get(0).classLoader() == null, "bootstrap Thread binding");
            Field direct = threads.get(0).fieldByName("daemon");
            if (direct != null) {
                daemonField = field(threads.get(0), "daemon", "Z", 2); holderField = null; daemonLayout = "thread-daemon";
            } else {
                holderField = field(threads.get(0), "holder", "Ljava/lang/Thread$FieldHolder;", 18);
                List<ReferenceType> holders = vm.classesByName("java.lang.Thread$FieldHolder");
                require(holders.size() == 1 && holders.get(0).classLoader() == null, "bootstrap Thread FieldHolder binding");
                daemonField = field(holders.get(0), "daemon", "Z", 64); daemonLayout = "thread-holder-daemon";
            }
            helperPrepare = manager.createClassPrepareRequest(); helperPrepare.addClassFilter("OverworldGenerationObservation");
            helperPrepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); helperPrepare.enable();
            if (!client) {
                spawnPrepare = manager.createClassPrepareRequest(); spawnPrepare.addClassFilter("dj");
                spawnPrepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); spawnPrepare.enable();
            } else spawnPrepare = null;
        }
        BreakpointRequest breakpoint(Method method, long index, String marker, int policy, boolean threadFiltered) {
            Location location = method.locationOfCodeIndex(index);
            require(location != null && location.codeIndex() == index, "exact original instruction location");
            BreakpointRequest result = manager.createBreakpointRequest(location); result.putProperty("marker", marker);
            if (threadFiltered) result.addThreadFilter(thread);
            result.setSuspendPolicy(policy); result.enable(); return result;
        }
        void prepared(ClassPrepareEvent event) {
            ReferenceType owner = event.referenceType();
            if (owner.name().equals("dj")) {
                require(!client, "server spawn only"); Method spawn = method(owner, "c", "()V", 4);
                byte[] code = spawn.bytecodes();
                require(code.length == 87 && (code[24] & 255) == 27 && (code[31] & 255) == 182 && (code[62] & 255) == 182,
                        "original spawn instruction binding");
                spawnEntry = breakpoint(spawn, 0, "spawn_entry", EventRequest.SUSPEND_EVENT_THREAD, false);
                spawnStep = breakpoint(spawn, 24, "spawn_step", EventRequest.SUSPEND_EVENT_THREAD, false); spawnPrepare.disable();
            } else {
                require(owner.name().equals("OverworldGenerationObservation"), "known helper class");
                breakpoint(method(owner, "begin", "(Ljava/lang/Object;Ljava/lang/Object;I)V", 10), 0, "begin", EventRequest.SUSPEND_ALL, false);
                breakpoint(method(owner, "end", "(Ljava/lang/Object;I)V", 10), 0, "end", EventRequest.SUSPEND_ALL, false);
                helperPrepare.disable();
            }
        }
        ReferenceType originalType(String name, ClassLoaderReference loader) {
            List<ReferenceType> found = vm.classesByName(name);
            require(found.size() == 1 && loader.equals(found.get(0).classLoader()), "same original loader class " + name);
            return found.get(0);
        }
        Map<String, Object> seeds() {
            ObjectReference actualRandom = reference(generator.getValue(generatorRandom));
            ObjectReference cave = reference(generator.getValue(caveField));
            return object("generator_seed", randomSeed(actualRandom), "cave_seed", randomSeed(reference(cave.getValue(caveRandom))));
        }
        void install() {
            ReferenceType worldType = world.referenceType(), generatorType = generator.referenceType();
            require(worldType.name().equals(client ? "fd" : "dj") && generatorType.name().equals(client ? "yf" : "pk"),
                    "actual original world/generator classes");
            ClassLoaderReference loader = worldType.classLoader();
            require(loader != null && loader.equals(generatorType.classLoader()) && loader.referenceType().name().equals("java.net.URLClassLoader"),
                    "same isolated original URL loader");
            chunkType = originalType(client ? "lm" : "hi", loader);
            ReferenceType caveBase = originalType(client ? "fv" : "dy", loader);
            generatorRandom = field(generatorType, "j", "Ljava/util/Random;", 2);
            caveField = field(generatorType, "u", client ? "Lfv;" : "Ldy;", 2);
            caveRandom = field(caveBase, "b", "Ljava/util/Random;", 4);
            require(reference(generator.getValue(field(generatorType, "p", client ? "Lfd;" : "Ldj;", 2))).equals(world),
                    "fresh generator attached to measured world");
            Method generate = method(generatorType, "b", client ? "(II)Llm;" : "(II)Lhi;", 1);
            require(generate.bytecodes().length == 130 && (generate.bytecodes()[17] & 255) == 182
                    && (generate.bytecodes()[129] & 255) == 176, "original complete generation entry bytecodes");
            for (int index : new int[] {0, 20, 41, 71, 83, 96, 107, 122, 127, 129})
                stageRequests.add(breakpoint(generate, index, "generator", EventRequest.SUSPEND_EVENT_THREAD, true));
            Method constructor = method(chunkType, "<init>", client ? "(Lfd;[BII)V" : "(Ldj;[BII)V", 1);
            require(constructor.bytecodes().length == 53, "original retained-block constructor body");
            stageRequests.add(breakpoint(constructor, 52, "constructor", EventRequest.SUSPEND_EVENT_THREAD, true));
            Method base = method(generatorType, "a", client ? "(II[B[Lkd;[D)V" : "(II[B[Lgs;[D)V", 1);
            require(base.bytecodes().length == 638, "original base body");
            stageRequests.add(breakpoint(base, 0, "base", EventRequest.SUSPEND_EVENT_THREAD, true));
            stageRequests.add(breakpoint(base, 637, "base", EventRequest.SUSPEND_EVENT_THREAD, true));
            Method surface = method(generatorType, "a", client ? "(II[B[Lkd;)V" : "(II[B[Lgs;)V", 1);
            require(surface.bytecodes().length == 605, "original surface body");
            stageRequests.add(breakpoint(surface, 0, "surface", EventRequest.SUSPEND_EVENT_THREAD, true));
            stageRequests.add(breakpoint(surface, 604, "surface", EventRequest.SUSPEND_EVENT_THREAD, true));
            Method finish = method(chunkType, client ? "c" : "b", "()V", 1);
            require(finish.bytecodes().length == 222, "original full finish body");
            stageRequests.add(breakpoint(finish, 0, "finish", EventRequest.SUSPEND_EVENT_THREAD, true));
            stageRequests.add(breakpoint(finish, 221, "finish", EventRequest.SUSPEND_EVENT_THREAD, true));
            worldHeight = method(worldType, "d", "(II)I", 1); directCaller = method(chunkType, "f", "(III)V", 2);
            require(worldHeight.bytecodes().length == 65 && directCaller.bytecodes().length == 71, "original height/caller bodies");
            heightEntry = manager.createBreakpointRequest(worldHeight.location()); heightEntry.addThreadFilter(thread);
            heightEntry.putProperty("marker", "height");
            heightEntry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
            heightExits = manager.createMethodExitRequest(); heightExits.addClassFilter(worldType); heightExits.addThreadFilter(thread);
            heightExits.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD); constructorRandom = seeds();
        }
        Map<String, Object> inventory(BreakpointEvent event, String marker, int index) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL && event.thread().equals(thread)
                    && thread.name().equals("main"), "complete inventory at matched suspended main marker");
            List<ThreadReference> actualThreads = vm.allThreads();
            require(!actualThreads.isEmpty() && actualThreads.size() <= MAX_THREADS, "complete Java thread inventory budget");
            ClassLoaderReference loader = world.referenceType().classLoader(); List<Object> entries = new ArrayList<>();
            int mainCount = 0; boolean othersDaemon = true, othersUnrelated = true;
            for (ThreadReference candidate : actualThreads) {
                boolean measured = candidate.equals(thread); if (measured) ++mainCount;
                require(candidate.isSuspended(), "all inventoried threads suspended");
                ObjectReference daemonOwner = candidate;
                if (holderField != null) {
                    daemonOwner = reference(candidate.getValue(holderField));
                    require(daemonOwner.referenceType().equals(daemonField.declaringType()), "actual platform FieldHolder identity");
                }
                Value daemonValue = daemonOwner.getValue(daemonField);
                require(daemonValue instanceof BooleanValue, "actual daemon boolean");
                boolean daemon = ((BooleanValue) daemonValue).value();
                int count = candidate.frameCount(); require(count >= 0 && count <= MAX_FRAMES, "complete thread stack budget");
                List<StackFrame> actualFrames = candidate.frames(0, count); require(actualFrames.size() == count, "complete thread frames");
                List<Object> frames = new ArrayList<>(); boolean original = false;
                for (StackFrame frame : actualFrames) {
                    Location location = frame.location(); ReferenceType owner = location.declaringType();
                    ClassLoaderReference frameLoader = owner.classLoader(); boolean fromOriginal = loader.equals(frameLoader); original |= fromOriginal;
                    frames.add(object("ordinal", frames.size(), "class", owner.name(), "method", location.method().name(),
                            "signature", location.method().signature(), "code_index", location.codeIndex(),
                            "class_loader_id", frameLoader == null ? null : frameLoader.uniqueID(),
                            "class_loader_class", frameLoader == null ? null : frameLoader.referenceType().name(), "original_frame", fromOriginal));
                }
                if (measured) require(!daemon, "measured main is non-daemon");
                else { othersDaemon &= daemon; othersUnrelated &= !original; }
                entries.add(object("ordinal", entries.size(), "id", candidate.uniqueID(), "name", candidate.name(),
                        "object_class", candidate.referenceType().name(), "status", candidate.status(), "daemon", daemon,
                        "measured", measured, "suspended", true, "frame_count", count, "original_frame", original, "frames", frames));
            }
            require(mainCount == 1 && othersDaemon && othersUnrelated, "finite marker main/daemon/original-frame inventory");
            return object("marker", marker, "index", index, "suspend_policy", EventRequest.SUSPEND_ALL,
                    "measured_thread_id", thread.uniqueID(), "original_loader_id", loader.uniqueID(), "thread_limit", MAX_THREADS,
                    "frames_per_thread_limit", MAX_FRAMES, "daemon_layout", daemonLayout,
                    "daemon_field", object("owner", daemonField.declaringType().name(), "signature", "Z", "modifiers", daemonField.modifiers()),
                    "holder_field", holderField == null ? null : object("owner", "java.lang.Thread", "signature", holderField.signature(), "modifiers", holderField.modifiers()),
                    "thread_count", entries.size(), "measured_main_thread", true, "all_threads_suspended", true,
                    "all_other_threads_daemon", othersDaemon, "no_other_original_frames", othersUnrelated, "threads", entries);
        }
        boolean direct(ThreadReference actual) throws Exception {
            return actual.frameCount() >= 2 && world.equals(actual.frame(0).thisObject())
                    && directCaller.equals(actual.frame(1).location().method()) && chunk.equals(actual.frame(1).thisObject());
        }
        void begin(BreakpointEvent event) throws Exception {
            List<Value> arguments = event.thread().frame(0).getArgumentValues(); int index = integer(arguments.get(2));
            require(active == -1 && index == calls.size() && index < CALLS, "ordered complete generation call markers");
            ObjectReference actualWorld = reference(arguments.get(0)), actualGenerator = reference(arguments.get(1));
            if (index == 0) {
                world = actualWorld; generator = actualGenerator; thread = event.thread();
                if (!client) {
                    require(spawnEntries == 1 && randomSteps == 0 && world.equals(spawnWorld) && thread.equals(spawnThread),
                            "qualifying genuine immediate-origin server setup"); spawnEntry.disable(); spawnStep.disable();
                }
                install();
            } else require(world.equals(actualWorld) && generator.equals(actualGenerator) && thread.equals(event.thread()), "retained generation context");
            active = index; started = true; chunk = null; finishing = false; pending = null;
            stages = new ArrayList<>(); queries = new ArrayList<>(); ignoredEntries = 0;
            beginThreads = inventory(event, "begin", index);
        }
        void stage(BreakpointEvent event, String kind) throws Exception {
            require(active >= 0 && event.thread().equals(thread), "original stage inside measured main call");
            int position = stages.size(); long index = event.location().codeIndex();
            require(position < STAGE_INDICES.length && STAGE_KINDS[position].equals(kind) && STAGE_INDICES[position] == index,
                    "exact 17 original generation stages: " + kind + "@" + index + " ordinal " + position);
            StackFrame frame = thread.frame(0); ObjectReference receiver = frame.thisObject();
            if (kind.equals("constructor")) {
                require(receiver.referenceType().equals(chunkType) && chunks.add(receiver.uniqueID()), "fresh exact original chunk identity");
                require(reference(receiver.getValue(field(chunkType, "d", client ? "Lfd;" : "Ldj;", 1))).equals(world), "returned chunk's original world");
                int x = integer(receiver.getValue(field(chunkType, "j", "I", 17))), z = integer(receiver.getValue(field(chunkType, "k", "I", 17)));
                require(x == COORDINATES[active][0] && z == COORDINATES[active][1], "actual constructor coordinates");
                require(blocks.add(reference(receiver.getValue(field(chunkType, "b", "[B", 1))).uniqueID()), "fresh returned block identity"); chunk = receiver;
            } else if (kind.equals("finish")) {
                require(receiver.equals(chunk), "finish same newly constructed chunk");
                if (index == 0) { require(!finishing && pending == null && queries.isEmpty(), "single finish entry"); finishing = true; heightEntry.enable(); }
                else {
                    require(finishing && pending == null && queries.size() == QUERIES, "complete 1024 query pairs at finish return");
                    finishing = false; heightEntry.disable(); heightExits.disable();
                }
            } else {
                require(receiver.equals(generator), "stage receiver is fresh selected generator");
                List<Value> arguments = frame.getArgumentValues();
                require(integer(arguments.get(0)) == COORDINATES[active][0] && integer(arguments.get(1)) == COORDINATES[active][1], "actual generation-stage coordinates");
            }
            Map<String, Object> random = seeds();
            stages.add(object("ordinal", position, "kind", kind, "class", event.location().declaringType().name(),
                    "method", event.location().method().name(), "signature", event.location().method().signature(),
                    "code_index", index, "generator_seed", random.get("generator_seed"), "cave_seed", random.get("cave_seed"),
                    "chunk_object_id", chunk == null ? null : chunk.uniqueID()));
        }
        void stopped(BreakpointEvent event) throws Exception {
            String marker = (String) event.request().getProperty("marker");
            if (marker.equals("spawn_entry")) {
                require(!started && spawnEntries == 0, "one original constructor spawn entry"); ++spawnEntries;
                spawnWorld = event.thread().frame(0).thisObject(); spawnThread = event.thread();
            } else if (marker.equals("spawn_step")) {
                require(!started && spawnEntries == 1 && spawnWorld.equals(event.thread().frame(0).thisObject())
                        && spawnThread.equals(event.thread()), "matched original spawn-step context");
                require(++randomSteps <= 65536, "bounded original spawn-step observations");
            } else if (marker.equals("begin")) { ++beginMarkersSeen; begin(event); }
            else if (marker.equals("end")) {
                ++endMarkersSeen;
                List<Value> arguments = event.thread().frame(0).getArgumentValues(); int index = integer(arguments.get(1));
                require(index == active && active == calls.size() && event.thread().equals(thread) && reference(arguments.get(0)).equals(chunk)
                        && stages.size() == 17 && queries.size() == QUERIES && pending == null && !finishing && ignoredEntries == 0,
                        "complete matching actual returned chunk/call/trace");
                Map<String, Object> identity = object("thread_id", thread.uniqueID(), "thread_name", thread.name(),
                        "world_object_id", world.uniqueID(), "generator_object_id", generator.uniqueID(), "chunk_object_id", chunk.uniqueID(),
                        "world_class", world.referenceType().name(), "generator_class", generator.referenceType().name(), "chunk_class", chunkType.name(),
                        "world_loader_id", world.referenceType().classLoader().uniqueID(), "generator_loader_id", generator.referenceType().classLoader().uniqueID());
                calls.add(object("index", active, "coordinates", Arrays.asList(COORDINATES[active][0], COORDINATES[active][1]),
                        "identity", identity, "begin_marker", true, "end_marker", true, "stages", stages,
                        "direct_call_count", queries.size(), "ignored_entries", ignoredEntries, "records", queries,
                        "begin_threads", beginThreads, "end_threads", inventory(event, "end", active)));
                active = -1;
            } else if (event.request().equals(heightEntry)) {
                ++heightEntriesSeen;
                require(active >= 0 && finishing && event.thread().equals(thread), "height read inside original full finishing");
                if (!direct(thread)) { ++ignoredEntries; throw new IllegalStateException("unexpected original height entry"); }
                require(pending == null && queries.size() < QUERIES, "bounded nonnested height call");
                List<Value> arguments = thread.frame(0).getArgumentValues(), caller = thread.frame(1).getArgumentValues();
                int x = integer(arguments.get(0)), z = integer(arguments.get(1));
                require(x == integer(caller.get(0)) && z == integer(caller.get(1)), "direct original height arguments");
                pending = new int[] {queries.size(), x, z, integer(caller.get(2))}; pendingDepth = thread.frameCount(); heightExits.enable();
            } else { ++stageMarkersSeen; stage(event, marker); }
        }
        void returned(MethodExitEvent event) throws Exception {
            if (!event.method().equals(worldHeight)) return;
            ++heightReturnsSeen;
            require(pending != null && active >= 0 && finishing && event.thread().equals(thread)
                    && thread.frameCount() == pendingDepth && direct(thread), "paired original height return frame");
            List<Value> arguments = thread.frame(0).getArgumentValues(), caller = thread.frame(1).getArgumentValues();
            require(integer(arguments.get(0)) == pending[1] && integer(arguments.get(1)) == pending[2]
                    && integer(caller.get(2)) == pending[3], "retained original height arguments");
            queries.add(object("ordinal", pending[0], "x", pending[1], "z", pending[2], "local_height", pending[3], "result", integer(event.returnValue())));
            pending = null; heightExits.disable();
        }
        Map<String, Object> setupTrace() {
            return object("enabled", !client, "spawn_entries", spawnEntries, "random_walk_iterations", randomSteps,
                    "random_step_code_index", 24, "same_world", client ? null : world.equals(spawnWorld), "same_thread", client ? null : thread.equals(spawnThread));
        }
        boolean measuredIdentityCreated() {
            return world != null || generator != null || chunk != null || thread != null || !chunks.isEmpty() || !blocks.isEmpty();
        }
        boolean measuredBindingCreated() {
            return chunkType != null || worldHeight != null || directCaller != null
                    || generatorRandom != null || caveField != null || caveRandom != null;
        }
        Map<String, Object> measurementEvidence() {
            return object("begin_markers", beginMarkersSeen, "end_markers", endMarkersSeen,
                    "stage_markers", stageMarkersSeen, "height_entries", heightEntriesSeen, "height_returns", heightReturnsSeen,
                    "completed_calls", calls.size(), "active_call", active, "started", started, "finishing", finishing,
                    "pending_call", pending != null, "pending_depth", pendingDepth, "ignored_entries", ignoredEntries,
                    "stage_record_count", stages == null ? 0 : stages.size(), "height_record_count", queries == null ? 0 : queries.size(),
                    "record_lists_created", stages != null || queries != null, "chunk_identities", chunks.size(), "block_identities", blocks.size(),
                    "stage_requests_created", stageRequests.size(), "height_requests_created", heightEntry != null || heightExits != null,
                    "measured_identity_created", measuredIdentityCreated(), "measured_binding_created", measuredBindingCreated(),
                    "marker_inventory_created", beginThreads != null, "constructor_random_created", constructorRandom != null);
        }
        boolean beforeMeasurement() {
            return beginMarkersSeen == 0 && endMarkersSeen == 0 && stageMarkersSeen == 0
                    && heightEntriesSeen == 0 && heightReturnsSeen == 0 && calls.isEmpty() && active == -1 && !started && !finishing
                    && pending == null && pendingDepth == 0 && ignoredEntries == 0 && stages == null && queries == null
                    && stageRequests.isEmpty() && heightEntry == null && heightExits == null
                    && !measuredIdentityCreated() && !measuredBindingCreated() && beginThreads == null && constructorRandom == null;
        }
    }

    private static void saveLog(Path path, Future<byte[]> bytes) {
        if (path == null || bytes == null || Files.exists(path, LinkOption.NOFOLLOW_LINKS)) return;
        try { Files.write(path, bytes.get(2, TimeUnit.SECONDS), StandardOpenOption.CREATE_NEW); }
        catch (Exception failure) { System.err.println("Child log capture failed: " + failure); }
    }
    public static void main(String[] args) {
        Process process = null; VirtualMachine vm = null; Future<byte[]> stdout = null, stderr = null;
        Path stdoutPath = null, stderrPath = null;
        ExecutorService readers = Executors.newFixedThreadPool(2); long started = System.nanoTime();
        long deadline = started + TimeUnit.SECONDS.toNanos(180); int status = 1;
        try {
            require(args.length == 6, "arguments: jar side seed_hex new_world_root snapshots_json trace_json");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            require(args[2].matches("[0-9a-f]{16}"), "seed hex"); long seed = Long.parseUnsignedLong(args[2], 16);
            require(seed == 23 || seed == 24 || seed == 26 || seed == 51, "qualifying fixed seed");
            Path tracePath = Path.of(args[5]).toAbsolutePath().normalize(); stdoutPath = Path.of(tracePath + ".child-stdout.txt"); stderrPath = Path.of(tracePath + ".child-stderr.txt");
            Path unavailablePath = Path.of(tracePath + ".unavailable.json");
            newOutput(tracePath); newOutput(unavailablePath); newOutput(stdoutPath); newOutput(stderrPath);
            require(!tracePath.equals(Path.of(args[4]).toAbsolutePath().normalize()), "separate trace/snapshot files");
            Path jar = Path.of(args[0]).toRealPath(); String jarHash = hash(Files.readAllBytes(jar));
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(jarHash.equals(expected), "pinned original archive");
            Path java = Path.of(System.getProperty("java.home"), "bin", System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            List<String> command = new ArrayList<>(Arrays.asList(java.toString(), "-Xmx512m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", System.getProperty("java.class.path"), "OverworldGenerationObservation"));
            command.addAll(Arrays.asList(Arrays.copyOf(args, 5))); process = new ProcessBuilder(command).start(); Process child = process;
            AtomicLong captured = new AtomicLong(); CompletableFuture<String> startup = new CompletableFuture<>();
            stdout = readers.submit(() -> capture(child.getInputStream(), startup, captured)); stderr = readers.submit(() -> capture(child.getErrorStream(), null, captured));
            String listening = startup.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(listening.matches("Listening for transport dt_socket at address: [0-9]+"), "loopback JDWP startup");
            String port = listening.substring(listening.lastIndexOf(' ') + 1);
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments(); connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(port); connection.get("timeout").setValue(Long.toString(remaining(deadline))); vm = connector.attach(connection);
            require(vm.canGetMethodReturnValues() && vm.canGetBytecodes(), "actual return values and bytecodes required");
            String vmName = vm.name(), vmVersion = vm.version(); Session session = new Session(vm, client); boolean alive = true; vm.resume();
            while (alive && System.nanoTime() < deadline) {
                EventSet set;
                try { set = vm.eventQueue().remove(Math.min(1000, remaining(deadline))); } catch (VMDisconnectedException closed) { break; }
                if (set == null) continue;
                for (Event event : set) {
                    if (event instanceof ClassPrepareEvent) session.prepared((ClassPrepareEvent) event);
                    else if (event instanceof BreakpointEvent) session.stopped((BreakpointEvent) event);
                    else if (event instanceof MethodExitEvent) session.returned((MethodExitEvent) event);
                    else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) alive = false;
                }
                try { set.resume(); } catch (VMDisconnectedException closed) { alive = false; }
            }
            require(process.waitFor(remaining(deadline), TimeUnit.MILLISECONDS), "debuggee exit deadline");
            byte[] out = stdout.get(remaining(deadline), TimeUnit.MILLISECONDS), err = stderr.get(remaining(deadline), TimeUnit.MILLISECONDS);
            Files.write(stdoutPath, out, StandardOpenOption.CREATE_NEW); Files.write(stderrPath, err, StandardOpenOption.CREATE_NEW);
            if (process.exitValue() == 77) {
                Path snapshots = Path.of(args[4]).toAbsolutePath().normalize();
                require(session.beforeMeasurement() && !Files.exists(snapshots, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(snapshots),
                        "child exit77 is valid only before all measurement evidence and without a snapshot: " + json(session.measurementEvidence()));
                require(hash(Files.readAllBytes(jar)).equals(jarHash), "original archive preserved");
                Map<String, Object> unavailable = object("schema", 1, "status", "unavailable", "side", args[1], "seed_hex", args[2],
                        "stage", "original_setup", "child_exit", 77, "jar_sha256", jarHash,
                        "measurement_evidence", session.measurementEvidence(), "snapshot_exists", false,
                        "child_stdout", object("file", stdoutPath.getFileName().toString(), "bytes", out.length, "sha256", hash(out)),
                        "child_stderr", object("file", stderrPath.getFileName().toString(), "bytes", err.length, "sha256", hash(err)),
                        "elapsed_ms", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started));
                byte[] encoded = (json(unavailable) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "unavailable report output bound");
                Files.write(unavailablePath, encoded, StandardOpenOption.CREATE_NEW);
                System.err.println("UNAVAILABLE: genuine normal generation setup before measurement"); status = 77;
            } else {
                require(process.exitValue() == 0, "original debuggee exit " + process.exitValue() + ":\n" + new String(err, StandardCharsets.UTF_8));
                require(session.calls.size() == CALLS && session.active == -1 && session.pending == null && !session.finishing,
                        "all nine complete generation calls");
                Path snapshots = Path.of(args[4]); require(Files.isRegularFile(snapshots, LinkOption.NOFOLLOW_LINKS) && Files.size(snapshots) <= MAX_BYTES,
                        "complete bounded generation snapshots");
                require(hash(Files.readAllBytes(jar)).equals(jarHash), "original archive preserved");
                Map<String, Object> record = object("schema", 1, "side", args[1], "seed_hex", args[2], "jar_sha256", jarHash,
                        "can_get_method_return_values", true, "can_get_bytecodes", true, "vm_name", vmName, "vm_version", vmVersion,
                        "listen_address", "127.0.0.1", "port", Integer.parseInt(port), "setup_trace", session.setupTrace(),
                        "constructor_random", session.constructorRandom, "calls", session.calls, "child_exit", process.exitValue(),
                        "child_stdout", object("file", stdoutPath.getFileName().toString(), "bytes", out.length, "sha256", hash(out)),
                        "child_stderr", object("file", stderrPath.getFileName().toString(), "bytes", err.length, "sha256", hash(err)),
                        "elapsed_ms", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started));
                // JSON encoding is shared with the reviewed chunk observer.
                byte[] encoded = (json(record) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "complete trace output bound"); Files.write(tracePath, encoded, StandardOpenOption.CREATE_NEW);
                System.out.println(args[1] + " seed=" + args[2] + " calls=9 height_queries=9216 stages=153"); status = 0;
            }
        } catch (Throwable failure) {
            System.err.println("FAILURE: normal generation controller"); failure.printStackTrace(System.err);
        } finally {
            if (vm != null) try { vm.dispose(); } catch (RuntimeException ignored) { }
            if (process != null && process.isAlive()) {
                process.destroyForcibly(); try { process.waitFor(2, TimeUnit.SECONDS); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            }
            saveLog(stdoutPath, stdout); saveLog(stderrPath, stderr); readers.shutdownNow();
        }
        System.exit(status);
    }
}
