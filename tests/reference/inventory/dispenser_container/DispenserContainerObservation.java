import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Genuine SH/AZ/IX constructors and inherited DW getter; no replacement class. */
@SuppressWarnings("removal")
public final class DispenserContainerObservation {
    static final String[] CORE = {"sh", "dw", "gp", "ix", "az", "ow", "iz", "lw", "gs", "js"};
    static final IdentityHashMap<iz,String> identities = new IdentityHashMap<>();
    static final LinkedHashMap<String,iz> inputs = new LinkedHashMap<>();
    static int network, nativeLinks, execs, writes, deletes, replacements;

    static void need(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Object field(Object receiver, Class<?> declaring, String name) throws Exception {
        Field value = declaring.getDeclaredField(name);
        value.setAccessible(true);
        return value.get(receiver); // Observation only; no reflective writes/invocation.
    }
    static Map<String,Object> map(Object... pairs) {
        Map<String,Object> value = new LinkedHashMap<>();
        for (int index = 0; index < pairs.length; index += 2) {
            String key = (String)pairs[index]; need(!value.containsKey(key), "unique authored JSON key");
            value.put(key, pairs[index + 1]);
        }
        return value;
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) {
            StringBuilder out = new StringBuilder("\"");
            String digits = "0123456789abcdef";
            for (int index = 0; index < ((String)value).length(); ++index) {
                char c = ((String)value).charAt(index);
                if (c == '"' || c == '\\') out.append('\\').append(c);
                else if (c < 32 || c >= 127) {
                    out.append("\\u").append(digits.charAt(c >>> 12)).append(digits.charAt((c >>> 8) & 15));
                    out.append(digits.charAt((c >>> 4) & 15)).append(digits.charAt(c & 15));
                } else out.append(c);
            }
            return out.append('"').toString();
        }
        if (value instanceof Map) {
            StringBuilder out = new StringBuilder("{");
            for (Map.Entry<?,?> entry : ((Map<?,?>)value).entrySet()) {
                if (out.length() != 1) out.append(',');
                out.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return out.append('}').toString();
        }
        if (value instanceof Iterable) {
            StringBuilder out = new StringBuilder("[");
            for (Object item : (Iterable<?>)value) {
                if (out.length() != 1) out.append(','); out.append(json(item));
            }
            return out.append(']').toString();
        }
        need(value instanceof Number || value instanceof Boolean, "closed authored JSON value");
        return value.toString();
    }
    static String digest(byte[] bytes) throws Exception {
        StringBuilder value = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) {
            int n = b & 255; value.append("0123456789abcdef".charAt(n >>> 4));
            value.append("0123456789abcdef".charAt(n & 15));
        }
        return value.toString();
    }
    static Object origins() throws Exception {
        Map<String,Object> classes = map(); Path jar = null;
        for (String name : CORE) {
            Class<?> type = Class.forName(name, false, DispenserContainerObservation.class.getClassLoader());
            Path source = Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if (jar == null) jar = source;
            need(Files.isRegularFile(source) && source.equals(jar), "one actual original archive");
            try (InputStream input = type.getResourceAsStream("/" + name + ".class")) {
                need(input != null, "actual original class resource");
                byte[] bytes = input.readNBytes(1024 * 1024 + 1);
                need(bytes.length <= 1024 * 1024, "bounded class resource");
                classes.put(name, map("bytes", bytes.length, "sha256", digest(bytes)));
            }
        }
        return map("jar", jar.toString(), "jar_sha256", digest(Files.readAllBytes(jar)), "classes", classes);
    }
    static String label(iz value) {
        if (value == null) return null;
        String label = identities.get(value);
        if (label == null) { label = "N" + identities.size(); identities.put(value, label); }
        return label;
    }
    static List<Object> array(iz[] values) {
        List<Object> out = new ArrayList<>();
        for (iz value : values) out.add(label(value));
        return out;
    }
    static Object snapshot(az tile, ix player) throws Exception {
        Object tileState = null, playerState = null;
        if (tile != null) {
            Object random = field(tile, az.class, "b");
            tileState = map("slots", array((iz[])field(tile, az.class, "a")), "world_null", tile.d == null,
                "random_constructed", random != null, "random_class", random == null ? null : random.getClass().getName());
        }
        if (player != null) playerState = map("main", array(player.a), "armor", array(player.b),
            "cursor", label(player.i()), "selected", player.c, "changed", player.e, "player_null", player.d == null);
        Map<String,Object> stacks = map();
        for (Map.Entry<String,iz> entry : inputs.entrySet()) {
            iz value = entry.getValue();
            stacks.put(entry.getKey(), List.of(value.c, value.a, field(value, iz.class, "d"), value.b));
        }
        return map("tile", tileState, "player", playerState, "stacks", stacks);
    }
    static Object layout(sh container, az tile, ix player) throws Exception {
        List<Object> rows = new ArrayList<>();
        for (Object value : container.e) {
            gp slot = (gp)value;
            Object owner = field(slot, gp.class, "e");
            rows.add(map("index", slot.a, "owner", owner == null ? null : owner == tile ? "AZ" : owner == player ? "IX" : "OTHER",
                "inventory_index", field(slot, gp.class, "d"), "x", slot.b, "y", slot.c, "class", slot.getClass().getName()));
        }
        return rows;
    }
    static Object base(sh container, az tile) throws Exception {
        Object listeners = field(container, dw.class, "g"), blocked = field(container, dw.class, "b");
        boolean allNull = true;
        for (Object value : container.d) allNull &= value == null;
        return map("window", container.f, "transaction", field(container, dw.class, "a"),
            "slots", container.e.size(), "snapshots", container.d.size(), "snapshots_null", allNull,
            "listeners", ((List<?>)listeners).size(), "blocked", ((java.util.Set<?>)blocked).size(),
            "collections_distinct", container.d != container.e && container.d != listeners && container.d != blocked &&
                container.e != listeners && container.e != blocked && listeners != blocked,
            "captured_az_same", field(container, sh.class, "a") == tile);
    }
    static Object throwable(Throwable root) {
        if (root == null) return null;
        IdentityHashMap<Throwable,Integer> ids = new IdentityHashMap<>();
        List<Throwable> pending = new ArrayList<>(); List<Object> nodes = new ArrayList<>();
        ids.put(root, 0); pending.add(root);
        for (int index = 0; index < pending.size(); ++index) {
            Throwable value = pending.get(index); List<Object> suppressed = new ArrayList<>();
            Throwable cause = value.getCause();
            if (cause != null && !ids.containsKey(cause)) { need(pending.size() < 32, "bounded complete Throwable graph"); ids.put(cause, pending.size()); pending.add(cause); }
            for (Throwable child : value.getSuppressed()) {
                if (!ids.containsKey(child)) { need(pending.size() < 32, "bounded complete suppression graph"); ids.put(child, pending.size()); pending.add(child); }
                suppressed.add(ids.get(child));
            }
            List<Object> frames = new ArrayList<>(); StackTraceElement[] stack = value.getStackTrace();
            need(stack.length <= 128, "bounded complete actual Throwable stack");
            for (StackTraceElement frame : stack) frames.add(map("class", frame.getClassName(), "method", frame.getMethodName(),
                "file", frame.getFileName(), "line", frame.getLineNumber(), "native", frame.isNativeMethod()));
            nodes.add(map("id", index, "class", value.getClass().getName(), "message", value.getMessage(),
                "cause", cause == null ? null : ids.get(cause), "suppressed", suppressed, "frames", frames));
        }
        return map("root", 0, "nodes", nodes);
    }
    static void observe(String[] row, int ordinal) throws Exception {
        need(row.length == 6 && (row[1].equals("0") || row[1].equals("1")) &&
            (row[2].equals("0") || row[2].equals("1")), "closed case declaration");
        identities.clear(); inputs.clear();
        ix player = row[1].equals("1") ? new ix((gs)null) : null;
        az tile = row[2].equals("1") ? new az() : null;
        sh container = new sh(player, tile);
        if (!row[3].equals("layout")) {
            if (tile != null) { iz a = new iz(1, 7, 3); inputs.put("A", a); identities.put(a, "A"); tile.a(0, a); }
            if (player != null) {
                iz b = new iz(4, 6, 2), h = new iz(5, 2, 1);
                inputs.put("B", b); identities.put(b, "B"); inputs.put("H", h); identities.put(h, "H");
                player.a(9, b); player.a(0, h);
            }
        }
        Object before = snapshot(tile, player), result = null; Throwable failure = null;
        int slot = Integer.parseInt(row[4]), button = Integer.parseInt(row[5]);
        try {
            if (row[3].equals("getter")) result = label(container.a(slot));
            else if (row[3].equals("usable")) result = container.b((gs)null);
            else if (row[3].equals("click")) result = label(container.a(slot, button, true, (gs)null));
            else need(row[3].equals("layout"), "declared bounded operation");
        } catch (Throwable actual) { failure = actual; }
        System.out.println("SH_ROW " + json(map("ordinal", ordinal, "case", row[0], "operation", row[3],
            "player_present", player != null, "az_present", tile != null, "slot", slot, "button", button,
            "layout", layout(container, tile, player), "base", base(container, tile), "before", before,
            "returned", failure == null, "result", result, "throwable", throwable(failure), "after", snapshot(tile, player))));
    }
    static final class Guard extends SecurityManager {
        @Override public void checkPermission(Permission value) {
            if (value.getName().equals("setSecurityManager")) { ++replacements; throw new SecurityException("guard replacement"); }
        }
        @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException("network"); }
        @Override public void checkListen(int port) { ++network; throw new SecurityException("listen"); }
        @Override public void checkAccept(String host, int port) { ++network; throw new SecurityException("accept"); }
        @Override public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native link"); }
        @Override public void checkExec(String command) { ++execs; throw new SecurityException("exec"); }
        @Override public void checkWrite(String path) { ++writes; throw new SecurityException("write"); }
        @Override public void checkDelete(String path) { ++deletes; throw new SecurityException("delete"); }
    }
    public static void main(String[] args) throws Exception {
        need(args.length == 1, "one exact case declaration");
        byte[] declared = Files.readAllBytes(Path.of(args[0]));
        need(declared.length <= 8192, "bounded declared input");
        Guard guard = new Guard(); System.setSecurityManager(guard);
        System.out.println("SH_ORIGINS " + json(origins()));
        List<String> cases = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8);
        need(cases.size() == 18, "exact bounded case inventory");
        for (int index = 0; index < cases.size(); ++index) observe(cases.get(index).split("\t", -1), index);
        System.out.println("SH_GUARD " + json(map("network", network, "native_links", nativeLinks, "execs", execs,
            "writes", writes, "deletes", deletes, "replacements", replacements, "installed", System.getSecurityManager() == guard)));
        System.out.println("SH_COMPLETE " + json(map("cases", cases.size(), "declaration_sha256", digest(declared),
            "scope", "GENUINE_SH_COMPONENT_ONLY_NO_SUCCESSFUL_GS_SHIFT_QUALIFICATION")));
    }
}
