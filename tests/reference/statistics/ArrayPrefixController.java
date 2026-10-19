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

/** Reads numeric identities at an untouched original Stats boundary. */
public final class ArrayPrefixController {
    private static final String HELPER = "ArrayPrefixObservation";
    private static final int ACTIVE_SECONDS = 40;
    private static final int STREAM_LIMIT = 16 * 1024 * 1024;
    private static final int MAX_STATS = 65536, MAX_HELD = 262144, MAX_TABLE = 131072;
    private static final int MAX_ARRAY = 32000, MAX_LIST = 131072;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static String units(String value) {
        require(value.length() <= 65536, "receipt UTF16 bound");
        if (value.isEmpty()) return "-";
        StringBuilder text = new StringBuilder(value.length() * 4);
        for (int index = 0; index < value.length(); index++) {
            String hex = Integer.toHexString(value.charAt(index));
            text.append("0".repeat(4 - hex.length())).append(hex);
        }
        return text.toString();
    }

    private static final class Rows {
        final StringBuilder text = new StringBuilder();
        int count;
        void add(Object... columns) {
            require(++count <= 700000, "row transport bound");
            for (int index = 0; index < columns.length; index++) {
                if (index != 0) text.append('\t');
                String value = String.valueOf(columns[index]);
                for (int at = 0; at < value.length(); at++)
                    require(value.charAt(at) >= 32 && value.charAt(at) <= 126, "ASCII columns");
                text.append(value);
            }
            text.append('\n');
            require(text.length() <= 32 * 1024 * 1024, "text transport bound");
        }
        byte[] bytes() { return text.toString().getBytes(StandardCharsets.US_ASCII); }
    }

