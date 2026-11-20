import com.sun.jdi.*;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

/** Reads a suspended untouched-original prefix; never completes Block bootstrap. */
public final class BlockRegistryPrefixController {
    private static final int STREAM_LIMIT = 16 * 1024 * 1024;
    private static final int ROW_LIMIT = 20000, TEXT_LIMIT = 4 * 1024 * 1024;
    private static final String HELPER = "BlockRegistryPrefixObservation";
    private static final String[] NAMES = ("u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
            + "aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az "
            + "aA aB aC aD aE aF aG aH aI aJ aK aL aM aN aO aP aQ aR aS aT aU aV aW aX aY aZ "
            + "ba bb bc bd be bf bg bh bi bj bk bl").split(" ");

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static String hex(long value, int digits) {
        String result = Long.toHexString(value);
        if (result.length() > digits) result = result.substring(result.length() - digits);
        return "0".repeat(digits - result.length()) + result;
    }

    private static String units(String value) {
        require(value.length() <= 65536, "individual UTF-16 string bound");
        if (value.isEmpty()) return "-";
        StringBuilder result = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); i++) result.append(hex(value.charAt(i), 4));
        return result.toString();
    }

    private static final class Rows {
        final StringBuilder text = new StringBuilder();
        int count;
        void add(Object... columns) {
            require(++count <= ROW_LIMIT, "snapshot/receipt row bound");
            for (int i = 0; i < columns.length; i++) {
                if (i != 0) text.append('\t');
                String value = String.valueOf(columns[i]);
                for (int j = 0; j < value.length(); j++) {
                    char c = value.charAt(j);
                    require(c >= 32 && c <= 126, "ASCII columns without control characters");
                }
                text.append(value);
            }
            text.append('\n');
            require(text.length() <= TEXT_LIMIT, "snapshot/receipt byte bound");
        }
        byte[] bytes() { return text.toString().getBytes(StandardCharsets.US_ASCII); }
    }

    private static long remaining(long deadline) {
        long millis = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(millis > 0, "45-second observation deadline");
        return millis;
    }

    private static final class Drain implements Callable<Void> {
        final InputStream stream;
        final AtomicLong total;
        final ByteArrayOutputStream retained = new ByteArrayOutputStream();
        final CompletableFuture<String> listening;
        volatile Throwable failure;
        Drain(InputStream stream, AtomicLong total, boolean startup) {
            this.stream = stream;
            this.total = total;
            listening = startup ? new CompletableFuture<>() : null;
        }
        public Void call() {
            ByteArrayOutputStream line = listening == null ? null : new ByteArrayOutputStream();
            try {
                byte[] buffer = new byte[4096];
                int n;
                while ((n = stream.read(buffer)) != -1) {
                    long before = total.getAndAdd(n);
                    int keep = (int)Math.max(0L, Math.min((long)n, STREAM_LIMIT - before));
                    synchronized (retained) { retained.write(buffer, 0, keep); }
                    if (keep != n && failure == null) failure = new IllegalStateException("combined child stream cap exceeded");
                    if (line != null) {
                        for (int i = 0; i < n; i++) {
                            if (buffer[i] == '\n') {
                                String actual = line.toString(StandardCharsets.US_ASCII);
                                if (actual.endsWith("\r")) actual = actual.substring(0, actual.length() - 1);
                                listening.complete(actual);
                                line = null;
                                break;
                            }
                            require(line.size() < 1024, "JDWP startup line bound");
                            line.write(buffer[i]);
                        }
                    }
                }
                if (line != null) listening.completeExceptionally(new IllegalStateException("missing JDWP startup line"));
            } catch (Throwable caught) {
                failure = caught;
                if (listening != null) listening.completeExceptionally(caught);
            }
            return null;
        }
        byte[] bytes() { synchronized (retained) { return retained.toByteArray(); } }
    }

    private static final class Watchdog implements Runnable {
        final Process child;
        volatile boolean fired, reaped;
        volatile String failure;
        Watchdog(Process child) { this.child = child; }
        public void run() {
            if (!child.isAlive()) return;
            fired = true;
            try { child.destroyForcibly(); }
            catch (Throwable caught) { failure = "watchdog terminate: " + caught; }
            finally {
                try {
                    reaped = child.waitFor(5, TimeUnit.SECONDS);
                    if (!reaped) failure = "watchdog mandatory wait did not reap child";
                } catch (Throwable caught) {
                    failure = "watchdog mandatory wait: " + caught;
                    if (caught instanceof InterruptedException) Thread.currentThread().interrupt();
                }
            }
        }
    }

    private static Field field(ReferenceType type, String name, String signature, int modifiers) {
        List<Field> found = new ArrayList<>();
        for (Field f : type.fields()) if (f.name().equals(name)) found.add(f);
        require(found.size() == 1 && found.get(0).declaringType().equals(type)
                && found.get(0).signature().equals(signature) && found.get(0).modifiers() == modifiers,
                "exact declared field " + type.name() + "." + name + signature);
        return found.get(0);
    }

    private static ReferenceType bootstrap(VirtualMachine vm, String name) {
        List<ReferenceType> found = vm.classesByName(name).stream().filter(t -> t.classLoader() == null).toList();
        require(found.size() == 1 && found.get(0).isPrepared(), "prepared bootstrap class " + name);
        return found.get(0);
    }

    private static final class Snapshot {
        final VirtualMachine vm;
        final boolean client;
        final ClassLoaderReference loader;
        final ReferenceType blocks;
        final Rows out = new Rows();
        final Map<Long, Integer> ranks = new LinkedHashMap<>();
        final List<ObjectReference> objects = new ArrayList<>();
        final Set<ReferenceType> declarations = new HashSet<>();
        final long deadline;
        int cells, stringUnits;

        Snapshot(VirtualMachine vm, boolean client, ClassLoaderReference loader,
                ReferenceType blocks, long deadline) {
            this.vm = vm; this.client = client; this.loader = loader;
            this.blocks = blocks; this.deadline = deadline;
        }

        int id(ObjectReference value) {
            if (value == null) return 0;
            Integer known = ranks.get(value.uniqueID());
            if (known != null) return known;
            require(objects.size() < 2048, "retained mirror bound");
            value.disableCollection();
            int rank = objects.size() + 1;
            ranks.put(value.uniqueID(), rank);
            objects.add(value);
            return rank;
        }

        String value(Value v) {
            if (v == null) return "NULL";
            if (v instanceof ObjectReference) return "R:" + id((ObjectReference)v);
            if (v instanceof BooleanValue) return "Z:" + (((BooleanValue)v).value() ? "1" : "0");
            if (v instanceof ByteValue) return "B:" + hex(((ByteValue)v).value(), 2);
            if (v instanceof CharValue) return "C:" + hex(((CharValue)v).value(), 4);
            if (v instanceof ShortValue) return "S:" + hex(((ShortValue)v).value(), 4);
            if (v instanceof IntegerValue) return "I:" + hex(((IntegerValue)v).value(), 8);
            if (v instanceof LongValue) return "J:" + hex(((LongValue)v).value(), 16);
            if (v instanceof FloatValue) return "F:" + hex(Float.floatToRawIntBits(((FloatValue)v).value()), 8);
            if (v instanceof DoubleValue) return "D:" + hex(Double.doubleToRawLongBits(((DoubleValue)v).value()), 16);
            throw new IllegalStateException("unsupported JDI value " + v.type().name());
        }

        void type(ReferenceType type) {
            if (!declarations.add(type)) return;
            remaining(deadline);
            out.add("TYPE", type.name(), type.signature(), type.modifiers(),
                    id(type.classLoader()), type.isPrepared(), type.isInitialized(), type.failedToInitialize());
            for (Field f : type.fields()) out.add("FIELD_DECL", type.name(), f.declaringType().name(),
                    f.name(), f.signature(), f.modifiers());
            for (Method m : type.methods()) out.add("METHOD_DECL", type.name(), m.declaringType().name(),
                    m.name(), m.signature(), m.modifiers());
        }

        ReferenceType original(String name) {
            List<ReferenceType> found = vm.classesByName(name).stream().filter(t -> loader.equals(t.classLoader())).toList();
            require(found.size() == 1 && found.get(0).isPrepared(), "already prepared original class " + name);
            return found.get(0);
        }

        void statics(String role, ReferenceType type) {
            type(type);
            List<Field> fields = type.fields().stream().filter(Field::isStatic).toList();
            Map<Field, Value> values = type.getValues(fields);
            require(values.size() == fields.size(), "complete actual static field batch");
            for (Field f : fields) {
                out.add("STATIC", role, type.name(), f.name(), f.signature(), f.modifiers(), value(values.get(f)));
            }
        }

        void instance(ObjectReference object) {
            ReferenceType actual = object.referenceType();
            boolean game = loader.equals(actual.classLoader());
            Set<String> jdk = Set.of("java.util.Random", "java.util.concurrent.atomic.AtomicLong",
                    "java.util.ArrayList", "java.util.HashSet", "java.util.HashMap");
            require(game || jdk.contains(actual.name()), "unexpanded reference class " + actual.name());
            ReferenceType owner = actual;
            while (owner != null) {
                type(owner);
                List<Field> fields = owner.fields().stream().filter(f -> !f.isStatic()).toList();
                Map<Field, Value> values = object.getValues(fields);
                require(values.size() == fields.size(), "complete actual instance field batch");
                for (Field f : fields) {
                    out.add("INSTANCE", id(object), owner.name(), f.name(), f.signature(), f.modifiers(),
                            value(values.get(f)));
                }
                owner = owner instanceof ClassType ? ((ClassType)owner).superclass() : null;
            }
        }

        void graph() {
            for (int index = 0; index < objects.size(); index++) {
                remaining(deadline);
                ObjectReference object = objects.get(index);
                ReferenceType actual = object.referenceType();
                out.add("OBJECT", index + 1, object.uniqueID(), actual.name(), actual.signature(), id(actual.classLoader()));
                if (object instanceof StringReference) {
                    String s = ((StringReference)object).value();
                    stringUnits += s.length();
                    require(stringUnits <= 262144, "total UTF-16 units bound");
                    out.add("STRING", index + 1, s.length(), units(s));
                } else if (object instanceof ArrayReference) {
                    ArrayReference a = (ArrayReference)object;
                    require(a.length() <= 16384 && cells <= 65536 - a.length(), "array cell bound");
                    cells += a.length();
                    out.add("ARRAY", index + 1, a.length(), actual.signature());
                    List<Value> elements = a.getValues();
                    require(elements.size() == a.length(), "complete actual array contents");
                    for (int i = 0; i < elements.size(); i++) out.add("ELEMENT", index + 1, i, value(elements.get(i)));
                } else if (object instanceof ClassObjectReference) {
                    ReferenceType reflected = ((ClassObjectReference)object).reflectedType();
                    out.add("CLASS_MIRROR", index + 1, reflected.name(), reflected.signature(),
                            id(reflected.classLoader()), reflected.isPrepared(), reflected.isInitialized());
                } else if (object instanceof ClassLoaderReference) {
                    out.add("LOADER_MIRROR", index + 1, object.uniqueID(), actual.name());
                } else if (object instanceof ThreadReference) {
                    out.add("THREAD_MIRROR", index + 1, units(((ThreadReference)object).name()));
                } else instance(object);
            }
        }

        void capture(BreakpointEvent event, Method clinit, String bytecodeHash, ReferenceType helper) throws Exception {
            out.add("BLOCK_REGISTRY_PREFIX_V1", client ? "client" : "server");
            out.add("POLICY", "ACTUAL_JDI_READS", "SUSPEND_ALL", "BEFORE_ITEM_ACTIVE_USE", 3398);
            out.add("METHOD_BINDING", blocks.name(), clinit.name(), clinit.signature(),
                    clinit.modifiers(), 3678, bytecodeHash, 3398);
            out.add("LOADER_ROOT", id(loader), loader.uniqueID());
            type(helper);
            Field heldLoader = field(helper, "originalLoader", "Ljava/net/URLClassLoader;", 73);
            Field heldUrl = field(helper, "originalJarUrl", "Ljava/lang/String;", 73);
            Field heldHash = field(helper, "verifiedJarSha256", "Ljava/lang/String;", 73);
            out.add("HELPER_BINDING", helper.name(), id(helper.classLoader()),
                    value(helper.getValue(heldLoader)),
                    units(((StringReference)helper.getValue(heldUrl)).value()),
                    ((StringReference)helper.getValue(heldHash)).value());
            require(blocks.fields().size() == 130, "17 instance plus113 static Block declarations");
            long staticCount = blocks.fields().stream().filter(Field::isStatic).count();
            require(staticCount == 113, "all113 Block static cells");
            statics("BLOCK", blocks);
            Set<Long> named = new HashSet<>(), privateRandoms = new HashSet<>(), seeds = new HashSet<>();
            for (int i = 0; i < NAMES.length; i++) {
                Field f = blocks.fieldByName(NAMES[i]);
                require(f != null && f.declaringType().equals(blocks) && f.modifiers() == 25,
                        "exact named Block static " + NAMES[i]);
                Value v = blocks.getValue(f);
                require(v instanceof ObjectReference && named.add(((ObjectReference)v).uniqueID()),
                        "96 distinct actual named Block owners");
                ObjectReference b = (ObjectReference)v;
                out.add("BLOCK_ALIAS", i, NAMES[i], id(b), b.referenceType().name());
                String own = b.referenceType().name();
                if (own.equals(client ? "xq" : "pa") || own.equals("e") || own.equals(client ? "tc" : "lz")) {
                    Field random = field(b.referenceType(), "a", "Ljava/util/Random;", 2);
                    Value r = b.getValue(random);
                    require(r instanceof ObjectReference && ((ObjectReference)r).referenceType().name().equals("java.util.Random")
                            && ((ObjectReference)r).referenceType().classLoader() == null
                            && privateRandoms.add(((ObjectReference)r).uniqueID()), "distinct ordinary private Random");
                    ReferenceType randomType = ((ObjectReference)r).referenceType();
                    Field seed = field(randomType, "seed", "Ljava/util/concurrent/atomic/AtomicLong;", 18);
                    Value atomic = ((ObjectReference)r).getValue(seed);
                    require(atomic instanceof ObjectReference
                            && ((ObjectReference)atomic).referenceType().name().equals("java.util.concurrent.atomic.AtomicLong")
                            && ((ObjectReference)atomic).referenceType().classLoader() == null
                            && seeds.add(((ObjectReference)atomic).uniqueID()),
                            "distinct private Random seed AtomicLong");
                    out.add("PRIVATE_RANDOM", i, id((ObjectReference)r), id((ObjectReference)atomic));
                }
            }
            require(named.size() == 96 && privateRandoms.size() == 4, "complete96 owners and four private Randoms");
            String[] roots = client ? new String[] {"ln", "dx", "gk", "ve", "ys", "tc", "db", "wo", "rt"}
                    : new String[] {"hj", "cm", "ei", "nj", "pw", "lz", "by", "oi", "kx"};
            for (String root : roots) statics("REACHED_CLASS", original(root));
            ReferenceType random = bootstrap(vm, "java.util.Random");
            type(random);
            Field unique = field(random, "seedUniquifier", "Ljava/util/concurrent/atomic/AtomicLong;", 26);
            out.add("PROFILE_UNIQUIFIER", value(random.getValue(unique)));
            List<ReferenceType> holders = vm.classesByName("java.lang.Math$RandomNumberGeneratorHolder");
            require(holders.size() <= 1 && (holders.isEmpty() || holders.get(0).classLoader() == null), "bootstrap Math holder binding");
            if (holders.isEmpty()) out.add("MATH_HOLDER", "ABSENT");
            else {
                ReferenceType holder = holders.get(0);
                out.add("MATH_HOLDER", "PRESENT", holder.isPrepared(), holder.isInitialized(), holder.failedToInitialize());
                if (holder.isPrepared()) statics("OBSERVED_MATH_HOLDER", holder);
            }
            List<ThreadReference> threads = vm.allThreads();
            require(threads.size() <= 64, "suspended thread inventory bound");
            for (ThreadReference t : threads) {
                require(t.isSuspended(), "SUSPEND_ALL actual thread status");
                out.add("THREAD", id(t), t.uniqueID(), units(t.name()), t.status(), t.suspendCount());
            }
            require(event.thread().frameCount() <= 128, "complete measured stack bound");
            List<StackFrame> frames = event.thread().frames();
            boolean helperFrame = false;
            for (int i = 0; i < frames.size(); i++) {
                Location at = frames.get(i).location();
                helperFrame |= at.declaringType().name().equals(HELPER) && at.method().name().equals("main");
                out.add("FRAME", i, at.declaringType().name(), at.method().name(), at.method().signature(),
                        at.codeIndex(), id(at.declaringType().classLoader()));
            }
            require(helperFrame, "actual Class.forName call retains helper main frame");
            graph();
            out.add("END", named.size(), privateRandoms.size(), objects.size(), cells, stringUnits, out.count);
        }
    }

    private static final class Session {
        final VirtualMachine vm;
        final boolean client;
        final String expectedHash, expectedUrl;
        final long deadline;
        ReferenceType helper, target;
        Method clinit;
        ClassLoaderReference loader;
        Snapshot snapshot;
        String bytecodeHash;
        int prepared;
        Session(VirtualMachine vm, boolean client, String expectedHash, String expectedUrl, long deadline) {
            this.vm = vm; this.client = client; this.expectedHash = expectedHash;
            this.expectedUrl = expectedUrl; this.deadline = deadline;
            for (String name : new String[] {HELPER, client ? "uu" : "na"}) {
                ClassPrepareRequest request = vm.eventRequestManager().createClassPrepareRequest();
                request.addClassFilter(name);
                request.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                request.enable();
            }
        }
        void prepare(ClassPrepareEvent event) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL, "class prepare suspension");
            ReferenceType type = event.referenceType();
            if (type.name().equals(HELPER)) {
                require(helper == null && type.classLoader() != null, "one application helper class");
                helper = type;
                return;
            }
            require(helper != null && target == null && type.name().equals(client ? "uu" : "na"), "single original target prepare");
            Field held = field(helper, "originalLoader", "Ljava/net/URLClassLoader;", 73);
            Value actual = helper.getValue(held);
            require(actual instanceof ClassLoaderReference && ((ClassLoaderReference)actual).referenceType().name().equals("java.net.URLClassLoader"),
                    "helper's actual isolated URLClassLoader");
            loader = (ClassLoaderReference)actual;
            require(loader.equals(type.classLoader()) && !loader.equals(helper.classLoader()), "actual original loader binding");
            Field hashField = field(helper, "verifiedJarSha256", "Ljava/lang/String;", 73);
            Field urlField = field(helper, "originalJarUrl", "Ljava/lang/String;", 73);
            require(helper.getValue(hashField) instanceof StringReference && helper.getValue(urlField) instanceof StringReference,
                    "helper verified supplied original before loading");
            require(((StringReference)helper.getValue(hashField)).value().equals(expectedHash)
                    && ((StringReference)helper.getValue(urlField)).value().equals(expectedUrl), "helper JAR hash/URL binding");
            List<Method> found = type.methodsByName("<clinit>", "()V");
            require(found.size() == 1 && found.get(0).declaringType().equals(type) && found.get(0).modifiers() == 8,
                    "original declared static initializer descriptor/modifiers");
            clinit = found.get(0);
            byte[] code = clinit.bytecodes();
            int named = client ? 325 : 324, item = client ? 222 : 221;
            require(code.length == 3678 && (code[3395] & 255) == 179 && u16(code, 3396) == named
                    && (code[3398] & 255) == 178 && u16(code, 3399) == item
                    && (code[3401] & 255) == 178 && u16(code, 3402) == (client ? 288 : 287)
                    && (code[3404] & 255) == 180 && u16(code, 3405) == (client ? 327 : 326)
                    && (code[3677] & 255) == 177, "exact source-bound bytecode boundary shape");
            bytecodeHash = hash(code);
            Location location = clinit.locationOfCodeIndex(3398);
            require(location != null && location.codeIndex() == 3398 && location.method().equals(clinit), "exact breakpoint location");
            BreakpointRequest request = vm.eventRequestManager().createBreakpointRequest(location);
            request.setSuspendPolicy(EventRequest.SUSPEND_ALL);
            request.enable();
            target = type;
            prepared++;
        }
        void stop(BreakpointEvent event) throws Exception {
            require(snapshot == null && prepared == 1 && event.request().suspendPolicy() == EventRequest.SUSPEND_ALL
                    && event.location().method().equals(clinit) && event.location().codeIndex() == 3398
                    && event.thread().name().equals("main"), "unique genuine Block prefix breakpoint");
            snapshot = new Snapshot(vm, client, loader, target, deadline);
            snapshot.capture(event, clinit, bytecodeHash, helper);
        }
        private static int u16(byte[] code, int at) { return ((code[at] & 255) << 8) | (code[at + 1] & 255); }
    }

    private static void save(Path output, String leaf, byte[] bytes) throws Exception {
        Files.write(output.resolve(leaf), bytes, StandardOpenOption.CREATE_NEW);
    }

    public static void main(String[] args) {
        Process child = null;
        VirtualMachine vm = null;
        Drain stdout = null, stderr = null;
        Future<Void> stdoutTask = null, stderrTask = null;
        ExecutorService readers = Executors.newFixedThreadPool(2, task -> {
            Thread thread = new Thread(task, "prefix-child-drain"); thread.setDaemon(true); return thread;
        });
        ScheduledExecutorService timer = Executors.newSingleThreadScheduledExecutor(task -> {
            Thread thread = new Thread(task, "prefix-owned-child-watchdog"); thread.setDaemon(true); return thread;
        });
        Watchdog watchdog = null;
        ScheduledFuture<?> watchdogTask = null;
        Path output = null, jar = null;
        String beforeHash = null, afterHash = null;
        String vmName = "", vmVersion = "";
        String exitOutcome = "NOT_REQUESTED";
        List<String> command = new ArrayList<>();
        List<String> failures = new ArrayList<>();
        Session session = null;
        boolean requestedExit = false, reaped = false, complete = false;
        int childExit = Integer.MIN_VALUE;
        long pid = -1, start = System.nanoTime(), deadline = start + TimeUnit.SECONDS.toNanos(45);
        try {
            require(Runtime.version().feature() == 17, "bounded installed Java17 observation profile");
            require(args.length == 3 && (args[0].equals("client") || args[0].equals("server")),
                    "Usage: BlockRegistryPrefixController client|server jar fresh-output-directory");
            boolean client = args[0].equals("client");
            jar = Path.of(args[1]).toRealPath();
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(Files.size(jar) == (client ? 1465375L : 503100L), "original JAR size");
            beforeHash = hash(Files.readAllBytes(jar));
            require(beforeHash.equals(expected), "original JAR SHA-256");
            Path wanted = Path.of(args[2]).toAbsolutePath().normalize();
            require(wanted.getParent() != null && Files.isDirectory(wanted.getParent(), LinkOption.NOFOLLOW_LINKS)
                    && !Files.exists(wanted, LinkOption.NOFOLLOW_LINKS), "fresh exclusively owned output directory");
            output = Files.createDirectory(wanted);
            Path java = Path.of(System.getProperty("java.home"), "bin",
                    System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            command.addAll(List.of(java.toString(), "-Xint", "-Xmx256m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                    "-cp", System.getProperty("java.class.path"), HELPER, args[0], jar.toString()));
            child = new ProcessBuilder(command).start();
            pid = child.pid();
            watchdog = new Watchdog(child);
            watchdogTask = timer.schedule(watchdog, Math.max(0L, deadline - System.nanoTime()), TimeUnit.NANOSECONDS);
            AtomicLong combined = new AtomicLong();
            stdout = new Drain(child.getInputStream(), combined, true);
            stderr = new Drain(child.getErrorStream(), combined, false);
            stdoutTask = readers.submit(stdout);
            stderrTask = readers.submit(stderr);
            Rows started = new Rows();
            started.add("BLOCK_PREFIX_CHILD_STARTED_V1", pid, false);
            for (int i = 0; i < command.size(); i++) started.add("COMMAND_UTF16", i, units(command.get(i)));
            save(output, "child.started.tsv", started.bytes());
            child.getOutputStream().close();
            String listen = stdout.listening.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(listen.matches("Listening for transport dt_socket at address: [0-9]{1,5}"), "literal JDWP startup line");
            int port = Integer.parseInt(listen.substring(listen.lastIndexOf(' ') + 1));
            require(port > 0 && port <= 65535, "loopback JDWP port");
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(c -> c.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments();
            connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(Integer.toString(port));
            connection.get("timeout").setValue(Long.toString(Math.min(10000, remaining(deadline))));
            vm = connector.attach(connection);
            require(vm.canGetBytecodes() && vm.canBeModified(), "JDI bytecode/breakpoint capabilities");
            vmName = vm.name();
            vmVersion = vm.version();
            session = new Session(vm, client, expected, jar.toUri().toURL().toExternalForm(), deadline);
            vm.resume();
            while (!complete) {
                require(stdout.failure == null && stderr.failure == null, "bounded child stream readers");
                EventSet set = vm.eventQueue().remove(Math.min(500, remaining(deadline)));
                if (set == null) continue;
                for (Event event : set) {
                    if (event instanceof ClassPrepareEvent) session.prepare((ClassPrepareEvent)event);
                    else if (event instanceof BreakpointEvent) {
                        session.stop((BreakpointEvent)event);
                        complete = true;
                    } else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) {
                        throw new IllegalStateException("original unavailable before accepted prefix; VM ended");
                    }
                }
                if (!complete) set.resume();
            }
            requestedExit = true;
            exitOutcome = "REQUEST_SENT";
            try { vm.exit(0); exitOutcome = "RETURNED"; }
            catch (VMDisconnectedException disconnected) { exitOutcome = "DISCONNECTED"; }
        } catch (Throwable failure) {
            failures.add(failure.getClass().getName() + ": " + failure.getMessage());
            failure.printStackTrace(System.err);
        } finally {
            if (child != null) {
                /* Do not detach/resume an unsuccessful suspended original. */
                if (requestedExit) try {
                    if (child.waitFor(2, TimeUnit.SECONDS)) reaped = true;
                } catch (Throwable failure) {
                    failures.add("requested-exit wait: " + failure);
                }
                try {
                    if (child.isAlive()) child.destroyForcibly();
                } catch (Throwable failure) {
                    failures.add("terminate: " + failure);
                } finally {
                    try {
                        if (child.waitFor(5, TimeUnit.SECONDS)) { reaped = true; childExit = child.exitValue(); }
                        else failures.add("cleanup wait did not reap child");
                    } catch (Throwable failure) {
                        failures.add("mandatory wait: " + failure);
                        if (failure instanceof InterruptedException) Thread.currentThread().interrupt();
                    }
                }
            }
            if (watchdogTask != null) {
                watchdogTask.cancel(false);
            }
            /* A successfully cancelled Future may still have a running body.
             * Join executor termination, without interrupting its mandatory wait. */
            timer.shutdown();
            try {
                if (!timer.awaitTermination(6, TimeUnit.SECONDS))
                    failures.add("watchdog executor did not terminate");
            } catch (Throwable failure) {
                failures.add("watchdog executor join: " + failure);
                if (failure instanceof InterruptedException) Thread.currentThread().interrupt();
            }
            if (watchdog != null) {
                if (watchdog.fired) failures.add("45-second child watchdog fired");
                if (watchdog.failure != null) failures.add(watchdog.failure);
            }
            if (reaped && vm != null) try { vm.dispose(); }
            catch (RuntimeException ignored) { /* Reaped OS child is already terminal. */ }
            for (Future<Void> task : Arrays.asList(stdoutTask, stderrTask)) if (task != null) {
                try { task.get(2, TimeUnit.SECONDS); }
                catch (Throwable failure) { failures.add("drain join: " + failure); }
            }
            if (stdout != null && stdout.failure != null) failures.add("stdout: " + stdout.failure);
            if (stderr != null && stderr.failure != null) failures.add("stderr: " + stderr.failure);
            if (child != null && (stdoutTask == null || stderrTask == null)) failures.add("child stream capture setup incomplete");
            readers.shutdownNow();
            if (jar != null) try { afterHash = hash(Files.readAllBytes(jar)); }
            catch (Throwable failure) { failures.add("after hash: " + failure); }
            if (beforeHash == null || !beforeHash.equals(afterHash)) failures.add("original JAR before/after mismatch");
            if (!(complete && requestedExit && reaped && childExit == 0)) failures.add("prefix/requested-exit/reap success gate not met");
            if (output != null) {
                byte[] rawOut = stdout == null ? new byte[0] : stdout.bytes();
                byte[] rawErr = stderr == null ? new byte[0] : stderr.bytes();
                try { save(output, "child-stdout.bin", rawOut); }
                catch (Throwable failure) { failures.add("stdout save: " + failure); }
                try { save(output, "child-stderr.bin", rawErr); }
                catch (Throwable failure) { failures.add("stderr save: " + failure); }
                if (session != null && session.snapshot != null) try {
                    save(output, "snapshot.raw.tsv", session.snapshot.out.bytes());
                } catch (Throwable failure) { failures.add("snapshot save: " + failure); }
                try {
                    Rows receipt = new Rows();
                    receipt.add("BLOCK_PREFIX_PROCESS_V1");
                    receipt.add("STATUS", failures.isEmpty() ? "PASS" : "FAIL");
                    receipt.add("POLICY", "REQUESTED_DEBUGGER_TERMINATION", "NOT_NATURAL_ORIGINAL_COMPLETION", "NOT_SUCCESSFUL_BOOTSTRAP");
                    receipt.add("CHILD", pid, reaped, childExit, requestedExit, complete);
                    receipt.add("DEBUGGER_EXIT_OUTCOME", exitOutcome);
                    for (int i = 0; i < command.size(); i++) receipt.add("COMMAND_UTF16", i, units(command.get(i)));
                    receipt.add("JAR_SHA256", beforeHash == null ? "-" : beforeHash, afterHash == null ? "-" : afterHash);
                    receipt.add("VM_NAME_UTF16", units(vmName));
                    receipt.add("VM_VERSION_UTF16", units(vmVersion));
                    receipt.add("STDOUT", rawOut.length, hash(rawOut),
                            stdoutTask != null && stdoutTask.isDone() && stdout != null && stdout.failure == null);
                    receipt.add("STDERR", rawErr.length, hash(rawErr),
                            stderrTask != null && stderrTask.isDone() && stderr != null && stderr.failure == null);
                    if (session != null && session.snapshot != null) {
                        byte[] rawSnapshot = session.snapshot.out.bytes();
                        receipt.add("SNAPSHOT_RAW", rawSnapshot.length, hash(rawSnapshot), complete);
                    }
                    receipt.add("OBSERVATION_DEADLINE_SECONDS", 45);
                    receipt.add("WATCHDOG", watchdog != null && watchdog.fired,
                            watchdog != null && watchdog.reaped);
                    receipt.add("ELAPSED_MS", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - start));
                    for (String failure : failures) receipt.add("FAILURE_UTF16", units(failure));
                    receipt.add("END", receipt.count);
                    save(output, "receipt.tsv", receipt.bytes());
                } catch (Throwable failure) {
                    failures.add("receipt save: " + failure);
                    failure.printStackTrace(System.err);
                }
            }
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
