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

/** Reads numeric identities at an untouched original bootstrap boundary. */
public final class ArrayPrefixController {
    private static final String HELPER = "ArrayPrefixObservation";
    private static final int ACTIVE_SECONDS = 40;
    private static final int STREAM_LIMIT = 16 * 1024 * 1024;
    private static final int MAX_STATS = 65536, MAX_HELD = 262144, MAX_TABLE = 131072;
    private static final int MAX_ARRAY = 32000, MAX_LIST = 131072;
    private static final int MAX_RECIPES = 4096, MAX_RECIPE_STACKS = 16384;

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
        final boolean client, achievements, recipes;
        final ClassLoaderReference loader;
        final ReferenceType statistics, base, item;
        final long deadline;
        final Rows out = new Rows(), binding = new Rows(), catalog = new Rows();
        final Map<Long, ObjectReference> held = new LinkedHashMap<>();
        final TreeMap<Integer, Stat> stats = new TreeMap<>();
        final Map<Long, Integer> ids = new HashMap<>();
        final LinkedHashMap<Long, ObjectReference> recipeStacks = new LinkedHashMap<>();
        final Map<Long, Integer> recipeStackIds = new HashMap<>();
        final ObjectReference[] strategies = new ObjectReference[3];
        final Field id, name, independent, key, formatter, associated;
        boolean released;
        Snapshot(VirtualMachine vm, boolean client, boolean achievements, boolean recipes, ClassLoaderReference loader,
                ReferenceType statistics, long deadline) {
            this.vm = vm; this.client = client; this.achievements = achievements; this.recipes = recipes; this.loader = loader;
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
        ReferenceType optionalOriginal(String name) {
            remaining(deadline);
            List<ReferenceType> found = vm.classesByName(name).stream()
                    .filter(type -> loader.equals(type.classLoader())).toList();
            require(found.size() <= 1, "at most one selected-loader original " + name);
            if (found.isEmpty()) return null;
            require(found.get(0) instanceof ClassType, "actual original class " + name);
            return found.get(0).isPrepared() ? found.get(0) : null;
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
                    : type.name().equals(client ? "tw" : "mm") ? "item"
                    : achievements && type.name().equals(client ? "ny" : "it") ? "achievement" : "unsupported";
            require(!kind.equals("unsupported"), "exact supported statistic concrete class");
            if (kind.equals("achievement"))
                require(type instanceof ClassType && ((ClassType)type).superclass().equals(base),
                        "actual Achievement extends original Stat");
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
        List<Value> achievementList(Value value) {
            if (value == null) return null;
            ObjectReference list = hold(value);
            ReferenceType type = bootstrap("java.util.ArrayList");
            require(list.referenceType().equals(type), "actual achievement ArrayList owner");
            int size = integer(list.getValue(field(type, "size", "I", 2)));
            ArrayReference data = (ArrayReference)hold(list.getValue(field(type, "elementData", "[Ljava/lang/Object;", 128)));
            require(size >= 0 && size <= MAX_LIST && size <= data.length(), "bounded achievement list prefix");
            List<Value> values = size == 0 ? List.of() : data.getValues(0, size);
            require(values.size() == size, "complete achievement list read");
            return values;
        }
        String achievementId(Value value) {
            String numeric = sid(value, true);
            if (value != null)
                require(stats.get(ids.get(((ObjectReference)value).uniqueID())).kind.equals("achievement"),
                        "actual registered Achievement reference");
            return numeric;
        }
        void achievementGraph() {
            ReferenceType owner = optionalOriginal(client ? "ep" : "cy");
            List<Value> values = null;
            if (owner == null) {
                require(stats.values().stream().noneMatch(statistic -> statistic.kind.equals("achievement")),
                        "no registered Achievement before owner preparation");
                out.add("ACHIEVEMENTS", 0, 0, 0, 0, "NULL");
                for (int index = 0; index < 16; index++) out.add("ACH_NAMED", index, "NULL");
                return;
            }
            require(owner instanceof ClassType && ((ClassType)owner).superclass().equals(bootstrap("java.lang.Object")),
                    "actual achievement owner class");
            values = achievementList(owner.getValue(field(owner, "e", "Ljava/util/List;", 9)));
            out.add("ACHIEVEMENTS", integer(owner.getValue(field(owner, "a", "I", 9))),
                    integer(owner.getValue(field(owner, "b", "I", 9))),
                    integer(owner.getValue(field(owner, "c", "I", 9))),
                    integer(owner.getValue(field(owner, "d", "I", 9))), values == null ? "NULL" : values.size());
            for (int index = 0; index < 16; index++)
                out.add("ACH_NAMED", index, achievementId(owner.getValue(field(owner,
                        String.valueOf((char)('f' + index)), client ? "Lny;" : "Lit;", 9))));
            LinkedHashMap<Long, ObjectReference> icons = new LinkedHashMap<>();
            Map<Long, Integer> iconIds = new HashMap<>();
            for (Stat statistic : stats.values()) if (statistic.kind.equals("achievement")) {
                remaining(deadline);
                ReferenceType type = statistic.object.referenceType();
                require(type.equals(original(client ? "ny" : "it")), "actual Achievement class identity");
                // Coordinate fields are checked independently of inherited same-letter formatter.
                integer(statistic.object.getValue(field(type, "a", "I", 17)));
                integer(statistic.object.getValue(field(type, "b", "I", 17)));
                stringOrNull(statistic.object.getValue(field(type, "l", "Ljava/lang/String;", 18)));
                if (client) {
                    Value callback = statistic.object.getValue(field(type, "m", "Lgt;", 2));
                    if (callback != null) {
                        ObjectReference object = hold(callback);
                        ReferenceType callbackType = original("gt");
                        require(callbackType instanceof InterfaceType && object.referenceType() instanceof ClassType
                                && ((ClassType)object.referenceType()).allInterfaces().contains(callbackType),
                                "actual achievement callback interface");
                    }
                }
                String parent = achievementId(statistic.object.getValue(field(type, "c", client ? "Lny;" : "Lit;", 17)));
                Value iconValue = statistic.object.getValue(field(type, "d", client ? "Liz;" : "Lfy;", 17));
                String icon = "NULL";
                if (iconValue != null) {
                    ObjectReference object = hold(iconValue);
                    require(object.referenceType().equals(original(client ? "iz" : "fy")),
                            "actual original icon stack class");
                    if (!icons.containsKey(object.uniqueID())) {
                        icons.put(object.uniqueID(), object);
                        iconIds.put(object.uniqueID(), icons.size());
                    }
                    require(icons.get(object.uniqueID()).equals(object), "actual icon identity bijection");
                    icon = "I:" + iconIds.get(object.uniqueID());
                }
                out.add("ACH", statistic.id, parent,
                        bool(statistic.object.getValue(field(type, client ? "n" : "m", "Z", 2))), icon);
            }
            for (ObjectReference icon : icons.values()) {
                remaining(deadline);
                ReferenceType type = icon.referenceType();
                out.add("ICON", iconIds.get(icon.uniqueID()), integer(icon.getValue(field(type, "a", "I", 1))),
                        integer(icon.getValue(field(type, "b", "I", 1))),
                        integer(icon.getValue(field(type, "c", "I", 1))),
                        integer(icon.getValue(field(type, "d", "I", 2))));
            }
            if (values != null) for (int index = 0; index < values.size(); index++) {
                remaining(deadline);
                out.add("ACH_LIST", index, achievementId(values.get(index)));
            }
        }
        void capture(String side, String entry, String boundary) {
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
            out.add(recipes ? "STAT_BOOTSTRAP_V1" : achievements ? "STAT_ARRAY_PREFIX_V2" : "STAT_ARRAY_PREFIX_V1",
                    side, entry, boundary);
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
            if (achievements) achievementGraph();
            out.add("END");
        }
        final Set<String> catalogTypes = new HashSet<>();
        ReferenceType catalogType(String name) {
            ReferenceType type = original(name);
            require(type instanceof ClassType && ((ClassType)type).superclass().equals(bootstrap("java.lang.Object")),
                    "exact direct-Object original catalog class " + name);
            if (catalogTypes.add(name))
                binding.add("ORIGINAL_CATALOG_TYPE", name, loader.uniqueID(), hold(type.classObject()).uniqueID(),
                        "EXCLUSIVE_VERIFIED_JAR_LOADER");
            return type;
        }
        List<Value> catalogList(Value value, String role) {
            ObjectReference list = hold(value);
            ReferenceType type = bootstrap("java.util.ArrayList");
            require(list.referenceType().equals(type), "actual catalog ArrayList " + role);
            int size = integer(list.getValue(field(type, "size", "I", 2)));
            Value backing = list.getValue(field(type, "elementData", "[Ljava/lang/Object;", 128));
            require(backing instanceof ArrayReference, "actual catalog list backing array");
            ArrayReference data = (ArrayReference)hold(backing);
            require(data.referenceType().classLoader() == null
                    && data.referenceType().signature().equals("[Ljava/lang/Object;")
                    && size >= 0 && size <= MAX_RECIPES && size <= data.length() && data.length() <= MAX_TABLE,
                    "complete bounded catalog logical list");
            binding.add("CATALOG_LIST", role, list.uniqueID(), size, data.uniqueID(), data.length());
            List<Value> values = size == 0 ? List.of() : data.getValues(0, size);
            require(values.size() == size, "complete actual catalog list read");
            return values;
        }
        ObjectReference manager(boolean crafting) throws Exception {
            String name = crafting ? (client ? "hk" : "ey") : (client ? "ey" : "de");
            ReferenceType type = optionalOriginal(name);
            if (type == null) return null;
            require(type.equals(catalogType(name)), "actual catalog manager type");
            String signature = "L" + name + ";";
            List<Method> getters = type.methodsByName("a", "()" + signature);
            require(getters.size() == 1 && getters.get(0).declaringType().equals(type)
                    && getters.get(0).modifiers() == 25, "exact original singleton getter");
            byte[] code = getters.get(0).bytecodes();
            require(code.length == 4 && (code[0] & 255) == 178
                    && Session.u16(code, 1) == (crafting ? 104 : 9) && (code[3] & 255) == 176,
                    "source-bound singleton getter returns actual static cell");
            List<Method> containers = type.methodsByName("b", crafting ? "()Ljava/util/List;" : "()Ljava/util/Map;");
            require(containers.size() == 1 && containers.get(0).declaringType().equals(type)
                    && containers.get(0).modifiers() == 1, "exact original manager container getter");
            byte[] container = containers.get(0).bytecodes();
            require(container.length == 5 && (container[0] & 255) == 42 && (container[1] & 255) == 180
                    && Session.u16(container, 2) == (crafting ? 105 : 10) && (container[4] & 255) == 176,
                    "source-bound actual retained container getter");
            binding.add("MANAGER_GETTERS", name, hash(code), hash(container));
            Value value = type.getValue(field(type, "a", signature, 26));
            binding.add("PUBLISHED_MANAGER", name, value == null ? "NULL" : hold(value).uniqueID());
            if (value == null) return null;
            ObjectReference object = hold(value);
            require(object.referenceType().equals(type), "actual published manager owner identity");
            return object;
        }
        String recipeStack(Value value) {
            if (value == null) return "NULL";
            ObjectReference object = hold(value);
            require(object.referenceType().equals(catalogType(client ? "iz" : "fy")), "actual recipe Stack class");
            long id = object.uniqueID();
            if (!recipeStacks.containsKey(id)) {
                require(recipeStacks.size() < MAX_RECIPE_STACKS, "bounded catalog Stack reference table");
                recipeStacks.put(id, object);
                recipeStackIds.put(id, recipeStacks.size());
            }
            require(recipeStacks.get(id).equals(object), "actual catalog Stack identity bijection");
            return "R:" + recipeStackIds.get(id);
        }
        void craftingCatalog(ObjectReference manager) {
            if (manager == null) {
                catalog.add("CRAFTING", "NULL");
                return;
            }
            ReferenceType type = manager.referenceType();
            List<Value> recipes = catalogList(manager.getValue(field(type, "b", "Ljava/util/List;", 2)), "CRAFTING");
            catalog.add("CRAFTING", recipes.size());
            for (int ordinal = 0; ordinal < recipes.size(); ordinal++) {
                remaining(deadline);
                ObjectReference recipe = hold(recipes.get(ordinal));
                type = recipe.referenceType();
                boolean shaped = type.name().equals(client ? "is" : "ft");
                require(shaped || type.name().equals(client ? "tt" : "ml"), "exact concrete recipe class");
                require(type.equals(catalogType(shaped ? (client ? "is" : "ft") : (client ? "tt" : "ml")))
                        && ((ClassType)type).allInterfaces().contains(original(client ? "dt" : "ck")),
                        "actual selected-loader recipe implementation");
                List<Value> ingredients;
                Value output;
                if (shaped) {
                    Value raw = recipe.getValue(field(type, "d", client ? "[Liz;" : "[Lfy;", 2));
                    require(raw instanceof ArrayReference, "actual shaped ingredient array");
                    ArrayReference array = (ArrayReference)hold(raw);
                    require(loader.equals(array.referenceType().classLoader())
                            && array.referenceType().signature().equals(client ? "[Liz;" : "[Lfy;")
                            && array.length() <= MAX_RECIPES, "bounded original shaped array extent");
                    ingredients = array.getValues();
                    require(ingredients.size() == array.length(), "every actual shaped array cell");
                    output = recipe.getValue(field(type, "e", client ? "Liz;" : "Lfy;", 2));
                    catalog.add("RECIPE", ordinal, "shaped", integer(recipe.getValue(field(type, "a", "I", 17))),
                            integer(recipe.getValue(field(type, "b", "I", 2))),
                            integer(recipe.getValue(field(type, "c", "I", 2))), array.length(), recipeStack(output));
                    binding.add("RECIPE_OBJECT", ordinal, recipe.uniqueID(), type.name(), array.uniqueID());
                } else {
                    ingredients = catalogList(recipe.getValue(field(type, "b", "Ljava/util/List;", 18)),
                            "SHAPELESS_" + ordinal);
                    output = recipe.getValue(field(type, "a", client ? "Liz;" : "Lfy;", 18));
                    catalog.add("RECIPE", ordinal, "shapeless", "NULL", "NULL", "NULL", ingredients.size(), recipeStack(output));
                    binding.add("RECIPE_OBJECT", ordinal, recipe.uniqueID(), type.name());
                }
                for (int index = 0; index < ingredients.size(); index++) {
                    remaining(deadline);
                    catalog.add("INGREDIENT", ordinal, index, recipeStack(ingredients.get(index)));
                }
            }
        }
        void smeltingCatalog(ObjectReference manager) {
            if (manager == null) {
                catalog.add("SMELTING", "NULL");
                return;
            }
            ObjectReference map = hold(manager.getValue(field(manager.referenceType(), "b", "Ljava/util/Map;", 2)));
            ReferenceType type = bootstrap("java.util.HashMap");
            require(map.referenceType().equals(type), "actual published smelting HashMap");
            int size = integer(map.getValue(field(type, "size", "I", 128)));
            require(size >= 0 && size <= MAX_RECIPES, "bounded actual smelting map size");
            Value raw = map.getValue(field(type, "table", "[Ljava/util/HashMap$Node;", 128));
            catalog.add("SMELTING", size);
            Set<Long> nodes = new HashSet<>();
            Set<Integer> keys = new HashSet<>();
            if (raw != null) {
                require(raw instanceof ArrayReference, "actual smelting table array");
                ArrayReference table = (ArrayReference)hold(raw);
                require(table.referenceType().classLoader() == null
                        && table.referenceType().signature().equals("[Ljava/util/HashMap$Node;")
                        && table.length() <= MAX_TABLE, "bounded complete smelting bucket table");
                binding.add("SMELTING_MAP", map.uniqueID(), table.uniqueID(), table.length(), size);
                ReferenceType node = bootstrap("java.util.HashMap$Node"), boxed = bootstrap("java.lang.Integer");
                Field key = field(node, "key", "Ljava/lang/Object;", 16);
                Field value = field(node, "value", "Ljava/lang/Object;", 0);
                Field next = field(node, "next", "Ljava/util/HashMap$Node;", 0);
                Field integer = field(boxed, "value", "I", 18);
                int ordinal = 0;
                // Actual JDK bucket/next traversal: deliberately never sorted by key.
                for (Value bucket : table.getValues()) {
                    Value cursor = bucket;
                    while (cursor != null) {
                        remaining(deadline);
                        ObjectReference entry = hold(cursor);
                        require(entry.referenceType().classLoader() == null
                                && (entry.referenceType().name().equals("java.util.HashMap$Node")
                                    || entry.referenceType().name().equals("java.util.HashMap$TreeNode"))
                                && nodes.add(entry.uniqueID()) && nodes.size() <= MAX_RECIPES,
                                "complete acyclic smelting bucket/next links");
                        Map<Field, Value> fields = entry.getValues(List.of(key, value, next));
                        ObjectReference number = hold(fields.get(key));
                        require(number.referenceType().equals(boxed), "actual smelting Integer key");
                        int numeric = integer(number.getValue(integer));
                        require(keys.add(numeric), "unique actual smelting Integer keys");
                        catalog.add("SMELT", ordinal++, numeric, recipeStack(fields.get(value)));
                        cursor = fields.get(next);
                    }
                }
            } else binding.add("SMELTING_MAP", map.uniqueID(), "NULL", 0, size);
            require(nodes.size() == size && keys.size() == size, "complete actual smelting map traversal");
        }
        void captureRecipes(String side, String entry) throws Exception {
            require(recipes, "recipe profile only catalog");
            catalog.add("RECIPE_CATALOG_V1", side, entry);
            craftingCatalog(manager(true));
            smeltingCatalog(manager(false));
            for (ObjectReference stack : recipeStacks.values()) {
                remaining(deadline);
                ReferenceType type = stack.referenceType();
                catalog.add("STACK", recipeStackIds.get(stack.uniqueID()),
                        integer(stack.getValue(field(type, "a", "I", 1))),
                        integer(stack.getValue(field(type, "b", "I", 1))),
                        integer(stack.getValue(field(type, "c", "I", 1))),
                        integer(stack.getValue(field(type, "d", "I", 2))));
            }
            catalog.add("END");
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
        final boolean client, achievements, recipes;
        final long deadline;
        ReferenceType helper, target;
        ClassLoaderReference loader;
        Method method;
        BreakpointRequest breakpoint;
        ExceptionRequest exceptionRequest;
        Snapshot snapshot;
        long threadId;
        String codeHash;
        Session(VirtualMachine vm, String side, String entry, String mode, boolean achievements, boolean recipes, String expectedHash,
                String expectedUrl, long deadline) {
            this.vm = vm; this.side = side; this.entry = entry; this.mode = mode;
            this.client = side.equals("client"); this.achievements = achievements; this.recipes = recipes;
            this.expectedHash = expectedHash; this.expectedUrl = expectedUrl; this.deadline = deadline;
            for (String name : new String[] {HELPER, targetName()}) {
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
                    : entry.equals("block") ? (client ? "uu" : "na")
                    : entry.equals("crafting") ? (client ? "hk" : "ey")
                    : entry.equals("smelting") ? (client ? "ey" : "de") : (client ? "gm" : "ej");
        }
        boolean completeBoundary() { return recipes && (entry.equals("stats") || entry.equals("block")); }
        boolean itemException() { return (achievements || recipes) && entry.equals("item"); }
        boolean managerException() { return recipes && (entry.equals("crafting") || entry.equals("smelting")); }
        boolean dBoundary() { return entry.equals("stats") || (achievements && entry.equals("block")) || managerException(); }
        String targetName() { return itemException() ? (client ? "iz" : "fy") : (client ? "jl" : "gg"); }
        long boundaryIndex() { return completeBoundary() ? 0 : itemException() ? 2
                : managerException() ? (entry.equals("crafting") ? 24 : 75) : dBoundary() ? 21 : 624; }
        String boundaryLabel() { return completeBoundary() ? "complete" : itemException() ? "item-icon-null2"
                : managerException() ? (entry.equals("crafting") ? "crafting-null24" : "smelting-null75")
                : dBoundary() ? "d21" : "clinit624"; }
        void prepare(ClassPrepareEvent event) throws Exception {
            require(event.request().suspendPolicy() == EventRequest.SUSPEND_ALL, "prepare SUSPEND_ALL");
            ReferenceType type = event.referenceType();
            if (type.name().equals(HELPER)) {
                require(helper == null && type.classLoader() != null, "one actual helper");
                helper = type;
                if (completeBoundary()) {
                    List<Method> markers = helper.methodsByName("complete", "()V");
                    require(markers.size() == 1 && markers.get(0).declaringType().equals(helper)
                            && markers.get(0).modifiers() == 9, "exact helper completion marker");
                    method = markers.get(0);
                    byte[] code = method.bytecodes();
                    require(code.length == 1 && (code[0] & 255) == 177, "empty helper completion marker return0");
                    codeHash = hash(code);
                    Location location = method.locationOfCodeIndex(0);
                    require(location != null && location.method().equals(method) && location.codeIndex() == 0,
                            "actual helper complete0 location");
                    breakpoint = vm.eventRequestManager().createBreakpointRequest(location);
                    breakpoint.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                    breakpoint.enable();
                }
                return;
            }
            require(helper != null && target == null && type.name().equals(targetName())
                    && !type.isInitialized() && !type.failedToInitialize(), "original target before initialization");
            Value value = helper.getValue(field(helper, "originalLoader", "Ljava/net/URLClassLoader;", 73));
            require(value instanceof ClassLoaderReference, "actual helper-held URLClassLoader");
            loader = (ClassLoaderReference)value;
            require(loader.equals(type.classLoader()) && !loader.equals(helper.classLoader())
                    && helperString("verifiedJarSha256").equals(expectedHash)
                    && helperString("originalJarUrl").equals(expectedUrl)
                    && helperString("originalEntry").equals(entry)
                    && helperString("originalTrigger").equals(trigger())
                    && helperString("originalProfile").equals(recipes ? "recipes" : achievements ? "achievements" : "arrays"),
                    "original loader/hash/entry/profile binding");
            if (completeBoundary()) {
                target = type;
                threadId = event.thread().uniqueID();
                return;
            }
            String methodName = itemException() ? "<init>" : dBoundary() ? "d" : "<clinit>";
            String signature = itemException() ? (client ? "(Lgm;)V" : "(Lej;)V") : "()V";
            List<Method> methods = type.methodsByName(methodName, signature);
            require(methods.size() == 1 && methods.get(0).declaringType().equals(type)
                    && methods.get(0).modifiers() == (itemException() ? 1 : dBoundary() ? 9 : 8),
                    "actual boundary method descriptor");
            method = methods.get(0);
            byte[] code = method.bytecodes();
            if (itemException()) {
                require(code.length == 11 && (code[0] & 255) == 42 && (code[1] & 255) == 43
                        && (code[2] & 255) == 180 && u16(code, 3) == 14
                        && (code[5] & 255) == 4 && (code[6] & 255) == 3
                        && (code[7] & 255) == 183 && u16(code, 8) == (client ? 41 : 39)
                        && (code[10] & 255) == 177, "source-bound Item icon getfield2 constructor");
            } else if (dBoundary()) {
                require(code.length == 231 && (code[0] & 255) == 178 && u16(code, 1) == 63
                        && (code[6] & 255) == 178 && u16(code, 7) == 64
                        && (code[13] & 255) == 187 && u16(code, 14) == (client ? 44 : 45)
                        && (code[17] & 255) == 183 && u16(code, 18) == (client ? 131 : 137)
                        && (code[20] & 255) == 75 && (code[21] & 255) == 184 && u16(code, 22) == 124
                        && (code[24] & 255) == 182 && u16(code, 25) == 125 && (code[230] & 255) == 177
                        && (!managerException() || ((code[72] & 255) == 184 && u16(code, 73) == 120
                            && (code[75] & 255) == 182 && u16(code, 76) == 121)),
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
            Location location = method.locationOfCodeIndex(boundaryIndex());
            require(location != null && location.method().equals(method)
                    && location.codeIndex() == boundaryIndex(), "exact source code index");
            if (itemException() || managerException()) {
                List<ReferenceType> nullTypes = vm.classesByName("java.lang.NullPointerException").stream()
                        .filter(found -> found.classLoader() == null && found.isPrepared()).toList();
                require(nullTypes.size() == 1, "actual prepared bootstrap NullPointerException");
                exceptionRequest = vm.eventRequestManager().createExceptionRequest(nullTypes.get(0), true, true);
                exceptionRequest.addClassFilter(type);
                exceptionRequest.addThreadFilter(event.thread());
                exceptionRequest.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                exceptionRequest.enable();
            } else {
                breakpoint = vm.eventRequestManager().createBreakpointRequest(location);
                breakpoint.setSuspendPolicy(EventRequest.SUSPEND_ALL);
                breakpoint.enable();
            }
            target = type;
            threadId = event.thread().uniqueID();
        }
        void stop(BreakpointEvent event) throws Exception {
            require(!itemException() && !managerException() && event.request().equals(breakpoint), "selected breakpoint request");
            stop(event.location(), event.thread(), event.request(), null, null);
        }
        void stop(ExceptionEvent event) throws Exception {
            require((itemException() || managerException()) && event.request().equals(exceptionRequest)
                    && event.exception().referenceType().name().equals("java.lang.NullPointerException")
                    && event.exception().referenceType().classLoader() == null, "selected actual bootstrap NPE request");
            stop(event.location(), event.thread(), event.request(), event.exception(), event.catchLocation());
        }
        void stop(Location location, ThreadReference thread, EventRequest request,
                  ObjectReference exception, Location catchLocation) throws Exception {
            require(snapshot == null && target != null
                    && request.suspendPolicy() == EventRequest.SUSPEND_ALL
                    && location.declaringType().equals(completeBoundary() ? helper : target)
                    && location.method().equals(method) && location.codeIndex() == boundaryIndex()
                    && thread.uniqueID() == threadId && thread.name().equals("main"),
                    "unique exact original boundary/thread");
            Field originalTarget = field(helper, "originalTarget", "Ljava/lang/Class;", 73);
            require("Ljava/lang/Class<*>;".equals(originalTarget.genericSignature()),
                    "exact helper target generic signature");
            Value mirror = helper.getValue(originalTarget);
            require(mirror instanceof ClassObjectReference
                    && ((ClassObjectReference)mirror).reflectedType().equals(target), "helper's preloaded target identity");
            List<ThreadReference> threads = vm.allThreads();
            require(threads.size() <= 128, "thread inventory transport bound");
            for (ThreadReference suspended : threads) require(suspended.isSuspended(), "actual all-thread suspension");
            ReferenceType statistics;
            if (itemException()) {
                List<ReferenceType> selected = vm.classesByName(client ? "jl" : "gg").stream()
                        .filter(type -> loader.equals(type.classLoader()) && type.isPrepared()).toList();
                require(selected.size() == 1, "one actual prepared selected-loader Stats class");
                statistics = selected.get(0);
            } else statistics = target;
            snapshot = new Snapshot(vm, client, achievements || recipes, recipes, loader, statistics, deadline);
            if (recipes) {
                snapshot.hold(loader);
                snapshot.hold(helper.classLoader());
                snapshot.hold(thread);
                snapshot.hold(mirror);
            }
            snapshot.binding.add(recipes ? "STAT_BOOTSTRAP_BINDING_V1" : achievements ? "STAT_ARRAY_BINDING_V2" : "STAT_ARRAY_BINDING_V1", side, entry);
            snapshot.binding.add("MODE", mode);
            ClassLoaderReference boundaryLoader = completeBoundary() ? helper.classLoader() : loader;
            if (recipes) snapshot.hold(boundaryLoader);
            snapshot.binding.add("BOUNDARY", location.declaringType().name(), method.name(), method.signature(),
                    location.codeIndex(), codeHash, boundaryLoader.uniqueID(), thread.uniqueID());
            if (achievements) {
                snapshot.binding.add("PROFILE", "achievements", "PUBLISHED_NUMERIC_GRAPH_NOT_FULL_HEAP");
                snapshot.binding.add("STATS_CLASS_STATE", statistics.isPrepared(), statistics.isInitialized(), statistics.failedToInitialize());
                require(!statistics.failedToInitialize() && statistics.isInitialized() == entry.equals("block"),
                        "actual selected Stats initialization state");
                if (exception != null) {
                    snapshot.binding.add("EXCEPTION", snapshot.hold(exception).uniqueID(), exception.referenceType().name());
                    snapshot.binding.add("CATCH", catchLocation == null ? "NULL" : catchLocation.declaringType().name(),
                            catchLocation == null ? "NULL" : catchLocation.method().name(),
                            catchLocation == null ? "NULL" : catchLocation.method().signature(),
                            catchLocation == null ? -1 : catchLocation.codeIndex());
                }
            }
            if (recipes) {
                snapshot.binding.add("PROFILE", "recipes", "PUBLISHED_NUMERIC_GRAPH_NOT_FULL_HEAP");
                snapshot.binding.add("ORIGINAL_ORIGIN", expectedHash, units(expectedUrl), loader.uniqueID(),
                        "ONE_JAR_PLATFORM_PARENT_HELPER_VERIFIED_TARGET_CODESOURCE");
                for (String name : new String[] {client ? "jl" : "gg", client ? "uu" : "na", client ? "gm" : "ej",
                        client ? "ep" : "cy", client ? "hk" : "ey", client ? "ey" : "de"}) classState(name);
                if (exception != null) {
                    snapshot.binding.add("EXCEPTION", snapshot.hold(exception).uniqueID(), exception.referenceType().name());
                    snapshot.binding.add("CATCH", catchLocation == null ? "NULL" : catchLocation.declaringType().name(),
                            catchLocation == null ? "NULL" : catchLocation.method().name(),
                            catchLocation == null ? "NULL" : catchLocation.method().signature(),
                            catchLocation == null ? -1 : catchLocation.codeIndex());
                }
            }
            try {
                require(thread.frameCount() <= 128, "complete stack transport bound");
                List<StackFrame> frames = thread.frames();
                require(!frames.isEmpty() && frames.get(0).location().method().equals(method)
                        && frames.get(0).location().codeIndex() == location.codeIndex(),
                        "actual suspended top frame equals boundary event");
                boolean helperFrame = false, triggerFrame = false;
                for (int index = 0; index < frames.size(); index++) {
                    Location at = frames.get(index).location();
                    if (recipes && at.declaringType().classLoader() != null)
                        snapshot.hold(at.declaringType().classLoader());
                    helperFrame |= at.declaringType().equals(helper) && at.method().name().equals("main");
                    triggerFrame |= at.declaringType().name().equals(trigger())
                            && loader.equals(at.declaringType().classLoader()) && at.method().name().equals("<clinit>");
                    snapshot.binding.add("FRAME", index, at.declaringType().name(), at.method().name(),
                            at.method().signature(), at.codeIndex(),
                            at.declaringType().classLoader() == null ? "BOOT" : at.declaringType().classLoader().uniqueID());
                }
                require(helperFrame && (completeBoundary() || triggerFrame), "real helper call and initiating constructor stack retained");
                if (achievements) verifyAchievementStack(frames, statistics);
                if (recipes) verifyRecipeStack(frames, statistics);
            } catch (IncompatibleThreadStateException failure) {
                throw new IllegalStateException("suspended stack unavailable", failure);
            }
            snapshot.capture(side, entry, boundaryLabel());
            if (recipes) snapshot.captureRecipes(side, entry);
            snapshot.binding.add("MIRRORS", snapshot.held.size());
            snapshot.binding.add("END");
        }
        void bindCode(String role, Location at, byte[] code) throws Exception {
            snapshot.binding.add("CALLSITE", role, at.declaringType().name(), at.method().name(),
                    at.method().signature(), at.codeIndex(), hash(code));
        }
        void classState(String name) {
            List<ReferenceType> types = vm.classesByName(name).stream()
                    .filter(type -> loader.equals(type.classLoader())).toList();
            require(types.size() <= 1, "one selected-loader class-state identity");
            if (types.isEmpty()) snapshot.binding.add("CLASS_STATE", name, "ABSENT");
            else {
                ReferenceType type = types.get(0);
                require(type instanceof ClassType, "actual original class-state type");
                snapshot.binding.add("CLASS_STATE", name, type.isPrepared(), type.isInitialized(),
                        type.failedToInitialize(), loader.uniqueID());
            }
        }
        void verifyRecipeStack(List<StackFrame> frames, ReferenceType statistics) throws Exception {
            if (completeBoundary()) {
                require(frames.size() >= 2, "complete marker actual caller");
                Location caller = frames.get(1).location();
                require(caller.declaringType().equals(helper) && caller.method().name().equals("main")
                        && caller.method().signature().equals("([Ljava/lang/String;)V")
                        && caller.method().modifiers() == 9, "ordinary helper main called complete marker");
                byte[] code = caller.method().bytecodes();
                long pc = caller.codeIndex();
                require(pc >= 0 && pc + 2 < code.length && (code[(int)pc] & 255) == 184,
                        "actual helper main invokestatic completion callsite");
                List<ReferenceType> triggers = vm.classesByName(trigger()).stream()
                        .filter(type -> loader.equals(type.classLoader())).toList();
                require(triggers.size() == 1 && triggers.get(0).isPrepared() && triggers.get(0).isInitialized()
                        && !triggers.get(0).failedToInitialize() && statistics.isInitialized()
                        && !statistics.failedToInitialize(), "ordinary original class initialization returned");
                for (StackFrame frame : frames)
                    require(!loader.equals(frame.location().declaringType().classLoader()),
                            "original trigger frames returned before helper marker");
                bindCode("HELPER_MAIN_COMPLETION", caller, code);
                snapshot.binding.add("ORIGINAL_CLASS_INITIALIZATION_RETURNED", true, trigger());
                return;
            }
            require(itemException() || managerException(), "selected recipe exception route");
            snapshot.binding.add("ORIGINAL_CLASS_INITIALIZATION_RETURNED", false, trigger());
            verifyAchievementStack(frames, statistics);
            if (itemException()) return;
            boolean crafting = entry.equals("crafting");
            ReferenceType owner = snapshot.catalogType(trigger());
            require(!owner.isInitialized() && !owner.failedToInitialize(), "actual manager class still initializing at NPE");
            require(snapshot.manager(crafting) == null, "actual unpublished singleton is null receiver from checked getter");
            snapshot.binding.add("NULL_RECEIVER", trigger(), "STATIC_SINGLETON_NULL_CHECKED_GETTER_NO_INTERVENING_WRITE",
                    boundaryIndex());
            boolean constructor = false, initializer = false, tools = false;
            for (StackFrame frame : frames) {
                Location at = frame.location();
                if (at.declaringType().equals(owner) && at.method().name().equals("<clinit>")) {
                    require(!initializer && at.method().signature().equals("()V") && at.method().modifiers() == 8
                            && at.codeIndex() == 4, "actual pending manager initializer4");
                    byte[] code = at.method().bytecodes();
                    require(code.length == 11 && (code[0] & 255) == 187 && u16(code, 1) == (crafting ? 37 : 1)
                            && (code[4] & 255) == 183 && u16(code, 5) == (crafting ? 147 : 33)
                            && (code[7] & 255) == 179 && u16(code, 8) == (crafting ? 104 : 9)
                            && (code[10] & 255) == 177, "source-bound constructor-before-singleton-publication");
                    bindCode("PENDING_MANAGER_INITIALIZER", at, code);
                    initializer = true;
                }
                if (at.declaringType().equals(owner) && at.method().name().equals("<init>")) {
                    require(!constructor && at.method().signature().equals("()V") && at.method().modifiers() == 2
                            && at.codeIndex() == (crafting ? 19 : 16), "actual pending manager constructor caller");
                    byte[] code = at.method().bytecodes();
                    if (crafting)
                        require(code.length == 3238 && (code[1] & 255) == 183 && u16(code, 2) == (client ? 159 : 166)
                                && (code[5] & 255) == 187 && u16(code, 6) == (client ? 47 : 51)
                                && (code[12] & 255) == 181 && u16(code, 13) == 105
                                && (code[15] & 255) == 187 && u16(code, 16) == 36
                                && (code[19] & 255) == 183 && u16(code, 20) == 145
                                && (code[3237] & 255) == 177, "source-bound pending crafting first helper constructor");
                    else
                        require(code.length == 220 && (code[1] & 255) == 183 && u16(code, 2) == 39
                                && (code[5] & 255) == 187 && u16(code, 6) == 6
                                && (code[12] & 255) == 181 && u16(code, 13) == 10
                                && (code[16] & 255) == 178 && u16(code, 17) == 25
                                && (code[19] & 255) == 180 && u16(code, 20) == 30
                                && (code[219] & 255) == 177, "source-bound pending smelting first Block load");
                    require(frame.thisObject() != null && frame.thisObject().referenceType().equals(owner),
                            "actual pending manager frame owner");
                    snapshot.binding.add("UNPUBLISHED_MANAGER_FRAME", snapshot.hold(frame.thisObject()).uniqueID(),
                            "EXCLUDED_FROM_PUBLISHED_CATALOG");
                    bindCode("PENDING_MANAGER_CONSTRUCTOR", at, code);
                    constructor = true;
                }
                if (crafting && at.declaringType().name().equals(client ? "ha" : "eq")
                        && loader.equals(at.declaringType().classLoader()) && at.method().name().equals("<init>")) {
                    require(!tools && at.method().signature().equals("()V") && at.method().modifiers() == 1
                            && at.codeIndex() == 113, "actual first helper Block.y load113");
                    byte[] code = at.method().bytecodes();
                    require(code.length == 294 && (code[97] & 255) == 181 && u16(code, 98) == 44
                            && (code[113] & 255) == 178 && u16(code, 114) == 47
                            && (code[290] & 255) == 181 && u16(code, 291) == 45
                            && (code[293] & 255) == 177, "source-bound first tools constructor pending field matrix");
                    bindCode("PENDING_TOOLS_CONSTRUCTOR", at, code);
                    tools = true;
                }
            }
            require(initializer && constructor && (!crafting || tools), "complete original pending-manager caller chain");
            snapshot.binding.add("EXCLUDED", "UNPUBLISHED_MANAGER_HELPERS_AND_OTHER_PARTIAL_HEAP");
        }
        void blockCaller(Location at) throws Exception {
            require(at.declaringType().name().equals(client ? "uu" : "na")
                    && loader.equals(at.declaringType().classLoader()) && at.method().name().equals("<clinit>")
                    && at.method().signature().equals("()V") && at.method().modifiers() == 8
                    && at.codeIndex() == 3674, "actual Block Stats.b caller");
            byte[] code = at.method().bytecodes();
            require(code.length == 3678 && (code[3668] & 255) == 178 && u16(code, 3669) == (client ? 354 : 353)
                    && (code[3671] & 255) == 3 && (code[3672] & 255) == 4 && (code[3673] & 255) == 84
                    && (code[3674] & 255) == 184 && u16(code, 3675) == (client ? 442 : 425)
                    && (code[3677] & 255) == 177, "source-bound actual Block caller tail");
            bindCode("BLOCK_CALLER", at, code);
        }
        void verifyAchievementStack(List<StackFrame> frames, ReferenceType statistics) throws Exception {
            if (dBoundary()) {
                require(frames.size() >= 3, "actual d21 caller frames");
                Location caller = frames.get(1).location();
                require(caller.declaringType().equals(statistics) && caller.method().name().equals("b")
                        && caller.method().signature().equals("()V") && caller.method().modifiers() == 9
                        && caller.codeIndex() == 40, "actual Stats.b40 recipe caller");
                byte[] code = caller.method().bytecodes();
                require(code.length == 44 && (code[36] & 255) == 4 && (code[37] & 255) == 179
                        && u16(code, 38) == 63 && (code[40] & 255) == 184
                        && u16(code, 41) == (client ? 139 : 131) && (code[43] & 255) == 177,
                        "source-bound Stats.b40 bytecodes");
                bindCode("STATS_B_CALLER", caller, code);
                blockCaller(frames.get(2).location());
                return;
            }
            require(itemException() && frames.size() >= 4, "actual Item icon exception stack");
            List<Value> arguments = frames.get(0).getArgumentValues();
            require(arguments.size() == 1 && arguments.get(0) == null
                    && frames.get(0).thisObject() != null
                    && frames.get(0).thisObject().referenceType().equals(target),
                    "actual null Item argument at icon getfield2");
            snapshot.hold(frames.get(0).thisObject());
            Location constructor = frames.get(1).location();
            require(constructor.declaringType().name().equals(client ? "ny" : "it")
                    && loader.equals(constructor.declaringType().classLoader())
                    && constructor.method().name().equals("<init>")
                    && constructor.method().signature().equals(client
                            ? "(ILjava/lang/String;IILgm;Lny;)V" : "(ILjava/lang/String;IILej;Lit;)V")
                    && constructor.method().modifiers() == 1 && constructor.codeIndex() == 12,
                    "actual Achievement Item constructor caller12");
            byte[] code = constructor.method().bytecodes();
            require(code.length == 21 && (code[6] & 255) == 187 && u16(code, 7) == (client ? 7 : 6)
                    && (code[9] & 255) == 89 && (code[10] & 255) == 25 && (code[11] & 255) == 5
                    && (code[12] & 255) == 183 && u16(code, 13) == (client ? 26 : 24)
                    && (code[17] & 255) == 183 && u16(code, 18) == (client ? 31 : 26)
                    && (code[20] & 255) == 177, "source-bound Achievement Item constructor bytecodes");
            bindCode("ACHIEVEMENT_CONSTRUCTOR", constructor, code);
            Location owner = frames.get(2).location();
            require(owner.declaringType().name().equals(client ? "ep" : "cy")
                    && loader.equals(owner.declaringType().classLoader()) && owner.method().name().equals("<clinit>")
                    && owner.method().signature().equals("()V") && owner.method().modifiers() == 8
                    && owner.codeIndex() == 23, "actual first Achievement initializer caller23");
            code = owner.method().bytecodes();
            require(code.length == 455 && (code[7] & 255) == 179 && u16(code, 8) == 28
                    && (code[10] & 255) == 187 && u16(code, 11) == (client ? 26 : 20)
                    && (code[19] & 255) == 178 && u16(code, 20) == 48 && (code[22] & 255) == 1
                    && (code[23] & 255) == 183 && u16(code, 24) == (client ? 69 : 62)
                    && (code[32] & 255) == 179 && u16(code, 33) == 29 && (code[454] & 255) == 177,
                    "source-bound first Achievement constructor call");
            bindCode("ACHIEVEMENT_OWNER", owner, code);
            boolean statsCaller = false, block = false;
            for (StackFrame frame : frames) {
                Location at = frame.location();
                if (at.declaringType().equals(statistics) && at.method().name().equals("<clinit>")) {
                    require(!statsCaller && at.method().signature().equals("()V") && at.method().modifiers() == 8
                            && at.codeIndex() == 624, "actual Stats achievement caller624");
                    code = at.method().bytecodes();
                    require(code.length == 636 && (code[621] & 255) == 179 && u16(code, 622) == 59
                            && (code[624] & 255) == 184 && u16(code, 625) == 119 && (code[635] & 255) == 177,
                            "source-bound Stats achievement call");
                    bindCode("STATS_ACHIEVEMENT_CALLER", at, code);
                    statsCaller = true;
                }
                if (at.declaringType().name().equals(client ? "uu" : "na")
                        && loader.equals(at.declaringType().classLoader()) && at.method().name().equals("<clinit>")) {
                    require(!block, "one actual Block caller");
                    blockCaller(at);
                    block = true;
                }
            }
            require(statsCaller && block, "actual Item-triggered nested Stats and Block callers");
            snapshot.binding.add("EXCLUDED", "UNPUBLISHED_PARTIAL_ACHIEVEMENT_AND_ICON_RAW_HEAP");
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
        boolean achievements = false, recipes = false;
        List<String> command = new ArrayList<>(), failures = new ArrayList<>();
        Session session = null;
        boolean complete = false, requestedExit = false, reaped = false;
        int requestedCode = -1, childExit = Integer.MIN_VALUE;
        long pid = -1, start = System.nanoTime(), deadline = start + TimeUnit.SECONDS.toNanos(ACTIVE_SECONDS);
        try {
            require(Runtime.version().feature() == 17, "captured Java17 observation profile");
            require((args.length == 4 || args.length == 5 || args.length == 6) && (args[0].equals("client") || args[0].equals("server"))
                    && (args[1].equals("stats") || args[1].equals("block") || args[1].equals("item")
                        || (args.length == 6 && args[5].equals("recipes")
                            && (args[1].equals("crafting") || args[1].equals("smelting")))),
                    "Usage: ArrayPrefixController client|server stats|block|item|crafting|smelting jar fresh-output-directory [interpreted|tier1|default [achievements|recipes]]");
            mode = args.length >= 5 ? args[4] : "interpreted";
            require(mode.equals("interpreted") || mode.equals("tier1") || mode.equals("default"),
                    "requested JVM mode");
            require(args.length != 6 || args[5].equals("achievements") || args[5].equals("recipes"), "optional observer profile");
            achievements = args.length == 6 && args[5].equals("achievements");
            recipes = args.length == 6 && args[5].equals("recipes");
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
            if (achievements) command.add("achievements");
            if (recipes) command.add("recipes");
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
            if (achievements) started.add("PROFILE", "achievements");
            if (recipes) started.add("PROFILE", "recipes");
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
            session = new Session(vm, args[0], args[1], mode, achievements, recipes, expected, jar.toUri().toURL().toExternalForm(), deadline);
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
                        } else if (event instanceof ExceptionEvent) {
                            boundary = true;
                            session.stop((ExceptionEvent)event);
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
                    if (recipes) {
                        try { save(output, "recipes.tsv", session.snapshot.catalog.bytes()); }
                        catch (Throwable failure) { failures.add("recipes save: " + failure); }
                    }
                }
                try {
                    Rows receipt = new Rows();
                    receipt.add("STAT_ARRAY_PROCESS_V1");
                    receipt.add("STATUS", failures.isEmpty() ? "PASS" : "FAIL");
                    if (recipes)
                        receipt.add("POLICY", "REQUESTED_DEBUGGER_TERMINATION", "NOT_NATURAL_PROCESS_COMPLETION",
                                session == null || !complete ? "NO_ACCEPTED_BOUNDARY"
                                    : session.completeBoundary() ? "ORIGINAL_CLASS_INITIALIZATION_RETURNED" : "STOPPED_ORIGINAL_EXCEPTION");
                    else
                        receipt.add("POLICY", "REQUESTED_DEBUGGER_TERMINATION", "NOT_NATURAL_COMPLETION", "NOT_SUCCESSFUL_BOOTSTRAP");
                    receipt.add("MODE", mode);
                    if (achievements) receipt.add("PROFILE", "achievements", "PUBLISHED_NUMERIC_GRAPH_NOT_FULL_HEAP");
                    if (recipes) receipt.add("PROFILE", "recipes", "PUBLISHED_NUMERIC_GRAPH_NOT_FULL_HEAP");
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
                        if (recipes) {
                            byte[] catalog = session.snapshot.catalog.bytes();
                            receipt.add("RECIPES", catalog.length, hash(catalog), session.snapshot.catalog.count);
                        }
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
