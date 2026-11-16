import java.io.BufferedReader;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Base64;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Controlled LW inputs to the untouched original CD, GP, DW, OG and IZ bodies. */
public final class ChestContainerObservation {
    private static final List<String> calls = new ArrayList<String>();
    private static final IdentityHashMap<iz, String> labels = new IdentityHashMap<iz, String>();
    private static final LinkedHashMap<String, iz> stacks = new LinkedHashMap<String, iz>();
    private static int network, nativeLinks;

    private static final class ProbeFailure extends RuntimeException {
        ProbeFailure(String message) { super(message); }
    }

    private static final class Inventory implements lw {
        final String label, failure;
        final int[] sizes;
        final int limit;
        final boolean usable;
        final iz[] values = new iz[64];
        int sizeCalls;

        Inventory(String label, String sizes, int limit, boolean usable, String failure) {
            this.label = label;
            String[] words = sizes.split(",");
            this.sizes = new int[words.length];
            for (int i = 0; i < words.length; ++i) this.sizes[i] = Integer.parseInt(words[i]);
            this.limit = limit;
            this.usable = usable;
            this.failure = failure;
        }

        private void call(String operation, String argument) {
            calls.add(label + "." + operation + argument);
            if (failure.equals(operation)) throw new ProbeFailure(label + "." + operation);
        }

        public int a() {
            call("size", "");
            return sizes[Math.min(sizeCalls++, sizes.length - 1)];
        }
        public iz f_(int index) { call("get", ":" + index); return values[index]; }
        public iz a(int index, int amount) {
            call("split", ":" + index + ":" + amount);
            return values[index];
        }
        public void a(int index, iz value) {
            call("set", ":" + index + ":" + label(value));
            values[index] = value;
        }
        public String c() { call("name", ""); return label + " inventory"; }
        public int d() { call("limit", ""); return limit; }
        public void y_() { call("changed", ""); }
        public boolean a_(gs player) { call("usable", player == null ? ":null" : ":player"); return usable; }
    }

    private static Object field(Object owner, Class<?> declared, String name) throws Exception {
        Field value = declared.getDeclaredField(name);
        value.setAccessible(true);
        return value.get(owner);
    }

    private static String label(iz stack) {
        if (stack == null) return null;
        String result = labels.get(stack);
        if (result == null) {
            result = "N" + (stacks.size() + 1);
            labels.put(stack, result);
            stacks.put(result, stack);
        }
        return result;
    }

    private static String quote(String text) {
        if (text == null) return "null";
        StringBuilder out = new StringBuilder("\"");
        for (int i = 0; i < text.length(); ++i) {
            char c = text.charAt(i);
            if (c == '"' || c == '\\') out.append('\\').append(c);
            else if (c < 32 || c >= 127) out.append(String.format("\\u%04x", (int)c));
            else out.append(c);
        }
        return out.append('"').toString();
    }

    private static String strings(List<String> values) {
        StringBuilder out = new StringBuilder("[");
        for (String value : values) {
            if (out.length() > 1) out.append(',');
            out.append(quote(value));
        }
        return out.append(']').toString();
    }

    private static String inventory(Inventory value) {
        if (value == null) return "null";
        StringBuilder out = new StringBuilder("[");
        for (iz stack : value.values) {
            if (out.length() > 1) out.append(',');
            out.append(quote(label(stack)));
        }
        return out.append(']').toString();
    }

    private static String snapshot(Inventory left, Inventory right) throws Exception {
        String leftSlots = inventory(left), rightSlots = inventory(right);
        StringBuilder out = new StringBuilder("{\"left\":").append(leftSlots)
            .append(",\"right\":").append(rightSlots).append(",\"stacks\":{");
        boolean comma = false;
        for (Map.Entry<String, iz> entry : stacks.entrySet()) {
            if (comma) out.append(',');
            comma = true;
            iz stack = entry.getValue();
            out.append(quote(entry.getKey())).append(":{")
               .append("\"id\":").append(stack.c).append(",\"count\":").append(stack.a)
               .append(",\"damage\":").append(field(stack, iz.class, "d"))
               .append(",\"animation\":").append(stack.b).append('}');
        }
        return out.append("}}").toString();
    }

