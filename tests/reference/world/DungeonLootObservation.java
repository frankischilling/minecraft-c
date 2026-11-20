import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.jar.JarFile;

/** Finite actual private Dungeon helper calls. No placement, World, original
 * field assignment, Random subclass, constructor bypass or resource shim. */
public final class DungeonLootObservation {
    private static final String CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    private static final String SERVER = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    private static final int MAX_SEARCH = 1 << 20;
    private static final int MAX_OPERATIONS = 512;
    private static final int MAX_OUTPUT = 8 * 1024 * 1024;
    private static final long[] EXTRA_SEEDS = {Long.MIN_VALUE, Long.MAX_VALUE, -1L, 0L, 1L,
        -23L, 23L, 0x0000ffffffffffffL, 0x0001000000000000L, 0x5deece66dL,
        -0x5deece66dL, 0x0123456789abcdefL};

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static Map<String,Object> object(Object... pairs) {
        require(pairs.length % 2 == 0, "object pairs");
        Map<String,Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) result.put((String)pairs[i], pairs[i + 1]);
        return result;
    }

    private static String hex(long value, int width) {
        return String.format(Locale.ROOT, width == 16 ? "%016x" : "%08x",
            width == 16 ? value : value & 0xffffffffL);
    }

    private static String utf16(String value) {
        if (value == null) return null;
        require(value.length() <= 65535, "complete bounded UTF16 string");
        StringBuilder result = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i)
            result.append(String.format(Locale.ROOT, "%04x", (int)value.charAt(i)));
        return result.toString();
    }

    private static String sha256(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static String descriptor(Class<?> type) {
        if (type.isArray()) return type.getName().replace('.', '/');
        if (!type.isPrimitive()) return "L" + type.getName().replace('.', '/') + ";";
        if (type == void.class) return "V";
        if (type == int.class) return "I";
        if (type == long.class) return "J";
        if (type == boolean.class) return "Z";
        if (type == byte.class) return "B";
        if (type == short.class) return "S";
        if (type == double.class) return "D";
        if (type == float.class) return "F";
        return "C";
    }

    private static Object throwable(Throwable failure) {
        StringWriter text = new StringWriter();
        failure.printStackTrace(new PrintWriter(text));
        require(text.getBuffer().length() <= 65536, "complete bounded Throwable graph");
        return object("status", "threw", "class", failure.getClass().getName(),
            "message_utf16", utf16(failure.getMessage()), "stack_trace", text.toString());
    }

    /* This independent control selects seeds by the paired source's Random
     * call order only. It never predicts an Item ID, Stack, mob text or a
     * successful original class initialization. A source failure before a
     * later draw can make the actual continuation differ from this model. */
    private static final class Selection {
        final boolean loot;
        final int[] bounds = new int[3], values = new int[3];
        int size, branch, gate = -1, count = -1, record = -1;

        Selection(Random random, boolean loot) {
            this.loot = loot;
            branch = draw(random, loot ? 11 : 4);
            if (!loot) return;
            if (branch == 1 || branch == 3 || branch == 4 || branch == 5)
                count = draw(random, 4);
            else if (branch == 7) gate = draw(random, 100);
            else if (branch == 8) {
                gate = draw(random, 2);
                if (gate == 0) count = draw(random, 4);
            } else if (branch == 9) {
                gate = draw(random, 10);
                if (gate == 0) record = draw(random, 2);
            }
        }

        int draw(Random random, int bound) {
            require(size < bounds.length, "bounded selection draw inventory");
            int value = random.nextInt(bound);
            bounds[size] = bound; values[size++] = value;
            return value;
        }

        Object view() {
            List<Object> draws = new ArrayList<>();
            for (int i = 0; i < size; ++i)
                draws.add(object("index", i, "bound", bounds[i], "value", values[i]));
            return object("role", "independent JDK Random source-order coverage control; not an actual call trace or expected return",
                "family", loot ? "loot" : "mob", "branch", branch, "gate_draw", gate,
                "count_draw", count, "record_offset_draw", record, "draws", draws);
        }
    }

    private static final class Requirement {
        final String label;
        final boolean loot;
        final int branch, gateMode, count, record;
        Long seed;

        Requirement(String label, boolean loot, int branch, int gateMode, int count, int record) {
            this.label = label; this.loot = loot; this.branch = branch;
            this.gateMode = gateMode; this.count = count; this.record = record;
        }

        boolean matches(Selection value) {
            return value.loot == loot && value.branch == branch &&
                (gateMode == 0 || (gateMode > 0 ? value.gate == 0 : value.gate > 0)) &&
                (count < 0 || value.count == count) && (record < 0 || value.record == record);
        }
    }

    private static List<Requirement> requirements() {
        List<Requirement> result = new ArrayList<>();
        // Reach all three gate misses before the first loot branch that can
        // actively use Item; an explicit block-first bootstrap is separate.
        for (int branch : new int[] {7, 8, 9})
            result.add(new Requirement("loot_gate_miss_" + branch, true, branch, -1, -1, -1));
        for (int branch = 0; branch <= 10; ++branch)
            result.add(new Requirement("loot_choice_" + branch, true, branch, 0, -1, -1));
        for (int branch : new int[] {1, 3, 4, 5, 8})
            for (int count = 0; count < 4; ++count)
                result.add(new Requirement("loot_choice_" + branch + "_count_draw_" + count,
                    true, branch, branch == 8 ? 1 : 0, count, -1));
        for (int branch : new int[] {7, 8, 9})
            result.add(new Requirement("loot_gate_hit_" + branch, true, branch, 1, -1, -1));
        for (int record = 0; record < 2; ++record)
            result.add(new Requirement("loot_record_offset_" + record, true, 9, 1, -1, record));
        for (int branch = 0; branch < 4; ++branch)
            result.add(new Requirement("mob_branch_" + branch, false, branch, 0, -1, -1));
        require(result.size() == 43, "complete fixed source-order coverage requirements");
        return result;
    }

    private static int findSeeds(List<Requirement> requirements) {
        int remaining = requirements.size();
        int visited = 0;
        for (long seed = 0; seed < MAX_SEARCH && remaining != 0; ++seed) {
            Selection loot = new Selection(new Random(seed), true);
            Selection mob = new Selection(new Random(seed), false);
            ++visited;
            for (Requirement requirement : requirements) {
                if (requirement.seed == null && requirement.matches(requirement.loot ? loot : mob)) {
                    requirement.seed = seed;
                    --remaining;
                }
            }
        }
        require(remaining == 0, "finite declared seed-search bound covered every required source draw alternative");
        return visited;
    }

    private static final class Binding {
        final URL jar;
        final URLClassLoader loader;
        final Class<?> feature, stack, item;
        final Constructor<?> constructor;
        final Method loot, mob;
        final Map<String,Object> origins = new LinkedHashMap<>(), methods = new LinkedHashMap<>(), fields = new LinkedHashMap<>();
        final IdentityHashMap<Object,String> identities = new IdentityHashMap<>();
        final List<Object> operations = new ArrayList<>();

        Binding(URL jar, URLClassLoader loader, boolean client) throws Exception {
            this.jar = jar; this.loader = loader;
            feature = type(client ? "er" : "da");
            Class<?> parent = type(client ? "pg" : "jm");
            require(feature.getSuperclass() == parent, "actual Dungeon feature superclass");
            stack = type(client ? "iz" : "fy");
            item = type(client ? "gm" : "ej");
            require(stack.getModifiers() == 17, "actual final public Stack");
            constructor = bindConstructor(feature);
            bindConstructor(parent);
            loot = bindMethod("loot", "a", stack);
            mob = bindMethod("mob", "b", String.class);
            bindConstructor(stack, item);
            bindConstructor(stack, item, int.class);
            bindConstructor(stack, item, int.class, int.class);
            bindConstructor(stack, int.class, int.class, int.class);
            Field[] members = stack.getDeclaredFields();
            require(members.length == 4, "actual complete four-field Stack layout");
            for (String name : new String[] {"a", "b", "c", "d"}) {
                Field member = stack.getDeclaredField(name);
                require(member.getType() == int.class && member.getModifiers() == (name.equals("d") ? 2 : 1), "exact Stack field descriptor/modifiers");
                member.setAccessible(true);
                fields.put(stack.getName() + "." + name, object("class", stack.getName(), "name", name,
                    "signature", "I", "modifiers", member.getModifiers()));
            }
        }

        Class<?> type(String name) throws Exception {
            Class<?> value = Class.forName(name, false, loader);
            require(value.getClassLoader() == loader && value.getProtectionDomain().getCodeSource() != null &&
                jar.equals(value.getProtectionDomain().getCodeSource().getLocation()), "untouched original class origin: " + name);
            if (!origins.containsKey(name)) {
                byte[] bytes;
                try (InputStream stream = value.getResourceAsStream("/" + name.replace('.', '/') + ".class")) {
                    require(stream != null, "original class resource");
                    bytes = stream.readNBytes(2 * 1024 * 1024 + 1);
                }
                require(bytes.length <= 2 * 1024 * 1024, "complete bounded class resource");
                origins.put(name, object("url", jar.toString(), "loader", "isolated URLClassLoader", "bytes", bytes.length, "sha256", sha256(bytes)));
            }
            return value;
        }

        Constructor<?> bindConstructor(Class<?> owner, Class<?>... parameters) throws Exception {
            Constructor<?> value = owner.getDeclaredConstructor(parameters);
            require(value.getModifiers() == 1, "public actual constructor");
            StringBuilder signature = new StringBuilder("(");
            for (Class<?> parameter : parameters) signature.append(descriptor(parameter));
            signature.append(")V");
            methods.put(owner.getName() + signature, object("class", owner.getName(), "name", "<init>",
                "signature", signature.toString(), "modifiers", value.getModifiers(), "binding_only", owner != feature));
            return value;
        }

        Method bindMethod(String role, String name, Class<?> result) throws Exception {
            Method value = feature.getDeclaredMethod(name, Random.class);
            require(value.getReturnType() == result && value.getModifiers() == 2, "exact private helper descriptor/modifiers");
            value.setAccessible(true);
            methods.put(role, object("class", feature.getName(), "name", name,
                "signature", "(Ljava/util/Random;)" + descriptor(result), "modifiers", 2));
            return value;
        }

        String reference(Object value) {
            if (value == null) return null;
            String old = identities.get(value);
            if (old != null) return old;
            String next = "r" + identities.size();
            identities.put(value, next);
            return next;
        }

        Map<String,Object> operation(String label, String owner, String method, String signature) {
            require(operations.size() < MAX_OPERATIONS, "complete bounded explicit operation list");
            Map<String,Object> row = object("ordinal", operations.size(), "label", label,
                "class", owner, "method", method, "signature", signature);
            operations.add(row);
            System.err.println("DUNGEON_HELPER_BEGIN " + row.get("ordinal") + " " + label);
            return row;
        }

        void returned(Map<String,Object> row, Object value) {
            row.put("outcome", object("status", "returned", "value", value));
            System.err.println("DUNGEON_HELPER_END " + row.get("ordinal") + " returned");
        }

        Object construct() throws Exception {
            Map<String,Object> row = operation("actual_Dungeon_constructor", feature.getName(), "<init>", "()V");
            try {
                Object result = constructor.newInstance();
                returned(row, object("reference", reference(result), "class", result.getClass().getName()));
                return result;
            } catch (InvocationTargetException failure) {
                row.put("outcome", throwable(failure.getCause()));
                throw failure;
            }
        }

        Object bootstrapBlock(boolean client) throws Exception {
            String name = client ? "uu" : "na";
            Class<?> expected = type(name); // Passive origin check, no initialization.
            Map<String,Object> row = operation("explicit_prepared_Block_first_initialization",
                "java.lang.Class", "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
            row.put("arguments", object("name", name, "initialize", true, "loader", "isolated original URLClassLoader"));
            Class<?> actual;
            try { actual = Class.forName(name, true, loader); }
            catch (Throwable failure) {
                row.put("outcome", throwable(failure));
                System.err.println("DUNGEON_HELPER_END " + row.get("ordinal") + " threw " + failure.getClass().getName());
                return object("scope", "explicit prepared helper class context, no World or natural Dungeon construction",
                    "operation", row, "phase", "no class-phase injection or Java initialization-state inspection");
            }
            require(actual == expected, "same checked original Block class");
            returned(row, object("class", actual.getName(), "origin", origins.get(name)));
            return object("scope", "one actual Block initialization request in this fresh process; genuine transitive source work retained; no World or natural Dungeon construction",
                "operation", row, "phase", "normal Class.forName return observed; no native phase flag was assigned");
        }

        Object instanceFields(Object value) throws Exception {
            List<Object> rows = new ArrayList<>();
            for (Class<?> owner = value.getClass(); owner != Object.class; owner = owner.getSuperclass()) {
                require(owner != null && owner.getClassLoader() == loader, "original instance hierarchy");
                type(owner.getName());
                Field[] members = owner.getDeclaredFields();
                require(members.length <= 32, "bounded instance layout");
                Arrays.sort(members, Comparator.comparing(Field::getName));
                for (Field member : members) {
                    if (Modifier.isStatic(member.getModifiers())) continue;
                    member.setAccessible(true);
                    require(member.getType() == int.class, "reached original helper instance fields are complete ints");
                    fields.put(owner.getName() + "." + member.getName(), object("class", owner.getName(),
                        "name", member.getName(), "signature", "I", "modifiers", member.getModifiers()));
                    rows.add(object("class", owner.getName(), "name", member.getName(), "type", "int", "value", member.getInt(value)));
                }
            }
            return object("reference", reference(value), "class", value.getClass().getName(), "fields", rows);
        }

        Object result(Object value, boolean isLoot) throws Exception {
            if (value == null) return object("kind", "null");
            if (isLoot) {
                require(value.getClass() == stack, "actual returned Stack class");
                Object inventory = instanceFields(value);
                Map<String,Object> roles = new LinkedHashMap<>();
                String[] names = {"c", "a", "d", "b"}, rolesNames = {"id", "count", "damage", "animation"};
                for (int i = 0; i < names.length; ++i) {
                    Field field = stack.getDeclaredField(names[i]); field.setAccessible(true);
                    roles.put(rolesNames[i], field.getInt(value));
                }
                return object("kind", "actual_Stack", "instance", inventory, "source_fields", roles,
                    "phase", "unobservable: original Stack has four int fields and no native RAW/READY/FAILED phase field");
            }
            require(value.getClass() == String.class, "actual returned String");
            String text = (String)value;
            return object("kind", "actual_String", "reference", reference(text),
                "length_utf16", text.length(), "utf16", utf16(text));
        }

        List<Object> continuation(String label, Random random) {
            List<Object> rows = new ArrayList<>();
            Map<String,Object> row = operation(label + "_post_nextInt", "java.util.Random", "nextInt", "()I");
            row.put("receiver", reference(random));
            int integer = random.nextInt(); returned(row, integer); rows.add(row);
            row = operation(label + "_post_nextInt1000", "java.util.Random", "nextInt", "(I)I");
            row.put("receiver", reference(random));
            row.put("bound", 1000); integer = random.nextInt(1000); returned(row, integer); rows.add(row);
            row = operation(label + "_post_nextLong", "java.util.Random", "nextLong", "()J");
            row.put("receiver", reference(random));
            long wide = random.nextLong(); returned(row, object("signed_decimal", Long.toString(wide), "raw_bits", hex(wide, 16))); rows.add(row);
            row = operation(label + "_post_nextFloat", "java.util.Random", "nextFloat", "()F");
            row.put("receiver", reference(random));
            float single = random.nextFloat(); returned(row, object("raw_bits", hex(Float.floatToRawIntBits(single), 8))); rows.add(row);
            row = operation(label + "_post_nextDouble", "java.util.Random", "nextDouble", "()D");
            row.put("receiver", reference(random));
            double doubled = random.nextDouble(); returned(row, object("raw_bits", hex(Double.doubleToRawLongBits(doubled), 16))); rows.add(row);
            return rows;
        }

        Object observe(String label, Object featureValue, boolean isLoot, Random random,
                       Object before, Selection independent) throws Exception {
            require(random == null || random.getClass() == Random.class, "genuine unmodified java.util.Random input");
            Method method = isLoot ? loot : mob;
            Map<String,Object> row = object("label", label, "family", isLoot ? "loot" : "mob",
                "feature_reference", reference(featureValue), "random_reference", reference(random),
                "before", before, "independent_selection", independent == null ? null : independent.view());
            Map<String,Object> call = operation(label, feature.getName(), method.getName(),
                "(Ljava/util/Random;)" + descriptor(method.getReturnType()));
            call.put("receiver", reference(featureValue)); call.put("random_argument", reference(random));
            try {
                Object actual = method.invoke(featureValue, new Object[] {random});
                Object observed = result(actual, isLoot);
                returned(call, observed); row.put("helper_outcome", call.get("outcome"));
            } catch (InvocationTargetException failure) {
                call.put("outcome", throwable(failure.getCause()));
                row.put("helper_outcome", call.get("outcome"));
                System.err.println("DUNGEON_HELPER_END " + call.get("ordinal") + " threw " + failure.getCause().getClass().getName());
            }
            row.put("post_call_random_operations", random == null ? List.of() : continuation(label, random));
            row.put("continuation_scope", "actual post-call draws consume the supplied Random; no before-state draw, reflective state access, seed reset or nondestructive-copy claim");
            return row;
        }
    }

    private static Object seed(long seed) {
        return object("scope", "fresh actual Random(long); declared initial seed only, not reflected internal state",
            "signed_decimal", Long.toString(seed), "raw_bits", hex(seed, 16));
    }

    private static Object resources(Path jar) throws Exception {
        List<Object> rows = new ArrayList<>();
        try (JarFile archive = new JarFile(jar.toFile())) {
            for (String name : new String[] {"lang/en_US.lang", "lang/stats_US.lang"}) {
                var entry = archive.getJarEntry(name);
                if (entry == null) { rows.add(object("name", name, "present", false)); continue; }
                byte[] bytes;
                try (InputStream stream = archive.getInputStream(entry)) { bytes = stream.readNBytes(1048577); }
                require(bytes.length <= 1048576, "complete bounded original language resource");
                rows.add(object("name", name, "present", true, "bytes", bytes.length, "sha256", sha256(bytes)));
            }
        }
        return object("scope", "archive receipt only; no replacement resource/provider or eager Item static read", "entries", rows);
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) {
            StringBuilder result = new StringBuilder("\"");
            for (char unit : ((String)value).toCharArray()) {
                if (unit == '"' || unit == '\\') result.append('\\').append(unit);
                else if (unit < 32 || unit > 126) result.append(String.format(Locale.ROOT, "\\u%04x", (int)unit));
                else result.append(unit);
            }
            return result.append('"').toString();
        }
        if (value instanceof Boolean || value instanceof Integer || value instanceof Long) return value.toString();
        if (value instanceof Map<?,?>) {
            StringBuilder result = new StringBuilder("{");
            for (var entry : ((Map<?,?>)value).entrySet()) {
                if (result.length() != 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return result.append('}').toString();
        }
        if (value instanceof List<?>) {
            StringBuilder result = new StringBuilder("[");
            for (Object entry : (List<?>)value) {
                if (result.length() != 1) result.append(',');
                result.append(json(entry));
            }
            return result.append(']').toString();
        }
        throw new AssertionError("unsupported JSON type: " + value.getClass().getName());
    }

    private static void pin(Path jar, boolean client) throws Exception {
        require(Files.size(jar) == (client ? 1465375 : 503100), "pinned original archive size");
        require(sha256(Files.readAllBytes(jar)).equals(client ? CLIENT : SERVER), "untouched pinned original archive SHA256");
    }

    public static void main(String[] args) throws Exception {
        System.setOut(System.err);
        require(args.length == 4, "jar client|server cold|block-first output_json");
        boolean client = args[1].equals("client");
        require(client || args[1].equals("server"), "paired side");
        boolean blockFirst = args[2].equals("block-first");
        require(blockFirst || args[2].equals("cold"), "explicit fresh-process helper context mode");
        Path argument = Path.of(args[0]).toAbsolutePath().normalize(), jar = argument.toRealPath();
        Path output = Path.of(args[3]).toAbsolutePath().normalize();
        require(argument.equals(jar) && Files.isRegularFile(jar, LinkOption.NOFOLLOW_LINKS), "canonical original archive");
        require(!Files.exists(output, LinkOption.NOFOLLOW_LINKS) && Files.isDirectory(output.getParent()) &&
            output.getParent().equals(output.getParent().toRealPath()), "fresh output in canonical existing owned parent");
        pin(jar, client);
        List<Requirement> requirements = requirements();
        int visited = findSeeds(requirements);
        Map<String,Object> record = object("schema", 1, "side", args[1], "mode", args[2], "jar_sha256", client ? CLIENT : SERVER,
            "scope", "finite untouched-original private Dungeon loot/mob helpers only; no placement, natural population, World or native parity claim",
            "selection_scope", "only coverage seeds/branch draws are independently selected with JDK Random; every Stack/mob/Throwable result comes from actual original invocation",
            "lifetime", "one genuine original Dungeon instance and original class context; fresh explicit seeded Random per independent case, then one shared continuation sequence",
            "item_active_use", "cold has no explicit original initialization prelude; block-first makes one recorded genuine Block initialization request. Reached Item/Stack active use is original source work, with no observer Item static read/write, fake resource, replacement success on failure or expected numeric palette",
            "operation_scope", "actual original Dungeon constructor/helper and explicit post-call Random draws; seed search/caller Random(long) inputs and original nested calls are not instrumented",
            "search", object("first_seed_inclusive", 0, "limit_exclusive", MAX_SEARCH, "visited", visited,
                "labels", requirements.size(), "order", "ascending nonnegative declared numeric seeds; independent fresh Random per helper family"),
            "resources", resources(jar));
        List<Object> cases = new ArrayList<>();
        URL url = jar.toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url}, ClassLoader.getPlatformClassLoader())) {
            Binding binding = new Binding(url, loader, client);
            record.put("platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader());
            record.put("class_origins", binding.origins); record.put("bindings", binding.methods);
            record.put("fields", binding.fields); record.put("operations", binding.operations);
            record.put("bootstrap", blockFirst ? binding.bootstrapBlock(client) :
                object("status", "not_requested", "scope", "cold explicit helper context; no Block/Item initialization prelude or successful-world claim"));
            Object feature = binding.construct();
            record.put("feature_before", binding.instanceFields(feature));
            cases.add(binding.observe("null_random_loot", feature, true, null,
                object("scope", "explicit NULL Random source control"), null));
            cases.add(binding.observe("null_random_mob", feature, false, null,
                object("scope", "explicit NULL Random source control"), null));
            for (Requirement requirement : requirements) {
                long initial = requirement.seed;
                Selection selection = new Selection(new Random(initial), requirement.loot);
                require(requirement.matches(selection), "selected seed still meets declared coverage condition");
                cases.add(binding.observe(requirement.label, feature, requirement.loot,
                    new Random(initial), seed(initial), selection));
            }
            for (int index = 0; index < EXTRA_SEEDS.length; ++index) {
                long initial = EXTRA_SEEDS[index];
                for (boolean isLoot : new boolean[] {true, false})
                    cases.add(binding.observe("declared_seed_" + index + (isLoot ? "_loot" : "_mob"),
                        feature, isLoot, new Random(initial), seed(initial), new Selection(new Random(initial), isLoot)));
            }
            long initial = -7046029254386353131L;
            Random shared = new Random(initial);
            boolean[] sequence = {true, false, true, true, false, false, true, false};
            for (int index = 0; index < sequence.length; ++index)
                cases.add(binding.observe("shared_random_" + index, feature, sequence[index], shared,
                    object("scope", "same actual Random continued through earlier helper AND explicit post-call draws",
                        "initial_seed", seed(initial), "index", index), null));
            require(cases.size() == 77, "complete finite helper-call inventory");
            record.put("feature_after", binding.instanceFields(feature));
            record.put("cases", cases); record.put("case_count", cases.size());
            record.put("operation_count", binding.operations.size());
            record.put("terminal", "completed finite observations; source-result/native comparison is external and no all-success claim is inferred");
        }
        pin(jar, client);
        byte[] bytes = (json(record) + "\n").getBytes(StandardCharsets.UTF_8);
        require(bytes.length <= MAX_OUTPUT, "complete bounded output; no truncation");
        Files.write(output, bytes, StandardOpenOption.CREATE_NEW);
    }
}