    private static long remaining(long deadline) {
        long millis = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(millis > 0, "active observation deadline");
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
                int count;
                while ((count = stream.read(buffer)) != -1) {
                    long before = total.getAndAdd(count);
                    int keep = (int)Math.max(0L, Math.min((long)count, STREAM_LIMIT - before));
                    synchronized (retained) { retained.write(buffer, 0, keep); }
                    if (keep != count && failure == null)
                        failure = new IllegalStateException("combined child stream cap exceeded");
                    if (line != null) {
                        for (int index = 0; index < count; index++) {
                            if (buffer[index] == '\n') {
                                String value = line.toString(StandardCharsets.US_ASCII);
                                if (value.endsWith("\r")) value = value.substring(0, value.length() - 1);
                                listening.complete(value);
                                line = null;
                                break;
                            }
                            require(line.size() < 1024, "JDWP startup line bound");
                            line.write(buffer[index]);
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

    private static Field field(ReferenceType type, String name, String signature, int sourceModifiers) {
        List<Field> fields = type.fields().stream().filter(value -> value.name().equals(name)).toList();
        // The pinned JDI profile reports exactly 0x800 in addition to source access flags
        // for fields with a generic signature; this was measured on two independent fields.
        int expectedModifiers = sourceModifiers;
        if (fields.size() == 1 && fields.get(0).genericSignature() != null) expectedModifiers += 0x800;
        if (!(fields.size() == 1 && fields.get(0).declaringType().equals(type)
                && fields.get(0).signature().equals(signature) && fields.get(0).modifiers() == expectedModifiers)) {
            StringBuilder actual = new StringBuilder();
            for (Field found : fields) {
                actual.append('[').append(found.declaringType().name()).append('.').append(found.name())
                        .append(" signature=").append(found.signature())
                        .append(" genericSignature=").append(found.genericSignature())
                        .append(" modifiers=").append(found.modifiers()).append(']');
            }
            throw new IllegalStateException("exact declared field " + type.name() + "." + name + signature
                    + " sourceModifiers=" + sourceModifiers + " expectedModifiers=" + expectedModifiers
                    + " actual=" + actual);
        }
        return fields.get(0);
    }

    private static int integer(Value value) {
        require(value instanceof IntegerValue, "int field value");
        return ((IntegerValue)value).value();
    }

    private static int bool(Value value) {
        require(value instanceof BooleanValue, "boolean field value");
        return ((BooleanValue)value).value() ? 1 : 0;
    }

    private static final class Stat {
        final ObjectReference object;
        final int id, independent;
        final String kind, formatter, associated;
        Stat(ObjectReference object, int id, int independent, String kind, String formatter, String associated) {
            this.object = object; this.id = id; this.independent = independent;
            this.kind = kind; this.formatter = formatter; this.associated = associated;
        }
    }

    private static final class Snapshot {
        final VirtualMachine vm;
        final boolean client;
        final ClassLoaderReference loader;
        final ReferenceType statistics, base, item;
        final long deadline;
        final Rows out = new Rows(), binding = new Rows();
        final Map<Long, ObjectReference> held = new LinkedHashMap<>();
        final TreeMap<Integer, Stat> stats = new TreeMap<>();
        final Map<Long, Integer> ids = new HashMap<>();
        final ObjectReference[] strategies = new ObjectReference[3];
        final Field id, name, independent, key, formatter, associated;
        boolean released;
        Snapshot(VirtualMachine vm, boolean client, ClassLoaderReference loader,
                ReferenceType statistics, long deadline) {
            this.vm = vm; this.client = client; this.loader = loader;
            this.statistics = statistics; this.deadline = deadline;
            base = original(client ? "vr" : "ns");
            item = original(client ? "tw" : "mm");
            id = field(base, "e", "I", 17);
            name = field(base, "f", "Ljava/lang/String;", 17);
            independent = field(base, "g", "Z", 1);
            key = field(base, "h", "Ljava/lang/String;", 1);
            formatter = field(base, "a", client ? "Lwk;" : "Loe;", 18);
            associated = field(item, "a", "I", 18);
        }
        ReferenceType original(String name) {
            remaining(deadline);
            List<ReferenceType> found = vm.classesByName(name).stream()
                    .filter(type -> loader.equals(type.classLoader())).toList();
            require(found.size() == 1 && found.get(0).isPrepared(), "one prepared original " + name);
            return found.get(0);
        }
        ReferenceType bootstrap(String name) {
            remaining(deadline);
            List<ReferenceType> found = vm.classesByName(name).stream()
                    .filter(type -> type.classLoader() == null).toList();
            require(found.size() == 1 && found.get(0).isPrepared(), "one prepared bootstrap " + name);
            return found.get(0);
        }
        ObjectReference hold(Value value) {
            require(value instanceof ObjectReference, "nonnull object reference");
            ObjectReference object = (ObjectReference)value;
            if (!held.containsKey(object.uniqueID())) {
                remaining(deadline);
                require(held.size() < MAX_HELD, "bounded retained JDI mirrors");
                object.disableCollection();
                held.put(object.uniqueID(), object);
            }
            return object;
        }
        void stringOrNull(Value value) {
            require(value == null || (value instanceof StringReference
                    && ((StringReference)value).referenceType().classLoader() == null),
                    "actual String/null field");
            if (value != null) hold(value); // Contents deliberately remain unread.
        }
        Stat readStat(ObjectReference object) {
            hold(object);
            ReferenceType type = object.referenceType();
            require(loader.equals(type.classLoader()), "original statistic loader");
            String kind = type.name().equals(client ? "yd" : "pm") ? "general"
                    : type.name().equals(client ? "tw" : "mm") ? "item" : "unsupported";
            require(!kind.equals("unsupported"), "exact General/Item statistic concrete class");
            Map<Field, Value> fields = object.getValues(List.of(id, name, independent, key, formatter));
            stringOrNull(fields.get(name));
            stringOrNull(fields.get(key));
            ObjectReference selected = hold(fields.get(formatter));
            String format = null;
            for (int index = 0; index < strategies.length; index++)
                if (selected.equals(strategies[index])) format = String.valueOf((char)('i' + index));
            require(format != null, "actual formatting strategy identity");
            String extra = kind.equals("item") ? Integer.toString(integer(object.getValue(associated))) : "NULL";
            return new Stat(object, integer(fields.get(id)), bool(fields.get(independent)), kind, format, extra);
        }
        void map() {
            ObjectReference map = hold(statistics.getValue(field(statistics, "a", "Ljava/util/Map;", 12)));
            ReferenceType type = bootstrap("java.util.HashMap");
            require(map.referenceType().equals(type), "actual HashMap owner");
            int size = integer(map.getValue(field(type, "size", "I", 128)));
            require(size >= 0 && size <= MAX_STATS, "map size transport bound");
            Value tableValue = map.getValue(field(type, "table", "[Ljava/util/HashMap$Node;", 128));
            Set<Long> nodes = new HashSet<>();
            if (tableValue != null) {
                require(tableValue instanceof ArrayReference, "actual HashMap table array");
                ArrayReference table = (ArrayReference)hold(tableValue);
                require(table.length() <= MAX_TABLE, "HashMap table transport bound");
                ReferenceType node = bootstrap("java.util.HashMap$Node");
                ReferenceType boxed = bootstrap("java.lang.Integer");
                Field nodeKey = field(node, "key", "Ljava/lang/Object;", 16);
                Field nodeValue = field(node, "value", "Ljava/lang/Object;", 0);
                Field next = field(node, "next", "Ljava/util/HashMap$Node;", 0);
                Field number = field(boxed, "value", "I", 18);
                for (Value first : table.getValues()) {
                    Value cursor = first;
                    while (cursor != null) {
                        remaining(deadline);
                        ObjectReference entry = hold(cursor);
                        require(entry.referenceType().classLoader() == null
                                && (entry.referenceType().name().equals("java.util.HashMap$Node")
                                    || entry.referenceType().name().equals("java.util.HashMap$TreeNode"))
                                && nodes.add(entry.uniqueID()) && nodes.size() <= MAX_STATS,
                                "complete acyclic HashMap bin links");
                        Map<Field, Value> fields = entry.getValues(List.of(nodeKey, nodeValue, next));
                        ObjectReference integerKey = hold(fields.get(nodeKey));
                        require(integerKey.referenceType().equals(boxed), "Integer map key");
                        int numeric = integer(integerKey.getValue(number));
                        Stat statistic = readStat(hold(fields.get(nodeValue)));
                        require(numeric == statistic.id && !stats.containsKey(numeric)
                                && !ids.containsKey(statistic.object.uniqueID()), "ID/object bijection and map key equality");
                        stats.put(numeric, statistic);
                        ids.put(statistic.object.uniqueID(), numeric);
                        cursor = fields.get(next);
                    }
                }
            }
            require(stats.size() == size && nodes.size() == size, "complete actual map traversal");
            out.add("MAP", size);
            for (Stat statistic : stats.values())
                out.add("MAPSTAT", statistic.id, statistic.kind, statistic.independent,
                        statistic.formatter, statistic.associated);
        }
        String sid(Value value, boolean nullable) {
            if (value == null) {
                require(nullable, "nonnull named statistic");
                return "NULL";
            }
            ObjectReference object = hold(value);
            Integer numeric = ids.get(object.uniqueID());
            require(numeric != null && stats.get(numeric).object.equals(object),
                    "reference points to same actual registered owner");
            return numeric.toString();
        }
        void array(String label) {
            Value value = statistics.getValue(field(statistics, label,
                    client ? "[Lvr;" : "[Lns;", 9));
            int populated = 0;
            if (value == null) out.add("ARRAY", label, "NULL");
            else {
                require(value instanceof ArrayReference, "statistic array reference");
                ArrayReference array = (ArrayReference)hold(value);
                require(loader.equals(array.referenceType().classLoader()) && array.length() <= MAX_ARRAY,
                        "original full array transport bound");
                out.add("ARRAY", label, array.length());
                List<Value> values = array.getValues();
                require(values.size() == array.length(), "full actual stat array read");
                for (int index = 0; index < values.size(); index++) {
                    remaining(deadline);
                    if (values.get(index) != null) {
                        out.add("SLOT", label, index, sid(values.get(index), false));
                        populated++;
                    }
                }
            }
            out.add("END_ARRAY", label, populated);
        }
        void list(String label) {
            ObjectReference list = hold(statistics.getValue(field(statistics, label, "Ljava/util/List;", 9)));
            ReferenceType type = bootstrap("java.util.ArrayList");
            require(list.referenceType().equals(type), "actual ArrayList owner");
            int size = integer(list.getValue(field(type, "size", "I", 2)));
            ArrayReference data = (ArrayReference)hold(list.getValue(field(type, "elementData", "[Ljava/lang/Object;", 128)));
            require(size >= 0 && size <= MAX_LIST && size <= data.length(), "actual logical ArrayList prefix");
            out.add("LIST", label, size);
            List<Value> values = size == 0 ? List.of() : data.getValues(0, size);
            require(values.size() == size, "complete logical ArrayList read");
            for (int index = 0; index < size; index++) {
                remaining(deadline);
                out.add("ELEMENT", label, index, sid(values.get(index), true));
            }
        }
        void capture(String side, String entry) {
            String[] strategyClasses = client ? new String[] {"bq", "bo", "bn"}
                    : new String[] {"ax", "av", "at"};
            for (int index = 0; index < strategies.length; index++) {
                String label = String.valueOf((char)('i' + index));
                strategies[index] = hold(base.getValue(field(base, label, client ? "Lwk;" : "Loe;", 9)));
                require(loader.equals(strategies[index].referenceType().classLoader())
                        && strategies[index].referenceType().name().equals(strategyClasses[index]),
                        "original strategy loader and concrete class");
                for (int prior = 0; prior < index; prior++)
                    require(!strategies[prior].equals(strategies[index]), "distinct strategy owners");
            }
            out.add("STAT_ARRAY_PREFIX_V1", side, entry, entry.equals("stats") ? "d21" : "clinit624");
            out.add("STATE", bool(statistics.getValue(field(statistics, "G", "Z", 10))),
                    bool(statistics.getValue(field(statistics, "H", "Z", 10))));
            map();
            for (int index = 0; index < 23; index++) {
                String label = String.valueOf((char)(index < 21 ? 'f' + index : 'A' + index - 21));
                out.add("NAMED", index, sid(statistics.getValue(field(statistics, label,
                        client ? "Lvr;" : "Lns;", 9)), false));
            }
            for (String label : new String[] {"C", "D", "E", "F"}) array(label);
            for (String label : new String[] {"b", "c", "d", "e"}) list(label);
            out.add("END");
        }
        void release() {
            if (released) return;
            RuntimeException failure = null;
            for (ObjectReference object : held.values()) {
                try { object.enableCollection(); }
                catch (RuntimeException caught) {
                    if (failure == null) failure = caught;
                    if (caught instanceof VMDisconnectedException) break;
                }
            }
            released = failure == null;
            if (failure != null) throw failure;
        }
    }

    private static final class Session {
        final VirtualMachine vm;
        final String side, entry, mode, expectedHash, expectedUrl;
        final boolean client;
        final long deadline;
        ReferenceType helper, target;
        ClassLoaderReference loader;
        Method method;
        BreakpointRequest breakpoint;
        Snapshot snapshot;
        long threadId;
        String codeHash;
        Session(VirtualMachine vm, String side, String entry, String mode, String expectedHash,
                String expectedUrl, long deadline) {
            this.vm = vm; this.side = side; this.entry = entry; this.mode = mode;
            this.client = side.equals("client");
            this.expectedHash = expectedHash; this.expectedUrl = expectedUrl; this.deadline = deadline;
            for (String name : new String[] {HELPER, client ? "jl" : "gg"}) {
                ClassPrepareRequest request = vm.eventRequestManager().createClassPrepareRequest();
                request.addClassFilter(name);
                request.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                request.enable();
            }
        }
        String helperString(String name) {
            Value value = helper.getValue(field(helper, name, "Ljava/lang/String;", 73));
            require(value instanceof StringReference, "helper String binding");
            return ((StringReference)value).value();
        }
        String trigger() {
            return entry.equals("stats") ? (client ? "jl" : "gg")
                    : entry.equals("block") ? (client ? "uu" : "na") : (client ? "gm" : "ej");
        }
        void prepare(ClassPrepareEvent event) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL, "prepare SUSPEND_ALL");
            ReferenceType type = event.referenceType();
            if (type.name().equals(HELPER)) {
                require(helper == null && type.classLoader() != null, "one actual helper");
                helper = type;
                return;
            }
            require(helper != null && target == null && type.name().equals(client ? "jl" : "gg")
                    && !type.isInitialized() && !type.failedToInitialize(), "original target before initialization");
            Value value = helper.getValue(field(helper, "originalLoader", "Ljava/net/URLClassLoader;", 73));
            require(value instanceof ClassLoaderReference, "actual helper-held URLClassLoader");
            loader = (ClassLoaderReference)value;
            require(loader.equals(type.classLoader()) && !loader.equals(helper.classLoader())
                    && helperString("verifiedJarSha256").equals(expectedHash)
                    && helperString("originalJarUrl").equals(expectedUrl)
                    && helperString("originalEntry").equals(entry)
                    && helperString("originalTrigger").equals(trigger()), "original loader/hash/entry binding");
            boolean statsEntry = entry.equals("stats");
            String methodName = statsEntry ? "d" : "<clinit>";
            List<Method> methods = type.methodsByName(methodName, "()V");
            require(methods.size() == 1 && methods.get(0).declaringType().equals(type)
                    && methods.get(0).modifiers() == (statsEntry ? 9 : 8), "actual boundary method descriptor");
            method = methods.get(0);
            byte[] code = method.bytecodes();
            if (statsEntry) {
                require(code.length == 231 && (code[0] & 255) == 178 && u16(code, 1) == 63
                        && (code[6] & 255) == 178 && u16(code, 7) == 64
                        && (code[13] & 255) == 187 && u16(code, 14) == (client ? 44 : 45)
                        && (code[17] & 255) == 183 && u16(code, 18) == (client ? 131 : 137)
                        && (code[20] & 255) == 75 && (code[21] & 255) == 184 && u16(code, 22) == 124
                        && (code[24] & 255) == 182 && u16(code, 25) == 125 && (code[230] & 255) == 177,
                        "source-bound d21 bytecode shape");
            } else {
                require(code.length == 636 && (code[618] & 255) == 184
                        && u16(code, 619) == (client ? 134 : 126)
                        && (code[621] & 255) == 179 && u16(code, 622) == 59
                        && (code[624] & 255) == 184 && u16(code, 625) == 119
                        && (code[628] & 255) == 179 && u16(code, 629) == 63
                        && (code[632] & 255) == 179 && u16(code, 633) == 64 && (code[635] & 255) == 177,
                        "source-bound clinit624 bytecode shape");
            }
            codeHash = hash(code);
            Location location = method.locationOfCodeIndex(statsEntry ? 21 : 624);
            require(location != null && location.method().equals(method)
                    && location.codeIndex() == (statsEntry ? 21 : 624), "exact source code index");
            breakpoint = vm.eventRequestManager().createBreakpointRequest(location);
            breakpoint.setSuspendPolicy(EventRequest.SUSPEND_ALL);
            breakpoint.enable();
            target = type;
            threadId = event.thread().uniqueID();
        }
        void stop(BreakpointEvent event) {
            require(snapshot == null && target != null && event.request().equals(breakpoint)
                    && event.request().suspendPolicy() == EventRequest.SUSPEND_ALL
                    && event.location().declaringType().equals(target)
                    && event.location().method().equals(method)
                    && event.location().codeIndex() == (entry.equals("stats") ? 21 : 624)
                    && event.thread().uniqueID() == threadId && event.thread().name().equals("main"),
                    "unique exact original breakpoint/thread");
            Field originalTarget = field(helper, "originalTarget", "Ljava/lang/Class;", 73);
            require("Ljava/lang/Class<*>;".equals(originalTarget.genericSignature()),
                    "exact helper target generic signature");
            Value mirror = helper.getValue(originalTarget);
            require(mirror instanceof ClassObjectReference
                    && ((ClassObjectReference)mirror).reflectedType().equals(target), "helper's preloaded target identity");
            List<ThreadReference> threads = vm.allThreads();
            require(threads.size() <= 128, "thread inventory transport bound");
            for (ThreadReference thread : threads) require(thread.isSuspended(), "actual all-thread suspension");
            snapshot = new Snapshot(vm, client, loader, target, deadline);
            snapshot.binding.add("STAT_ARRAY_BINDING_V1", side, entry);
            snapshot.binding.add("MODE", mode);
            snapshot.binding.add("BOUNDARY", target.name(), method.name(), method.signature(),
                    event.location().codeIndex(), codeHash, loader.uniqueID(), event.thread().uniqueID());
            try {
                require(event.thread().frameCount() <= 128, "complete stack transport bound");
                List<StackFrame> frames = event.thread().frames();
                require(!frames.isEmpty() && frames.get(0).location().method().equals(method)
                        && frames.get(0).location().codeIndex() == event.location().codeIndex(),
                        "actual suspended top frame equals boundary event");
                boolean helperFrame = false, triggerFrame = false;
                for (int index = 0; index < frames.size(); index++) {
                    Location at = frames.get(index).location();
                    helperFrame |= at.declaringType().equals(helper) && at.method().name().equals("main");
                    triggerFrame |= at.declaringType().name().equals(trigger())
                            && loader.equals(at.declaringType().classLoader()) && at.method().name().equals("<clinit>");
                    snapshot.binding.add("FRAME", index, at.declaringType().name(), at.method().name(),
                            at.method().signature(), at.codeIndex(),
                            at.declaringType().classLoader() == null ? "BOOT" : at.declaringType().classLoader().uniqueID());
                }
                require(helperFrame && triggerFrame, "real helper call and initiating constructor stack retained");
            } catch (IncompatibleThreadStateException failure) {
                throw new IllegalStateException("suspended stack unavailable", failure);
            }
            snapshot.capture(side, entry);
            snapshot.binding.add("MIRRORS", snapshot.held.size());
            snapshot.binding.add("END");
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
            Thread thread = new Thread(task, "stat-array-drain"); thread.setDaemon(true); return thread;
        });
        ScheduledExecutorService timer = Executors.newSingleThreadScheduledExecutor(task -> {
            Thread thread = new Thread(task, "stat-array-child-watchdog"); thread.setDaemon(true); return thread;
        });
        Watchdog watchdog = null;
        ScheduledFuture<?> watchdogTask = null;
        Path output = null, jar = null;
        String before = null, after = null, vmName = "", vmVersion = "", exitOutcome = "NOT_REQUESTED";
        String mode = "interpreted";
        List<String> command = new ArrayList<>(), failures = new ArrayList<>();
        Session session = null;
        boolean complete = false, requestedExit = false, reaped = false;
        int requestedCode = -1, childExit = Integer.MIN_VALUE;
        long pid = -1, start = System.nanoTime(), deadline = start + TimeUnit.SECONDS.toNanos(ACTIVE_SECONDS);
        try {
            require(Runtime.version().feature() == 17, "captured Java17 observation profile");
            require((args.length == 4 || args.length == 5) && (args[0].equals("client") || args[0].equals("server"))
                    && (args[1].equals("stats") || args[1].equals("block") || args[1].equals("item")),
                    "Usage: ArrayPrefixController client|server stats|block|item jar fresh-output-directory [interpreted|tier1|default]");
            mode = args.length == 5 ? args[4] : "interpreted";
            require(mode.equals("interpreted") || mode.equals("tier1") || mode.equals("default"),
                    "requested JVM mode");
            boolean client = args[0].equals("client");
            jar = Path.of(args[2]).toRealPath();
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(Files.size(jar) == (client ? 1465375L : 503100L), "original JAR size");
            before = hash(Files.readAllBytes(jar));
            require(before.equals(expected), "original JAR SHA-256");
            Path wanted = Path.of(args[3]).toAbsolutePath().normalize();
            require(wanted.getParent() != null && Files.isDirectory(wanted.getParent(), LinkOption.NOFOLLOW_LINKS)
                    && !Files.exists(wanted, LinkOption.NOFOLLOW_LINKS), "fresh exclusively owned output");
            output = Files.createDirectory(wanted);
            Path java = Path.of(System.getProperty("java.home"), "bin",
                    System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            command.add(java.toString());
            if (mode.equals("interpreted")) command.add("-Xint");
            else if (mode.equals("tier1")) command.add("-XX:TieredStopAtLevel=1");
            command.addAll(List.of("-Xmx256m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                    "-cp", System.getProperty("java.class.path"), HELPER, args[0], args[1], jar.toString()));
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
            started.add("STAT_ARRAY_CHILD_STARTED_V1", pid, false);
            started.add("MODE", mode);
            for (int index = 0; index < command.size(); index++) started.add("COMMAND_UTF16", index, units(command.get(index)));
            save(output, "child.started.tsv", started.bytes());
            child.getOutputStream().close();
            String listen = stdout.listening.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(listen.matches("Listening for transport dt_socket at address: [0-9]{1,5}"), "literal JDWP startup");
            int port = Integer.parseInt(listen.substring(listen.lastIndexOf(' ') + 1));
            require(port > 0 && port <= 65535, "loopback port");
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments();
            connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(Integer.toString(port));
            connection.get("timeout").setValue(Long.toString(Math.min(10000, remaining(deadline))));
            vm = connector.attach(connection);
            require(vm.canGetBytecodes() && vm.canBeModified(), "JDI breakpoint/bytecode capabilities");
            vmName = vm.name(); vmVersion = vm.version();
            session = new Session(vm, args[0], args[1], mode, expected, jar.toUri().toURL().toExternalForm(), deadline);
            vm.resume();
            while (!complete) {
                require(stdout.failure == null && stderr.failure == null, "bounded stream capture");
                EventSet set = vm.eventQueue().remove(Math.min(250, remaining(deadline)));
                if (set == null) continue;
                boolean boundary = false, eventFailed = false;
                try {
                    for (Event event : set) {
                        if (event instanceof ClassPrepareEvent) session.prepare((ClassPrepareEvent)event);
                        else if (event instanceof BreakpointEvent) {
                            boundary = true;
                            session.stop((BreakpointEvent)event);
                            complete = true;
                        } else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent)
                            throw new IllegalStateException("original unavailable before accepted prefix");
                    }
                } catch (Throwable failure) {
                    eventFailed = true;
                    throw failure;
                } finally {
                    if (boundary || eventFailed) {
                        try {
                            if (session.snapshot != null) session.snapshot.release();
                        } catch (Throwable failure) {
                            eventFailed = true; complete = false;
                            failures.add("mirror release: " + failure);
                        }
                        requestedExit = true;
                        requestedCode = complete && !eventFailed ? 0 : 1;
                        exitOutcome = "REQUEST_SENT";
                        try { vm.exit(requestedCode); exitOutcome = "RETURNED"; }
                        catch (VMDisconnectedException disconnected) { exitOutcome = "DISCONNECTED"; }
                    } else {
                        set.resume();
                    }
                }
            }
        } catch (Throwable failure) {
            failures.add(failure.getClass().getName() + ": " + failure.getMessage());
            failure.printStackTrace(System.err);
        } finally {
            if (child != null) {
                if (requestedExit) try { if (child.waitFor(2, TimeUnit.SECONDS)) reaped = true; }
                catch (Throwable failure) { failures.add("requested-exit wait: " + failure); }
                try { if (child.isAlive()) child.destroyForcibly(); }
                catch (Throwable failure) { failures.add("terminate: " + failure); }
                finally {
                    try {
                        if (child.waitFor(5, TimeUnit.SECONDS)) { reaped = true; childExit = child.exitValue(); }
                        else failures.add("mandatory cleanup wait did not reap child");
                    } catch (Throwable failure) {
                        failures.add("mandatory wait: " + failure);
                        if (failure instanceof InterruptedException) Thread.currentThread().interrupt();
                    }
                }
            }
            if (watchdogTask != null) watchdogTask.cancel(false);
            timer.shutdown();
            try { if (!timer.awaitTermination(6, TimeUnit.SECONDS)) failures.add("watchdog executor did not terminate"); }
            catch (Throwable failure) {
                failures.add("watchdog join: " + failure);
                if (failure instanceof InterruptedException) Thread.currentThread().interrupt();
            }
            if (watchdog != null) {
                if (watchdog.fired) failures.add("active child watchdog fired");
                if (watchdog.failure != null) failures.add(watchdog.failure);
            }
            if (reaped && vm != null) try { vm.dispose(); }
            catch (RuntimeException ignored) { /* Actual OS child is already terminal. */ }
            for (Future<Void> task : Arrays.asList(stdoutTask, stderrTask)) if (task != null) {
                try { task.get(2, TimeUnit.SECONDS); }
                catch (Throwable failure) { failures.add("drain join: " + failure); }
            }
            if (stdout != null && stdout.failure != null) failures.add("stdout: " + stdout.failure);
            if (stderr != null && stderr.failure != null) failures.add("stderr: " + stderr.failure);
            if (child != null && (stdoutTask == null || stderrTask == null)) failures.add("incomplete stream setup");
            readers.shutdownNow();
            if (jar != null) try { after = hash(Files.readAllBytes(jar)); }
            catch (Throwable failure) { failures.add("after JAR hash: " + failure); }
            if (before == null || !before.equals(after)) failures.add("original JAR before/after mismatch");
            if (!(complete && requestedExit && requestedCode == 0 && reaped && childExit == 0
                    && session != null && session.snapshot != null && session.snapshot.released))
                failures.add("actual prefix/zero requested exit/reap/mirror-release gate not met");
            if (output != null) {
                byte[] rawOut = stdout == null ? new byte[0] : stdout.bytes();
                byte[] rawErr = stderr == null ? new byte[0] : stderr.bytes();
                if (rawErr.length != 0) failures.add("nonempty child stderr");
                try { save(output, "child-stdout.bin", rawOut); }
                catch (Throwable failure) { failures.add("stdout save: " + failure); }
                try { save(output, "child-stderr.bin", rawErr); }
                catch (Throwable failure) { failures.add("stderr save: " + failure); }
                if (session != null && session.snapshot != null) {
                    try { save(output, "snapshot.tsv", session.snapshot.out.bytes()); }
                    catch (Throwable failure) { failures.add("snapshot save: " + failure); }
                    try { save(output, "binding.tsv", session.snapshot.binding.bytes()); }
                    catch (Throwable failure) { failures.add("binding save: " + failure); }
                }
                try {
                    Rows receipt = new Rows();
                    receipt.add("STAT_ARRAY_PROCESS_V1");
                    receipt.add("STATUS", failures.isEmpty() ? "PASS" : "FAIL");
                    receipt.add("POLICY", "REQUESTED_DEBUGGER_TERMINATION", "NOT_NATURAL_COMPLETION", "NOT_SUCCESSFUL_BOOTSTRAP");
                    receipt.add("MODE", mode);
                    receipt.add("CHILD", pid, reaped, childExit, requestedExit, requestedCode, complete);
                    receipt.add("DEBUGGER_EXIT_OUTCOME", exitOutcome);
                    for (int index = 0; index < command.size(); index++) receipt.add("COMMAND_UTF16", index, units(command.get(index)));
                    receipt.add("JAR_SHA256", before == null ? "-" : before, after == null ? "-" : after);
                    receipt.add("VM_NAME_UTF16", units(vmName));
                    receipt.add("VM_VERSION_UTF16", units(vmVersion));
                    receipt.add("STDOUT", rawOut.length, hash(rawOut), stdoutTask != null && stdoutTask.isDone() && stdout.failure == null);
                    receipt.add("STDERR", rawErr.length, hash(rawErr), stderrTask != null && stderrTask.isDone() && stderr.failure == null);
                    if (session != null && session.snapshot != null) {
                        byte[] snapshot = session.snapshot.out.bytes(), binding = session.snapshot.binding.bytes();
                        receipt.add("SNAPSHOT", snapshot.length, hash(snapshot), session.snapshot.out.count);
                        receipt.add("BINDING", binding.length, hash(binding), session.snapshot.binding.count);
                        receipt.add("MIRRORS", session.snapshot.held.size(), session.snapshot.released);
                    }
                    receipt.add("ACTIVE_DEADLINE_SECONDS", ACTIVE_SECONDS);
                    receipt.add("WATCHDOG", watchdog != null && watchdog.fired, watchdog != null && watchdog.reaped);
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
