import com.sun.jdi.ArrayReference;
import com.sun.jdi.ByteValue;
import com.sun.jdi.StringReference;
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

/** Loopback, read-only JDI capture of genuine cache acquisition and full finish queries. */
public final class CacheAcquisitionController {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final int MAX_THREADS = 64, MAX_FRAMES = 64;
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
        require(result > 0, "90-second debuggee/controller deadline"); return Math.min(result, 10000);
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
                && (result.modifiers() == modifiers || (modifiers & 0x1000) != 0 && result.modifiers() == (modifiers | 0xf0000000)), "exact field " + owner.name() + "." + name + " expected=" + signature + "/" + modifiers + " actual=" + (result == null ? "null" : result.signature() + "/" + result.modifiers()));
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
        final int callCount;
        final Field daemonField, holderField;
        final String daemonLayout;
        final List<Object> operations = new ArrayList<>();
        final List<ClassPrepareRequest> preparations = new ArrayList<>();
        final Map<Method, String> exitMethods = new LinkedHashMap<>();
        final Map<String, Object> objectReferences = new LinkedHashMap<>();
        final Map<String, Long> objectIDs = new LinkedHashMap<>();
        Field generatorRandom, caveField, caveRandom;
        Map<String, Object> beginRandom;
        ObjectReference world, cache, generator, owner, chunk, spawnWorld;
        ThreadReference thread, spawnThread;
        ReferenceType worldType, cacheType, chunkType;
        Method worldHeight, directCaller, generate, wrapperLoad, actualLoad, acquire, activate;
        BreakpointRequest heightEntry, spawnEntry, spawnStep;
        MethodExitRequest heightExits, cacheExits, registrationExits, loaderExits, generatorExits;
        BreakpointRequest getEntry;
        int active = -1, beginCount, endCount, eventCount, spawnEntries, randomSteps, pendingDepth;
        int generatedCount, finishingCount, deliveredEvents;
        boolean finishing;
        int[] pending;
        List<Object> events, finishes, queries;
        Map<String, Object> beginThreads;
        String btRun;
        Session(VirtualMachine vm, boolean client, int callCount) {
            this.vm = vm; this.client = client; this.callCount = callCount; manager = vm.eventRequestManager();
            ReferenceType threads = vm.classesByName("java.lang.Thread").get(0);
            require(threads.classLoader() == null, "bootstrap Thread binding");
            if (threads.fieldByName("daemon") != null) {
                daemonField = field(threads, "daemon", "Z", 2); holderField = null; daemonLayout = "thread-daemon";
            } else {
                holderField = field(threads, "holder", "Ljava/lang/Thread$FieldHolder;", 18);
                List<ReferenceType> holders = vm.classesByName("java.lang.Thread$FieldHolder");
                require(holders.size() == 1 && holders.get(0).classLoader() == null, "bootstrap FieldHolder binding");
                daemonField = field(holders.get(0), "daemon", "Z", 64); daemonLayout = "thread-holder-daemon";
            }
            prepare("CacheAcquisitionObservation");
            if (!client) { prepare("dj"); prepare("net.minecraft.server.MinecraftServer"); prepare("bt"); }
        }
        void prepare(String name) {
            ClassPrepareRequest request = manager.createClassPrepareRequest(); request.addClassFilter(name);
            request.setSuspendPolicy(EventRequest.SUSPEND_ALL); request.enable(); preparations.add(request);
        }
        BreakpointRequest stop(Method method, long index, String marker, boolean filtered, int policy) {
            Location location = method.locationOfCodeIndex(index);
            require(location != null && location.codeIndex() == index, "exact original code index " + method + "@" + index);
            BreakpointRequest request = manager.createBreakpointRequest(location); request.putProperty("marker", marker);
            if (filtered) request.addThreadFilter(thread); request.setSuspendPolicy(policy); request.enable(); return request;
        }
        void prepared(ClassPrepareEvent event) {
            ReferenceType type = event.referenceType();
            if (type.name().equals("CacheAcquisitionObservation")) {
                stop(method(type, "begin", "(Ljava/lang/Object;Ljava/lang/Object;I)V", 10), 0, "begin", false, EventRequest.SUSPEND_ALL);
                stop(method(type, "end", "(Ljava/lang/Object;I)V", 10), 0, "end", false, EventRequest.SUSPEND_ALL);
                stop(method(type, "identity", "(Ljava/lang/Object;Ljava/lang/String;)V", 10), 0, "identity", false, EventRequest.SUSPEND_EVENT_THREAD);
            } else if (type.name().equals("dj")) {
                Method spawn = method(type, "c", "()V", 4); byte[] code = spawn.bytecodes();
                require(code.length == 87 && (code[24] & 255) == 27 && (code[31] & 255) == 182 && (code[62] & 255) == 182,
                        "pinned ordinary spawn body");
                spawnEntry = stop(spawn, 0, "spawn_entry", false, EventRequest.SUSPEND_EVENT_THREAD);
                spawnStep = stop(spawn, 24, "spawn_step", false, EventRequest.SUSPEND_EVENT_THREAD);
            } else if (type.name().equals("bt")) {
                btRun = HexFormat.of().formatHex(method(type, "run", "()V", 1).bytecodes());
                require(btRun.equals("140005b8000ba7fffa4ca7fff6"), "exact inert bt.run body");
            } else {
                require(type.name().equals("net.minecraft.server.MinecraftServer"), "known prepared original type");
                for (String[] binding : new String[][] {{"c", "()Z", "2"}, {"run", "()V", "1"}, {"main", "([Ljava/lang/String;)V", "9"}})
                    stop(method(type, binding[0], binding[1], Integer.parseInt(binding[2])), 0, "forbidden_server_start", false, EventRequest.SUSPEND_ALL);
            }
            event.request().disable();
        }
        ReferenceType originalType(String name) {
            List<ReferenceType> found = vm.classesByName(name);
            require(found.size() == 1 && world.referenceType().classLoader().equals(found.get(0).classLoader()), "same original loader " + name);
            return found.get(0);
        }
        Value read(ObjectReference object, ReferenceType type, String name, String signature, int modifiers) {
            return object.getValue(field(type, name, signature, modifiers));
        }
        ObjectReference cacheObject(String name, String signature) { return reference(read(cache, cacheType, name, signature, 2)); }
        Value bootstrap(ObjectReference value, String name, String signature) {
            require(value.referenceType().classLoader() == null, "bootstrap collection identity");
            Field actual = value.referenceType().fieldByName(name);
            require(actual != null && actual.signature().equals(signature), "bootstrap field descriptor " + name);
            return value.getValue(actual);
        }
        List<Object> mapNodes(ObjectReference map, boolean values) {
            require(map.referenceType().name().equals("java.util.HashMap"), "actual HashMap");
            int size = integer(bootstrap(map, "size", "I")); require(size >= 0 && size <= 64, "complete bounded map");
            List<Object> result = new ArrayList<>(); Set<Long> nodes = new HashSet<>();
            Value table = bootstrap(map, "table", "[Ljava/util/HashMap$Node;");
            if (table != null) {
                require(table instanceof ArrayReference && ((ArrayReference) table).length() <= 256, "bounded table");
                for (Value bucket : ((ArrayReference) table).getValues()) {
                    for (ObjectReference node = (ObjectReference) bucket; node != null; node = (ObjectReference) bootstrap(node, "next", "Ljava/util/HashMap$Node;")) {
                        require(result.size() < 64 && nodes.add(node.uniqueID()) && node.referenceType().name().equals("java.util.HashMap$Node"), "bounded ordinary map chains");
                        ObjectReference key = reference(bootstrap(node, "key", "Ljava/lang/Object;"));
                        require(key.referenceType().name().equals("java.lang.Integer"), "actual Integer key");
                        int number = integer(bootstrap(key, "value", "I"));
                        result.add(values ? object("key", number, "value", chunk((ObjectReference) bootstrap(node, "value", "Ljava/lang/Object;"))) : number);
                    }
                }
            }
            require(result.size() == size, "complete actual map traversal"); return result;
        }
        List<Object> listValues(ObjectReference list, boolean chunks, int limit) {
            require(list.referenceType().name().equals("java.util.ArrayList"), "actual ArrayList");
            int size = integer(bootstrap(list, "size", "I")); require(size >= 0 && size <= limit, "bounded list");
            ArrayReference elements = (ArrayReference) bootstrap(list, "elementData", "[Ljava/lang/Object;");
            require(elements != null && elements.length() >= size && elements.length() <= 65536, "bounded list owner storage");
            List<Object> result = new ArrayList<>();
            for (Value value : elements.getValues(0, size)) result.add(chunks ? chunk((ObjectReference) value) : region(reference(value)));
            return result;
        }
        Object chunk(ObjectReference value) {
            if (value == null) return null;
            require(chunkType.classLoader().equals(value.referenceType().classLoader()), "same original chunk loader");
            Map<String, Object> flags = new LinkedHashMap<>();
            for (String name : new String[] {"c", "n", "o", "p", "q"}) flags.put(name, ((BooleanValue) read(value, chunkType, name, "Z", 1)).value());
            return object("id", value.uniqueID(), "class", value.referenceType().name(),
                    "coordinates", Arrays.asList(integer(read(value, chunkType, "j", "I", 17)), integer(read(value, chunkType, "k", "I", 17))),
                    "flags", flags, "time", String.format("%016x", ((LongValue) read(value, chunkType, "r", "J", 1)).value()));
        }
        Object region(ObjectReference value) {
            ReferenceType type = originalType(client ? "st" : "lv");
            require(value.referenceType().equals(type), "actual queued light region");
            ObjectReference kind = reference(read(value, type, "a", client ? "Leb;" : "Lco;", 17));
            ReferenceType kindType = originalType(client ? "eb" : "co");
            List<Object> bounds = new ArrayList<>();
            ObjectReference sky = reference(kindType.getValue(field(kindType, "a", client ? "Leb;" : "Lco;", 0x4019)));
            ObjectReference block = reference(kindType.getValue(field(kindType, "b", client ? "Leb;" : "Lco;", 0x4019)));
            require(kind.equals(sky) || kind.equals(block), "actual light enum identity");
            bounds.add(kind.equals(sky) ? 0 : 1);
            for (String name : new String[] {"b", "c", "d", "e", "f", "g"}) bounds.add(integer(read(value, type, name, "I", 1)));
            return object("id", value.uniqueID(), "region", bounds);
        }
        Object cacheSnapshot() {
            String[] names = client ? new String[] {"a","b","e","f"} : new String[] {"b","c","f","g"};
            ObjectReference map = cacheObject(names[2], "Ljava/util/Map;"), list = cacheObject(names[3], "Ljava/util/List;");
            ObjectReference pendingSet = cacheObject(names[0], "Ljava/util/Set;");
            require(pendingSet.referenceType().name().equals("java.util.HashSet"), "actual pending HashSet");
            ObjectReference pendingMap = reference(bootstrap(pendingSet, "map", "Ljava/util/HashMap;"));
            ObjectReference queue = reference(read(world, worldType, "C", "Ljava/util/List;", 2));
            return object("cache_id", cache.uniqueID(), "map_id", map.uniqueID(), "list_id", list.uniqueID(),
                    "pending_id", pendingSet.uniqueID(), "queue_id", queue.uniqueID(),
                    "entries", mapNodes(map, true), "insertions", listValues(list, true, 64), "pending", mapNodes(pendingMap, false),
                    "queue", listValues(queue, false, 32768), "fallback", chunk(cacheObject(names[1], client ? "Llm;" : "Lhi;")),
                    "world_force_load", ((BooleanValue) read(world, worldType, "y", "Z", 1)).value(),
                    "provider_force_load", client ? null : ((BooleanValue) read(cache, cacheType, "a", "Z", 1)).value());
        }
        // JDWP class filters still incur VM-wide method-exit work. Keep each
        // return watch enabled only inside a short known lifecycle interval;
        // expensive terrain/snapshot/JSON work runs without such a watch.
        MethodExitRequest exits(ReferenceType type) {
            MethodExitRequest request = manager.createMethodExitRequest(); request.addClassFilter(type); request.addThreadFilter(thread);
            request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD); request.enable(); return request;
        }
        void install() {
            worldType = originalType(client ? "fd" : "dj"); chunkType = originalType(client ? "lm" : "hi");
            cacheType = originalType(client ? "ok" : "he");
            require(cache.referenceType().equals(cacheType) && world.referenceType().name().equals(client ? "fd" : "dp"), "measured exact scope");
            generator = cacheObject(client ? "c" : "d", client ? "Lcl;" : "Lbl;");
            ReferenceType generatorType = originalType(client ? "yf" : "pk"); require(generator.referenceType().equals(generatorType), "retained actual generator");
            require(reference(read(generator, generatorType, "p", client ? "Lfd;" : "Ldj;", 2)).equals(world), "generator/world identity");
            generatorRandom = field(generatorType, "j", "Ljava/util/Random;", 2);
            caveField = field(generatorType, "u", client ? "Lfv;" : "Ldy;", 2);
            caveRandom = field(originalType(client ? "fv" : "dy"), "b", "Ljava/util/Random;", 4);
            if (!client) owner = reference(read(world, originalType("dp"), "F", "Lnet/minecraft/server/MinecraftServer;", 2));
            acquire = method(cacheType, "c", client ? "(II)Llm;" : "(II)Lhi;", 1);
            require(acquire.bytecodes().length == 394, "complete original acquisition body");
            for (int index : new int[] {0,20,46,78,85,100,112,128,391}) stop(acquire, index, "acquire", true, EventRequest.SUSPEND_EVENT_THREAD);
            Method get = method(cacheType, "b", client ? "(II)Llm;" : "(II)Lhi;", 1);
            getEntry = stop(get, 0, "get", true, EventRequest.SUSPEND_EVENT_THREAD); exitMethods.put(get, "get_return");
            wrapperLoad = method(cacheType, client ? "d" : "e", client ? "(II)Llm;" : "(II)Lhi;", 2);
            stop(wrapperLoad, 0, "load_wrapper", true, EventRequest.SUSPEND_EVENT_THREAD); exitMethods.put(wrapperLoad, "load_wrapper_return");
            ObjectReference loader = cacheObject(client ? "d" : "e", client ? "Lbf;" : "Lan;");
            actualLoad = method(loader.referenceType(), "a", client ? "(Lfd;II)Llm;" : "(Ldj;II)Lhi;", 1);
            stop(actualLoad, 0, "loader", true, EventRequest.SUSPEND_EVENT_THREAD); exitMethods.put(actualLoad, "loader_return"); loaderExits = exits(loader.referenceType()); loaderExits.disable();
            generate = method(generatorType, "b", client ? "(II)Llm;" : "(II)Lhi;", 1);
            stop(generate, 0, "generate", true, EventRequest.SUSPEND_EVENT_THREAD);
            stop(generate, 129, "generator_ready", true, EventRequest.SUSPEND_EVENT_THREAD);
            exitMethods.put(generate, "generate_return"); generatorExits = exits(generatorType); generatorExits.disable();
            Method finish = method(chunkType, client ? "c" : "b", "()V", 1);
            require(finish.bytecodes().length == 222, "complete original finish body");
            stop(finish, 0, "finish", true, EventRequest.SUSPEND_EVENT_THREAD); stop(finish, 221, "finish", true, EventRequest.SUSPEND_EVENT_THREAD);
            activate = method(chunkType, client ? "e" : "d", "()V", 1);
            require(activate.bytecodes().length == 52, "complete original activation body");
            for (int index : new int[] {0,5,51}) stop(activate, index, "activate", true, EventRequest.SUSPEND_EVENT_THREAD);
            Method population = method(cacheType, "a", client ? "(Lcl;II)V" : "(Lbl;II)V", 1);
            stop(population, 0, "forbidden_population", true, EventRequest.SUSPEND_EVENT_THREAD);
            for (String signature : new String[] {"(Ljava/util/Collection;)V", "(Ljava/util/List;)V"}) {
                Method registration = method(worldType, "a", signature, 1);
                stop(registration, 0, signature.contains("Collection") ? "register_tiles" : "register_entities", true, EventRequest.SUSPEND_EVENT_THREAD);
                exitMethods.put(registration, signature.contains("Collection") ? "register_tiles_return" : "register_entities_return");
            }
            exitMethods.put(acquire, "acquire_return"); cacheExits = exits(cacheType); cacheExits.disable();
            registrationExits = exits(worldType); registrationExits.disable();
            if (!client) {
                Method drop = method(cacheType, "d", "(II)V", 1);
                for (int index : new int[] {0,71,89}) stop(drop, index, "pending", true, EventRequest.SUSPEND_EVENT_THREAD);
                exitMethods.put(drop, "pending_return");
            }
            worldHeight = method(worldType, "d", "(II)I", 1); directCaller = method(chunkType, "f", "(III)V", 2);
            heightEntry = manager.createBreakpointRequest(worldHeight.location()); heightEntry.addThreadFilter(thread);
            heightEntry.putProperty("marker", "height"); heightEntry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
            heightExits = manager.createMethodExitRequest(); heightExits.addClassFilter(worldType); heightExits.addThreadFilter(thread);
            heightExits.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        }
        Map<String, Object> inventory(BreakpointEvent event, String marker, int index) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL && event.thread().equals(thread)
                    && thread.name().equals("main"), "complete inventory at matched suspended main marker");
            List<ThreadReference> actualThreads = vm.allThreads();
            require(!actualThreads.isEmpty() && actualThreads.size() <= MAX_THREADS, "complete Java thread inventory budget");
            ClassLoaderReference loader = world.referenceType().classLoader(); List<Object> entries = new ArrayList<>();
            int mainCount = 0, inertCount = 0; boolean othersDaemon = true, othersUnrelated = true, noUnallowed = true;
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
                boolean inert = !client && candidate.referenceType().equals(originalType("bt"));
                if (measured) require(!daemon, "measured main is non-daemon");
                else {
                    othersDaemon &= daemon; othersUnrelated &= !original;
                    if (inert) {
                        ++inertCount;
                        require(daemon && reference(read(candidate, originalType("bt"), "a", "Lnet/minecraft/server/MinecraftServer;", 0x1010)).equals(owner)
                                && actualFrames.size() >= 2 && actualFrames.size() <= 3, "exact owner-bound inert daemon");
                        StackFrame run = actualFrames.get(actualFrames.size() - 1);
                        require(run.location().method().equals(method(originalType("bt"), "run", "()V", 1))
                                && candidate.equals(run.thisObject()) && run.location().codeIndex() == 3, "exact inert original run sleep call");
                        for (int i = 0; i < actualFrames.size() - 1; ++i) {
                            Location location = actualFrames.get(i).location();
                            require(location.declaringType().name().equals("java.lang.Thread") && location.declaringType().classLoader() == null
                                    && (location.method().name().equals("sleep") || location.method().name().equals("sleep0"))
                                    && location.method().signature().equals("(J)V"), "only exact platform sleep chain");
                        }
                    } else noUnallowed &= !original;
                }
                entries.add(object("ordinal", entries.size(), "id", candidate.uniqueID(), "name", candidate.name(),
                        "object_class", candidate.referenceType().name(), "status", candidate.status(), "daemon", daemon,
                        "measured", measured, "exact_inert_bt", inert, "suspended", true, "frame_count", count, "original_frame", original, "frames", frames));
            }
            require(mainCount == 1 && othersDaemon && noUnallowed && (client ? inertCount == 0 && othersUnrelated : inertCount == 1), "finite scope-specific marker inventory");
            return object("marker", marker, "index", index, "suspend_policy", EventRequest.SUSPEND_ALL,
                    "measured_thread_id", thread.uniqueID(), "original_loader_id", loader.uniqueID(), "thread_limit", MAX_THREADS,
                    "frames_per_thread_limit", MAX_FRAMES, "daemon_layout", daemonLayout,
                    "daemon_field", object("owner", daemonField.declaringType().name(), "signature", "Z", "modifiers", daemonField.modifiers()),
                    "holder_field", holderField == null ? null : object("owner", "java.lang.Thread", "signature", holderField.signature(), "modifiers", holderField.modifiers()),
                    "thread_count", entries.size(), "measured_main_thread", true, "all_threads_suspended", true,
                    "all_other_threads_daemon", othersDaemon, "no_other_original_frames", othersUnrelated, "no_unallowed_original_frames", noUnallowed,
                    "exact_inert_bt_count", inertCount, "bt_run_hex", btRun, "threads", entries);
        }

        boolean direct(ThreadReference actual) throws Exception {
            return actual.frameCount() >= 2 && world.equals(actual.frame(0).thisObject())
                    && directCaller.equals(actual.frame(1).location().method()) && chunk.equals(actual.frame(1).thisObject());
        }
        Map<String, Object> seeds() {
            ObjectReference cave = reference(generator.getValue(caveField));
            return object("generator_seed", String.format("%012x", randomSeed(reference(generator.getValue(generatorRandom)))),
                    "cave_seed", String.format("%012x", randomSeed(reference(cave.getValue(caveRandom)))));
        }
        Object byteArray(ObjectReference value, String name, int length) {
            ArrayReference array = (ArrayReference) read(value, chunkType, name, "[B", 1);
            return bytes(array, length);
        }
        Object bytes(ArrayReference array, int length) {
            require(array != null && array.length() == length, "complete original local array");
            byte[] raw = new byte[length]; List<Value> values = array.getValues();
            for (int i = 0; i < length; ++i) { require(values.get(i) instanceof ByteValue, "actual byte value"); raw[i] = ((ByteValue) values.get(i)).value(); }
            return object("length", length, "values", HexFormat.of().formatHex(raw));
        }
        Object localChunk(ObjectReference value) {
            Map<String, Object> data = new LinkedHashMap<>(); data.put("blocks", byteArray(value, "b", 32768));
            ReferenceType nibble = originalType(client ? "wi" : "ob");
            String[] names = {"e","f","g"}, roles = {"metadata","skylight","block_light"};
            for (int i = 0; i < names.length; ++i) {
                ObjectReference wrapper = reference(read(value, chunkType, names[i], client ? "Lwi;" : "Lob;", 1));
                data.put(roles[i], bytes((ArrayReference) read(wrapper, nibble, "a", "[B", 17), 16384));
            }
            data.put("heightmap", byteArray(value, "h", 256)); data.put("minimum_height", integer(read(value, chunkType, "i", "I", 1)));
            return object("chunk", chunk(value), "data", data);
        }
        void begin(BreakpointEvent event) throws Exception {
            List<Value> arguments = event.thread().frame(0).getArgumentValues(); int index = integer(arguments.get(2));
            require(active == -1 && index == operations.size() && index < callCount, "ordered complete acquisition markers");
            ObjectReference actualWorld = reference(arguments.get(0)), actualCache = reference(arguments.get(1));
            if (index == 0) {
                world = actualWorld; cache = actualCache; thread = event.thread();
                require(thread.name().equals("main"), "measured main thread");
                if (!client) {
                    require(spawnEntries == 1 && randomSteps == 0 && world.equals(spawnWorld) && thread.equals(spawnThread)
                            && btRun != null, "qualifying dedicated setup and exact inert daemon body");
                    spawnEntry.disable(); spawnStep.disable();
                }
                install();
            } else require(world.equals(actualWorld) && cache.equals(actualCache) && thread.equals(event.thread()), "stable actual context across operations");
            active = index; chunk = null; finishing = false; pending = null; pendingDepth = 0;
            events = new ArrayList<>(); finishes = new ArrayList<>(); queries = null; generatedCount = 0; finishingCount = 0;
            beginThreads = inventory(event, "begin", index); beginRandom = seeds();
        }
        void record(ThreadReference actual, String kind, ObjectReference result) throws Exception {
            require(active >= 0 && thread.equals(actual) && events.size() < 512 && ++eventCount <= 4096, "bounded measured events");
            StackFrame frame = actual.frame(0); Location location = frame.location();
            List<Object> arguments = new ArrayList<>();
            for (Value argument : frame.getArgumentValues()) {
                if (argument instanceof IntegerValue) arguments.add(integer(argument));
                else if (argument instanceof ObjectReference) arguments.add(object("id", ((ObjectReference) argument).uniqueID(), "class", ((ObjectReference) argument).referenceType().name()));
                else require(argument == null, "known original lifecycle argument shape");
            }
            List<Object> callers = new ArrayList<>();
            require(actual.frameCount() <= MAX_FRAMES, "bounded measured stack");
            for (StackFrame caller : actual.frames()) {
                Location site = caller.location();
                if (world.referenceType().classLoader().equals(site.declaringType().classLoader()))
                    callers.add(object("class", site.declaringType().name(), "method", site.method().name(), "signature", site.method().signature(),
                            "code_index", site.codeIndex(), "receiver", caller.thisObject() == null ? null : caller.thisObject().uniqueID()));
            }
            events.add(object("ordinal", events.size(), "kind", kind, "class", location.declaringType().name(), "method", location.method().name(),
                    "signature", location.method().signature(), "code_index", location.codeIndex(), "thread_id", thread.uniqueID(),
                    "receiver", frame.thisObject() == null ? null : frame.thisObject().uniqueID(), "arguments", arguments,
                    "result", result == null ? null : chunk(result), "generated_local", kind.equals("generate_return") ? localChunk(result) : null, "chunk", chunk == null ? null : chunk(chunk),
                    "cache", cacheSnapshot(), "original_frames", callers));
        }
        void requireCaller(Method method, ObjectReference receiver) throws Exception {
            require(thread.frameCount() >= 2 && thread.frame(1).location().method().equals(method)
                    && receiver.equals(thread.frame(1).thisObject()), "matched direct original caller and receiver");
        }
        void stopped(BreakpointEvent event) throws Exception {
            String marker = (String) event.request().getProperty("marker");
            require(!marker.startsWith("forbidden"), "unrequested original boundary " + marker);
            if (marker.equals("spawn_entry")) {
                require(beginCount == 0 && spawnEntries++ == 0, "one constructor spawn entry");
                spawnWorld = event.thread().frame(0).thisObject(); spawnThread = event.thread(); return;
            }
            if (marker.equals("spawn_step")) {
                require(beginCount == 0 && spawnEntries == 1 && spawnThread.equals(event.thread())
                        && spawnWorld.equals(event.thread().frame(0).thisObject()) && ++randomSteps <= 65536, "bounded genuine spawn steps"); return;
            }
            if (marker.equals("identity")) {
                List<Value> values = event.thread().frame(0).getArgumentValues(); ObjectReference value = reference(values.get(0));
                require(values.get(1) instanceof StringReference && event.thread().name().equals("main"), "ordinary helper reference marker");
                String name = ((StringReference) values.get(1)).value(); require(name.matches("r[0-9]+") && objectReferences.size() < 1024, "bounded deterministic helper reference");
                Long previous = objectIDs.putIfAbsent(name, value.uniqueID()); require(previous == null || previous.longValue() == value.uniqueID(), "stable helperref/objectID binding");
                // Match Class.getName() in Model.reference: JDI names byte arrays
                // "byte[]", whereas their reflection class name is "[B".
                String className = value instanceof ArrayReference
                        ? value.referenceType().signature().replace('/', '.') : value.referenceType().name();
                objectReferences.putIfAbsent(name, object("reference", name, "object_id", value.uniqueID(), "class", className,
                        "loader_id", value.referenceType().classLoader() == null ? null : value.referenceType().classLoader().uniqueID())); return;
            }
            if (marker.equals("begin")) { ++beginCount; begin(event); return; }
            require(active >= 0 && thread.equals(event.thread()), "measurement inside matching main operation");
            StackFrame frame = thread.frame(0);
            if (marker.equals("end")) {
                ++endCount; List<Value> arguments = frame.getArgumentValues();
                require(integer(arguments.get(1)) == active && pending == null && !finishing && generatedCount == finishingCount
                        && generatedCount == finishes.size(), "complete acquisition operation and all generated full finishes");
                operations.add(object("index", active, "thread_id", thread.uniqueID(), "world_id", world.uniqueID(), "cache_id", cache.uniqueID(),
                        "generator_id", generator.uniqueID(), "result", chunk((ObjectReference) arguments.get(0)),
                        "begin_threads", beginThreads, "end_threads", inventory(event, "end", active), "begin_random", beginRandom, "end_random", seeds(), "events", events, "finishes", finishes));
                active = -1; cacheExits.disable(); loaderExits.disable(); generatorExits.disable(); registrationExits.disable(); return;
            }
            if (marker.equals("height")) {
                require(finishing && direct(thread) && pending == null && queries.size() < 1024, "bounded matched direct height entry");
                List<Value> arguments = frame.getArgumentValues(), caller = thread.frame(1).getArgumentValues();
                int x = integer(arguments.get(0)), z = integer(arguments.get(1));
                require(x == integer(caller.get(0)) && z == integer(caller.get(1)), "same original height call arguments");
                pending = new int[] {x,z,integer(caller.get(2))}; pendingDepth = thread.frameCount(); heightExits.enable(); return;
            }
            if (marker.equals("acquire") || marker.equals("get") || marker.equals("pending") || marker.equals("load_wrapper"))
                require(cache.equals(frame.thisObject()), "actual retained cache receiver");
            if (marker.equals("load_wrapper")) { requireCaller(acquire, cache); cacheExits.enable(); }
            if (marker.equals("get") || marker.equals("acquire") && event.location().codeIndex() == 391) cacheExits.enable();
            if (marker.equals("loader")) { requireCaller(wrapperLoad, cache); require(reference(frame.getArgumentValues().get(0)).equals(world), "actual loader world argument"); loaderExits.enable(); }
            if (marker.equals("generate")) { require(generator.equals(frame.thisObject()), "retained generator receiver"); requireCaller(acquire, cache); ++generatedCount; cacheExits.disable(); }
            if (marker.equals("generator_ready")) { require(generator.equals(frame.thisObject()) && !finishing, "finished generator return site"); generatorExits.enable(); }
            if (marker.equals("finish")) {
                requireCaller(generate, generator);
                if (event.location().codeIndex() == 0) {
                    require(!finishing && pending == null, "single complete finish"); chunk = frame.thisObject();
                    require(chunk.referenceType().equals(chunkType) && reference(read(chunk, chunkType, "d", client ? "Lfd;" : "Ldj;", 1)).equals(world), "exact normal chunk and world");
                    queries = new ArrayList<>(); finishing = true; cacheExits.disable(); getEntry.disable(); heightEntry.enable();
                } else {
                    require(finishing && frame.thisObject().equals(chunk) && pending == null && queries.size() == 1024, "all1024 exact original height pairs");
                    finishing = false; heightEntry.disable(); heightExits.disable(); getEntry.enable(); ++finishingCount;
                    finishes.add(object("chunk", chunk(chunk), "direct_call_count", queries.size(), "records", queries));
                }
            }
            if (marker.equals("activate")) { requireCaller(acquire, cache); require(frame.thisObject().equals(chunk), "activate returned generated chunk"); }
            if (marker.equals("register_tiles") || marker.equals("register_entities")) { requireCaller(activate, chunk); require(world.equals(frame.thisObject()), "actual world registration receiver"); registrationExits.enable(); }
            record(thread, marker, null);
        }
        void returned(MethodExitEvent event) throws Exception {
            if (event.request().equals(heightExits)) {
                if (!event.method().equals(worldHeight)) return;
                require(active >= 0 && finishing && pending != null && thread.equals(event.thread())
                        && thread.frameCount() == pendingDepth && direct(thread), "matched exact height return frame");
                List<Value> arguments = thread.frame(0).getArgumentValues(), caller = thread.frame(1).getArgumentValues();
                require(integer(arguments.get(0)) == pending[0] && integer(arguments.get(1)) == pending[1]
                        && integer(caller.get(2)) == pending[2], "retained arguments at original return");
                queries.add(Arrays.asList(pending[0],pending[1],pending[2],integer(event.returnValue())));
                pending = null; heightExits.disable(); return;
            }
            String kind = exitMethods.get(event.method()); if (kind == null || active < 0) return;
            if (event.request().equals(registrationExits)) registrationExits.disable();
            if (event.request().equals(loaderExits)) loaderExits.disable();
            if (event.request().equals(generatorExits)) generatorExits.disable();
            if (event.request().equals(cacheExits)) cacheExits.disable();
            require(thread.equals(event.thread()), "paired original lifecycle return thread");
            Value value = event.returnValue(); ObjectReference result = value instanceof ObjectReference ? (ObjectReference) value : null;
            if (event.method().equals(generate)) require(result != null && result.equals(chunk) && !finishing, "actual generated finished identity return");
            if (event.method().equals(wrapperLoad) || event.method().equals(actualLoad)) {
                require(value == null || value instanceof ObjectReference, "nonnull or genuine load miss");
                require(result == null, "initial fresh-owned corpus must take genuine disk miss");
            }
            record(event.thread(), kind, result);
        }
        boolean beforeMeasurement() { return beginCount == 0 && endCount == 0 && operations.isEmpty() && active == -1
                && eventCount == 0 && world == null && cache == null && generator == null && chunk == null && pending == null && !finishing; }
        Object setupProof() { return object("spawn_entries", spawnEntries, "random_steps", randomSteps, "same_world", client ? null : world.equals(spawnWorld),
                "same_thread", client ? null : thread.equals(spawnThread), "bt_run_hex", btRun); }
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
        long deadline = started + TimeUnit.SECONDS.toNanos(90); int status = 1;
        try {
            require(args.length == 7 || args.length == 8, "jar side mode seed_hex new_owned_root snapshots_json [scenario] trace_json");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            require(args[2].equals(client ? "persistent-client" : "dedicated-constructor") && args[3].equals("0000000000000017"), "explicit scope and initial seed23");
            String scenario = args.length == 8 ? args[6] : "shared";
            require(scenario.equals("shared") || !client && scenario.equals("dedicated-gates"), "scenario");
            Path trace = Path.of(args[args.length-1]).toAbsolutePath().normalize(), snapshot = Path.of(args[5]).toAbsolutePath().normalize();
            stdoutPath = Path.of(trace + ".child-stdout.txt"); stderrPath = Path.of(trace + ".child-stderr.txt");
            Path unavailable = Path.of(trace + ".unavailable.json");
            for (Path output : Arrays.asList(trace, snapshot, stdoutPath, stderrPath, unavailable)) newOutput(output);
            require(new HashSet<>(Arrays.asList(trace,snapshot,stdoutPath,stderrPath,unavailable)).size() == 5, "distinct new output paths");
            Path jar = Path.of(args[0]).toRealPath(); String jarHash = hash(Files.readAllBytes(jar));
            require(jarHash.equals(client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"), "pinned original archive");
            Path java = Path.of(System.getProperty("java.home"), "bin", System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            List<String> command = new ArrayList<>(Arrays.asList(java.toString(), "-Xint", "-Xmx512m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", System.getProperty("java.class.path"), "CacheAcquisitionObservation"));
            command.addAll(Arrays.asList(Arrays.copyOf(args,args.length-1))); process = new ProcessBuilder(command).start(); Process child = process;
            AtomicLong captured = new AtomicLong(); CompletableFuture<String> startup = new CompletableFuture<>();
            stdout = readers.submit(() -> capture(child.getInputStream(), startup, captured)); stderr = readers.submit(() -> capture(child.getErrorStream(), null, captured));
            String listening = startup.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(listening.matches("Listening for transport dt_socket at address: [0-9]+"), "loopback ephemeral JDWP startup");
            String port = listening.substring(listening.lastIndexOf(' ')+1);
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments(); connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(port); connection.get("timeout").setValue(Long.toString(remaining(deadline))); vm = connector.attach(connection);
            require(vm.canGetMethodReturnValues() && vm.canGetBytecodes(), "actual return values and bytecodes required");
            String vmName = vm.name(), vmVersion = vm.version(); Session session = new Session(vm,client,scenario.equals("shared") ? 6 : 8);
            boolean alive = true; vm.resume();
            while (alive && System.nanoTime() < deadline) {
                EventSet set;
                try { set = vm.eventQueue().remove(Math.min(1000,remaining(deadline))); } catch (VMDisconnectedException closed) { break; }
                if (set == null) continue;
                for (Event event : set) {
                    require(++session.deliveredEvents <= 65536, "bounded total JDI event delivery");
                    if (event instanceof ClassPrepareEvent) session.prepared((ClassPrepareEvent) event);
                    else if (event instanceof BreakpointEvent) session.stopped((BreakpointEvent) event);
                    else if (event instanceof MethodExitEvent) session.returned((MethodExitEvent) event);
                    else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) alive = false;
                }
                try { set.resume(); } catch (VMDisconnectedException closed) { alive = false; }
            }
            require(process.waitFor(remaining(deadline),TimeUnit.MILLISECONDS), "bounded child exit");
            byte[] out = stdout.get(remaining(deadline),TimeUnit.MILLISECONDS), err = stderr.get(remaining(deadline),TimeUnit.MILLISECONDS);
            Files.write(stdoutPath,out,StandardOpenOption.CREATE_NEW); Files.write(stderrPath,err,StandardOpenOption.CREATE_NEW);
            require(hash(Files.readAllBytes(jar)).equals(jarHash), "original archive preserved");
            if (process.exitValue() == 77) {
                require(session.beforeMeasurement() && !Files.exists(snapshot,LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(snapshot), "exit77 only before all measurements and without snapshots");
                Files.writeString(unavailable,json(object("schema",1,"status","unavailable","stage","original_setup","child_exit",77,
                        "side",args[1],"mode",args[2],"scenario",scenario,"jar_sha256",jarHash,"begin_markers",0,"end_markers",0,
                        "completed_operations",0,"snapshot_exists",false,"child_command",command,"debuggee_mode","interpreted","elapsed_ms",TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-started)))+"\n",StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW);
                status = 77;
            } else {
                require(process.exitValue() == 0,"original child exit " + process.exitValue() + ":\n" + new String(err,StandardCharsets.UTF_8));
                require(session.operations.size() == session.callCount && session.beginCount == session.callCount && session.endCount == session.callCount
                        && session.active == -1 && session.pending == null && !session.finishing, "all complete operation/finish boundaries");
                require(Files.isRegularFile(snapshot,LinkOption.NOFOLLOW_LINKS) && Files.size(snapshot) <= MAX_BYTES,"complete bounded snapshots");
                Map<String,Object> record = object("schema",1,"side",args[1],"mode",args[2],"scenario",scenario,"seed_hex",args[3],"jar_sha256",jarHash,
                        "can_get_method_return_values",true,"can_get_bytecodes",true,"vm_name",vmName,"vm_version",vmVersion,"child_command",command,"debuggee_mode","interpreted",
                        "listen_address","127.0.0.1","port",Integer.parseInt(port),"setup_proof",session.setupProof(),"object_references",new ArrayList<>(session.objectReferences.values()),"operations",session.operations,
                        "jdi_event_count",session.deliveredEvents,"child_exit",0,"child_stdout",object("file",stdoutPath.getFileName().toString(),"bytes",out.length,"sha256",hash(out)),
                        "child_stderr",object("file",stderrPath.getFileName().toString(),"bytes",err.length,"sha256",hash(err)),
                        "elapsed_ms",TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-started));
                byte[] encoded = (json(record)+"\n").getBytes(StandardCharsets.UTF_8); require(encoded.length <= MAX_BYTES,"complete trace output bound");
                Files.write(trace,encoded,StandardOpenOption.CREATE_NEW); status = 0;
                System.out.println(args[1]+" "+scenario+" operations="+session.callCount+" events="+session.eventCount);
            }
        } catch (Throwable failure) { System.err.println("FAILURE: cache acquisition controller"); failure.printStackTrace(System.err); }
        finally {
            if (vm != null) try { vm.dispose(); } catch (RuntimeException ignored) { }
            if (process != null && process.isAlive()) {
                process.destroyForcibly(); try { process.waitFor(2,TimeUnit.SECONDS); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            }
            saveLog(stdoutPath,stdout); saveLog(stderrPath,stderr); readers.shutdownNow();
        }
        System.exit(status);
    }
}