    private static void place(Inventory owner, String locations) {
        if (owner == null || locations.equals("-")) return;
        for (String location : locations.split(";")) {
            String[] words = location.split(":");
            owner.values[Integer.parseInt(words[0])] = stacks.get(words[1]);
        }
    }

    private static String layout(cd container, Inventory left, Inventory right) throws Exception {
        StringBuilder out = new StringBuilder("[");
        for (Object raw : container.e) {
            gp slot = (gp)raw;
            Object owner = field(slot, gp.class, "e");
            String ownerLabel = owner == null ? null : owner == left ? "L" : owner == right ? "R" : "unknown";
            if (out.length() > 1) out.append(',');
            out.append('[').append(slot.a).append(',').append(quote(ownerLabel)).append(',')
               .append(field(slot, gp.class, "d")).append(',').append(slot.b).append(',')
               .append(slot.c).append(']');
        }
        return out.append(']').toString();
    }

    private static String digest(byte[] bytes) throws Exception {
        StringBuilder result = new StringBuilder();
        for (byte value : MessageDigest.getInstance("SHA-256").digest(bytes))
            result.append(String.format("%02x", value & 255));
        return result.toString();
    }

    private static String origins() throws Exception {
        String[] names = {"cd", "og", "lw", "gp", "dw", "iz", "gm", "hp", "e", "gs", "dc", "js", "ix", "uu", "id", "da", "bb", "bw", "cv"};
        Path jar = null;
        StringBuilder classes = new StringBuilder("{");
        for (String name : names) {
            Class<?> type = Class.forName(name, false, ChestContainerObservation.class.getClassLoader());
            Path source = Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if (jar == null) jar = source;
            if (!Files.isRegularFile(source) || !source.equals(jar)) throw new AssertionError("same supplied original JAR");
            if (classes.length() > 1) classes.append(',');
            try (InputStream input = type.getResourceAsStream("/" + name + ".class")) {
                if (input == null) throw new AssertionError("original class resource");
                classes.append(quote(name)).append(':').append(quote(digest(input.readAllBytes())));
            }
        }
        return "{\"jar_sha256\":" + quote(digest(Files.readAllBytes(jar))) + ",\"classes\":" + classes.append('}') + "}";
    }

    private static void observe(String declaration) throws Exception {
        String[] v = declaration.split("\t", -1);
        if (v.length != 19) throw new IllegalArgumentException("declared column count");
        calls.clear(); labels.clear(); stacks.clear();
        if (!v[16].equals("-")) for (String value : v[16].split(";")) {
            String[] words = value.split(":");
            iz stack = new iz(Integer.parseInt(words[1]), Integer.parseInt(words[2]), Integer.parseInt(words[3]));
            stack.b = Integer.parseInt(words[4]);
            labels.put(stack, words[0]); stacks.put(words[0], stack);
        }
        Inventory left = v[3].equals("-") ? null : new Inventory("L", v[3], Integer.parseInt(v[5]), Boolean.parseBoolean(v[7]), v[17]);
        Inventory right = v[9].equals("true") ? left : v[4].equals("-") ? null : new Inventory("R", v[4], Integer.parseInt(v[6]), Boolean.parseBoolean(v[8]), v[18]);
        place(left, v[14]); place(right, v[15]);
        String name = v[10].equals("-") ? null : new String(Base64.getDecoder().decode(v[10]), StandardCharsets.UTF_8);
        int index = Integer.parseInt(v[11]), amount = Integer.parseInt(v[12]);
        String before = snapshot(left, right), result = "null", slots = "null", rows = "null", base = "null", screen = "null", error = "null", message = "null";
        try {
            if (v[1].equals("cd") || v[1].equals("hp")) {
                Inventory suppliedLeft = Boolean.parseBoolean(v[13]) ? null : left;
                cd container;
                if (v[1].equals("hp")) {
                    hp value = new hp(suppliedLeft, right);
                    container = (cd)value.j;
                    screen = "{\"width\":" + field(value, id.class, "a") +
                        ",\"height\":" + field(value, id.class, "i") + ",\"rows\":" + field(value, hp.class, "n") +
                        ",\"input\":" + value.f + ",\"pauses\":" + value.c() +
                        ",\"left_same\":" + (field(value, hp.class, "l") == suppliedLeft) +
                        ",\"right_same\":" + (field(value, hp.class, "m") == right) + "}";
                } else container = new cd(suppliedLeft, right);
                rows = field(container, cd.class, "b").toString();
                slots = layout(container, suppliedLeft, right);
                Object listeners = field(container, dw.class, "g"), blocked = field(container, dw.class, "b");
                boolean distinct = container.d != container.e && container.d != listeners &&
                    container.e != listeners && blocked != container.d && blocked != container.e && blocked != listeners;
                boolean allNull = true;
                for (Object value : container.d) if (value != null) allNull = false;
                base = "{\"window\":" + container.f + ",\"transaction\":" + field(container, dw.class, "a") +
                    ",\"snapshots\":" + container.d.size() + ",\"all_snapshots_null\":" + allNull +
                    ",\"listeners\":" + ((java.util.List<?>)listeners).size() +
                    ",\"blocked_players\":" + ((java.util.Set<?>)blocked).size() + ",\"distinct_collections\":" + distinct + "}";
                if (v[2].equals("shift")) result = quote(label(container.a(index)));
                else if (v[2].equals("usable")) result = Boolean.toString(container.b((gs)null));
                else if (!v[2].equals("construct")) throw new IllegalArgumentException("CD operation");
            } else {
                og value = new og(name, left, right);
                if (v[2].equals("size")) result = Integer.toString(value.a());
                else if (v[2].equals("name")) result = quote(value.c());
                else if (v[2].equals("get")) result = quote(label(value.f_(index)));
                else if (v[2].equals("set")) value.a(index, stacks.get("A"));
                else if (v[2].equals("split")) result = quote(label(value.a(index, amount)));
                else if (v[2].equals("limit")) result = Integer.toString(value.d());
                else if (v[2].equals("changed")) value.y_();
                else if (v[2].equals("usable")) result = Boolean.toString(value.a_((gs)null));
                else throw new IllegalArgumentException("OG operation");
            }
        } catch (NullPointerException failure) { error = quote(failure.getClass().getName()); message = quote(failure.getMessage());
        } catch (IndexOutOfBoundsException failure) { error = quote(failure.getClass().getName()); message = quote(failure.getMessage());
        } catch (ProbeFailure failure) { error = quote("ProbeFailure"); message = quote(failure.getMessage()); }
        String after = snapshot(left, right);
        System.out.println("CHEST_CONTAINER {\"profile\":" + quote(v[0]) + ",\"result\":" + result +
            ",\"rows\":" + rows + ",\"layout\":" + slots + ",\"base\":" + base + ",\"screen\":" + screen + ",\"error\":" + error +
            ",\"error_message\":" + message + ",\"calls\":" + strings(calls) +
            ",\"before\":" + before + ",\"after\":" + after + "}");
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("one declaration file required");
        String origins = origins();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission value) {}
            public void checkConnect(String host, int port) { ++network; throw new SecurityException("network"); }
            public void checkListen(int port) { ++network; throw new SecurityException("listen"); }
            public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native link"); }
        });
        // This process explicitly initializes original Blocks before Items.
        // Entering GM first exposes a different circular-init prefix.
        Class.forName("uu");
        Class.forName("gm");
        System.out.println("CHEST_ORIGINS " + origins);
        try (BufferedReader reader = Files.newBufferedReader(Path.of(args[0]), StandardCharsets.UTF_8)) {
            String line;
            while ((line = reader.readLine()) != null) observe(line);
        }
        System.out.println("CHEST_GUARD {\"network\":" + network + ",\"native_links\":" + nativeLinks + "}");
        if (network != 0 || nativeLinks != 0) throw new AssertionError("forbidden external operation");
    }
}
