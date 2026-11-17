import java.io.FileDescriptor;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/** Authored component caller of unchanged original yk/ow/nu code.
 * No class replacement, Source-private write, hash forcing, World or GL.
 * Compile against the supplied unchanged client JAR. Root owns execution. */
public final class SignNbtObservation {
    static final String JAR_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final List<Object> labels = new ArrayList<Object>();
    static final Map<String,Object> origins = new LinkedHashMap<String,Object>();
    static final List<String> cases = new ArrayList<String>();
    static final int[] FIRST = {101, -20, 303};
    static final int[] SECOND = {-1234567, 64, Integer.MIN_VALUE + 17};
    static final String[] OLD = {"old-one", "old-two", "old-three", "old-four"};
    static final String[] NEW = {"new-one", "new-two", "new-three", "new-four"};
    static final Field SIGN_FLAG = declared(yk.class, "c", boolean.class, false);
    static final Field BASE_INVALID = declared(ow.class, "h", boolean.class, false);
    static final Field COMPOUND_MAP = declared(nu.class, "a", Map.class, false);
    static final Field TAG_NAME = declared(ij.class, "a", String.class, false);
    static Path archive;
    static int rows, constructorCalls, readCalls, writeCalls;
    static int networkAttempts, nativeAttempts, execAttempts, writeAttempts, deleteAttempts, exitAttempts;

    interface SourceCall { void run() throws Throwable; }

    static void need(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    static Map<String,Object> map(Object... words) {
        need((words.length & 1) == 0, "paired JSON keys");
        Map<String,Object> out = new LinkedHashMap<String,Object>();
        for (int i = 0; i < words.length; i += 2) {
            String key = (String)words[i];
            need(!out.containsKey(key), "unique JSON keys");
            out.put(key, words[i + 1]);
        }
        return out;
    }

    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) {
            StringBuilder out = new StringBuilder("\"");
            for (char c : ((String)value).toCharArray()) {
                if (c == '"' || c == '\\') out.append('\\').append(c);
                else if (c < 32 || c >= 127) out.append(String.format("\\u%04x", (int)c));
                else out.append(c);
            }
            return out.append('"').toString();
        }
        if (value instanceof Map) {
            StringBuilder out = new StringBuilder("{");
            for (Map.Entry<?,?> entry : ((Map<?,?>)value).entrySet()) {
                if (out.length() > 1) out.append(',');
                out.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return out.append('}').toString();
        }
        if (value instanceof Iterable) {
            StringBuilder out = new StringBuilder("[");
            for (Object item : (Iterable<?>)value) {
                if (out.length() > 1) out.append(',');
                out.append(json(item));
            }
            return out.append(']').toString();
        }
        need(value instanceof Number || value instanceof Boolean, "bounded JSON value type");
        return value.toString();
    }

    static void row(String kind, Object value) {
        ++rows;
        need(rows <= 512, "bounded complete row count");
        System.out.println("SIGN_NBT_" + kind + " " + json(value));
    }

    static String sha(byte[] bytes) throws Exception {
        StringBuilder out = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes))
            out.append(String.format("%02x", b & 255));
        return out.toString();
    }

    static Field declared(Class<?> owner, String name, Class<?> type, boolean publicField) {
        try {
            Field value = owner.getDeclaredField(name);
            need(value.getType() == type && !Modifier.isStatic(value.getModifiers()), "exact instance field");
            if (publicField) need(Modifier.isPublic(value.getModifiers()), "actual public field only");
            value.setAccessible(true);
            return value;
        } catch (ReflectiveOperationException failure) {
            throw new ExceptionInInitializerError(failure);
        }
    }

    static String label(Object value) {
        if (value == null) return null;
        for (int i = 0; i < labels.size(); ++i)
            if (labels.get(i) == value) return "R" + i;
        need(labels.size() < 512, "bounded linear reference labels");
        labels.add(value);
        return "R" + (labels.size() - 1);
    }

    static String units(String value) {
        if (value == null) return null;
        need(value.length() <= 4096, "bounded complete UTF-16 String");
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < value.length(); ++i)
            out.append(String.format("%04x", (int)value.charAt(i)));
        return out.toString();
    }

    static Object string(String value) {
        return value == null ? null : map("ref", label(value), "units", units(value), "length", value.length());
    }

    static void origin(Class<?> type) throws Exception {
        String name = type.getName();
        if (origins.containsKey(name)) return;
        need(type.getClassLoader() == SignNbtObservation.class.getClassLoader(), "ordinary same defining loader");
        Path source = Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
        need(source.equals(archive), "unchanged archive CodeSource for " + name);
        try (InputStream in = type.getResourceAsStream("/" + name.replace('.', '/') + ".class")) {
            need(in != null, "complete original Class resource");
            byte[] raw = in.readAllBytes();
            origins.put(name, map("bytes", raw.length, "sha256", sha(raw)));
        }
    }

    static Object state(yk tile) throws Exception {
        if (tile == null) return null;
        need(tile.getClass() == yk.class, "actual complete yk receiver");
        String[] lines = tile.a; // Actual public array read; no copy installed into Source.
        List<Object> contents = null;
        if (lines != null) {
            need(lines.length <= 16, "bounded complete Sign array");
            contents = new ArrayList<Object>();
            for (String line : lines) contents.add(string(line));
        }
        return map("ref", label(tile), "class", tile.getClass().getName(),
                   "world", tile.d == null ? null : map("class", tile.d.getClass().getName(), "ref", label(tile.d)),
                   "xyz", List.of(tile.e, tile.f, tile.g), "invalid", BASE_INVALID.getBoolean(tile),
                   "lines_array", lines == null ? null : map("ref", label(lines), "length", lines.length, "items", contents),
                   "edit_row", tile.b, "private_c", SIGN_FLAG.getBoolean(tile));
    }

    static Object tag(ij value, int depth) throws Exception {
        if (value == null) return null;
        need(depth <= 4, "bounded complete authored NBT depth");
        origin(value.getClass());
        int type = value.a(); // Actual return-only original tag-type method, observation only.
        Map<String,Object> out = map("ref", label(value), "class", value.getClass().getName(),
                                     "type", type, "name", string((String)TAG_NAME.get(value)));
        if (type == 10) {
            need(value.getClass() == nu.class, "actual concrete Compound");
            Map<?,?> data = (Map<?,?>)COMPOUND_MAP.get(value);
            need(data.size() <= 32, "bounded complete Compound fields");
            List<Object> encounter = new ArrayList<Object>();
            Map<String,Object> logical = new TreeMap<String,Object>();
            for (Map.Entry<?,?> entry : data.entrySet()) {
                need(entry.getKey() instanceof String && entry.getValue() instanceof ij, "actual NBT map entries");
                String key = (String)entry.getKey();
                encounter.add(units(key));
                need(logical.put(units(key), tag((ij)entry.getValue(), depth + 1)) == null, "unique logical NBT key");
            }
            out.put("encounter_key_units", encounter);
            out.put("logical_entries", logical);
        } else {
            need(type == 1 || type == 3 || type == 8, "only authored Byte/Int/String payloads");
            List<Field> fields = new ArrayList<Field>();
            for (Field field : value.getClass().getDeclaredFields())
                if (!Modifier.isStatic(field.getModifiers())) fields.add(field);
            need(fields.size() == 1, "one actual tag payload field");
            Field payload = fields.get(0);
            payload.setAccessible(true);
            if (type == 8) {
                need(payload.getType() == String.class, "actual String tag payload");
                out.put("value", string((String)payload.get(value)));
            } else {
                need(payload.getType() == (type == 1 ? byte.class : int.class), "actual integer tag payload");
                out.put("value", payload.get(value));
            }
        }
        return out;
    }

    static Object throwable(Throwable failure, List<Throwable> seen) {
        if (failure == null) return null;
        for (int i = 0; i < seen.size(); ++i)
            if (seen.get(i) == failure) return map("cycle_to", i);
        need(seen.size() < 16, "bounded complete Throwable graph");
        int index = seen.size();
        seen.add(failure);
        StackTraceElement[] stack = failure.getStackTrace();
        need(stack.length <= 128, "bounded complete Throwable stack");
        List<Object> frames = new ArrayList<Object>();
        for (StackTraceElement frame : stack)
            frames.add(map("class", frame.getClassName(), "method", frame.getMethodName(),
                           "file", frame.getFileName(), "line", frame.getLineNumber(),
                           "native", frame.isNativeMethod(), "loader", frame.getClassLoaderName(),
                           "module", frame.getModuleName(), "module_version", frame.getModuleVersion()));
        Throwable[] suppressed = failure.getSuppressed();
        need(suppressed.length <= 16, "bounded complete suppressed Throwables");
        List<Object> suppressedRows = new ArrayList<Object>();
        for (Throwable item : suppressed) suppressedRows.add(throwable(item, seen));
        return map("node", index, "class", failure.getClass().getName(), "message", failure.getMessage(),
                   "frames", frames, "cause", throwable(failure.getCause(), seen), "suppressed", suppressedRows);
    }

    static void beginCase(String name, String scope) {
        need(!cases.contains(name), "unique declared case");
        cases.add(name);
        labels.clear();
        row("CASE", map("index", cases.size() - 1, "case", name, "scope", scope,
                        "reference_labels", "linear == labels reset per case; observation output only"));
    }

    static yk construct(String name, String phase) throws Exception {
        ++constructorCalls;
        yk tile = null;
        Throwable escaped = null;
        try { tile = new yk(); }
        catch (Throwable actual) { escaped = actual; }
        row("CALL", map("case", name, "phase", phase, "method", "yk.<init>()V", "before", null,
                        "returned", escaped == null, "failure", throwable(escaped, new ArrayList<Throwable>()),
                        "after", state(tile)));
        need(escaped == null && tile != null, "constructor preparation completed; a failure is not a usable receiver");
        return tile;
    }

    static boolean call(String name, String phase, String method, yk tile, nu compound, SourceCall body)
            throws Exception {
        Object before = state(tile), nbtBefore = tag(compound, 0);
        need(json(before).equals(json(state(tile))), "NBT observation preserved actual Tile entry fields");
        Throwable escaped = null;
        try { body.run(); }
        catch (Throwable actual) { escaped = actual; }
        Object after = state(tile), nbtAfter = tag(compound, 0);
        need(json(after).equals(json(state(tile))), "NBT observation preserved actual Tile exit fields");
        row("CALL", map("case", name, "phase", phase, "method", method,
                        "before", before, "nbt_before", nbtBefore,
                        "returned", escaped == null, "failure", throwable(escaped, new ArrayList<Throwable>()),
                        "after", after, "nbt_after", nbtAfter));
        return escaped == null;
    }

    static boolean read(String name, String phase, yk tile, nu compound) throws Exception {
        ++readCalls;
        return call(name, phase, "yk.a(Lnu;)V", tile, compound, () -> tile.a(compound));
    }

    static boolean write(String name, String phase, yk tile, nu compound) throws Exception {
        ++writeCalls;
        return call(name, phase, "yk.b(Lnu;)V", tile, compound, () -> tile.b(compound));
    }

    static nu input(int[] xyz, String[] lines, int wrongCoordinate, int wrongLine, boolean unknown) {
        nu result = new nu();
        String[] names = {"x", "y", "z"};
        if (xyz != null) for (int i = 0; i < 3; ++i) {
            if (i == wrongCoordinate) result.a(names[i], "wrong-coordinate-tag");
            else result.a(names[i], xyz[i]);
        }
        for (int i = 0; i < 4; ++i) {
            String name = "Text" + (i + 1);
            if (i == wrongLine) result.a(name, 900 + i);
            else if (lines[i] != null) result.a(name, lines[i]);
        }
        if (unknown) {
            nu nested = new nu();
            nested.a("Number", 1234567);
            nested.a("Text", "unrelated-input");
            result.a("InputOnly", nested);
            result.a("InputByte", (byte)-7);
        }
        return result;
    }

    static nu seededOutput() {
        nu result = new nu();
        result.a("id", 999);
        result.a("x", "old-x-tag");
        result.a("Text1", 111);
        result.a("Text4", "old-four-tag");
        nu nested = new nu();
        nested.a("Number", -7654321);
        nested.a("Text", "unrelated-output");
        result.a("KeepOut", nested);
        return result;
    }

    static void serializeAndReadBack(String name, yk tile) throws Exception {
        nu fresh = new nu();
        boolean freshReturned = write(name, "write-fresh", tile, fresh);
        write(name, "write-existing", tile, seededOutput());
        if (freshReturned) {
            yk second = construct(name, "roundtrip-constructor");
            read(name, "roundtrip-read", second, fresh);
        }
    }

    static void successfulReadCase(String name, int[] xyz, String[] lines, boolean unknown) throws Exception {
        beginCase(name, "Genuine constructor, real authored Compound setters, actual read/write and second-constructor readback; no public field prestate");
        yk tile = construct(name, "constructor");
        if (read(name, "read", tile, input(xyz, lines, -1, -1, unknown))) serializeAndReadBack(name, tile);
    }

    static String repeated(char value, int count) {
        char[] out = new char[count];
        Arrays.fill(out, value);
        return new String(out);
    }

    static String[] lengths(int count) {
        return new String[] {repeated('A', count), repeated('B', count), repeated('C', count), repeated('D', count)};
    }

    static void profiles() throws Exception {
        String name = "constructor-defaults";
        beginCase(name, "Actual constructor and default-state writer/readback, without authored Source fields");
        serializeAndReadBack(name, construct(name, "constructor"));
        successfulReadCase("missing-all-text", FIRST, new String[4], false);
        successfulReadCase("mixed-missing-text", FIRST, new String[] {null, "present-two", null, "present-four"}, false);
        successfulReadCase("length-14", FIRST, lengths(14), false);
        successfulReadCase("length-15", FIRST, lengths(15), false);
        successfulReadCase("length-16", FIRST, lengths(16), false);
        successfulReadCase("supplementary-inside-15", FIRST,
                           new String[] {repeated('Q', 13) + "\ud83d\ude00", "two", "three", "four"}, false);
        successfulReadCase("supplementary-cut-at-15", FIRST,
                           new String[] {repeated('Q', 14) + "\ud83d\ude00", "two", "three", "four"}, false);
        successfulReadCase("literal-unpaired-units", FIRST,
                           new String[] {"left\ud800right", "left\udc00right", "\u00e9\u4e2d", "\u00a7"}, false);
        successfulReadCase("color-and-spacing", FIRST,
                           new String[] {"\u00a7aGreen", " leading trailing ", "\u00a7c12345678901234", ""}, false);
        successfulReadCase("missing-coordinates", null, NEW, false);
        successfulReadCase("unknown-input-and-existing-output", SECOND, NEW, true);
        for (int i = 0; i < 4; ++i) {
            name = "late-wrong-text-" + (i + 1);
            beginCase(name, "Actual successful first read supplies prior state, then actual wrong-type read; no continuation after the second failure");
            yk tile = construct(name, "constructor");
            need(read(name, "first-read", tile, input(FIRST, OLD, -1, -1, false)), "ordinary first-read preparation");
            read(name, "second-read", tile, input(SECOND, NEW, -1, i, false));
        }
        for (int i = 0; i < 3; ++i) {
            name = "late-wrong-coordinate-" + "xyz".charAt(i);
            beginCase(name, "Actual successful first read supplies prior coordinates/lines, then wrong coordinate tag; no private prestate");
            yk tile = construct(name, "constructor");
            need(read(name, "first-read", tile, input(FIRST, OLD, -1, -1, false)), "ordinary first-read preparation");
            read(name, "second-read", tile, input(SECOND, NEW, i, -1, false));
        }
        name = "null-nbt-read";
        beginCase(name, "Actual fresh receiver and null Compound argument; observe early private-c/base failure prefix");
        read(name, "read-null", construct(name, "constructor"), null);
        name = "null-nbt-write";
        beginCase(name, "Actual fresh receiver and null Compound argument; actual parent write failure");
        write(name, "write-null", construct(name, "constructor"), null);
        name = "writer-public-overlong-lines";
        beginCase(name, "Separate component-only public String[] assignment, essential to writer no-truncation branch; not a loaded-call preparation");
        yk tile = construct(name, "constructor");
        Object before = state(tile);
        tile.a = new String[] {repeated('A', 16), repeated('B', 31),
                               repeated('K', 14) + "\ud83d\ude00", "\u00a7a" + repeated('C', 14)};
        row("PUBLIC_PRESTATE", map("case", name, "fields", List.of("yk.a"), "source_call", false,
                                   "before", before, "after", state(tile)));
        serializeAndReadBack(name, tile);
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length == 1, "exact original archive path");
        archive = Path.of(args[0]).toRealPath();
        need(sha(Files.readAllBytes(archive)).equals(JAR_SHA), "unchanged original archive hash");
        for (Class<?> type : new Class<?>[] {yk.class, ow.class, nu.class, ij.class}) origin(type);
        row("BEGIN", map("schema", 1, "jar_sha256", JAR_SHA,
                         "scope", "Ordinary unchanged-original Sign constructor/NBT component methods only; no World, factory, GL, editor, native or save-file roundtrip qualification",
                         "vm", map("runtime", System.getProperty("java.runtime.version"),
                                   "vm", System.getProperty("java.vm.name"), "vm_version", System.getProperty("java.vm.version"),
                                   "os", System.getProperty("os.name"), "arch", System.getProperty("os.arch")),
                         "observer", "private fields are read only; linear aliases are output only; one declared public array prestate case"));
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) { }
            public void checkPermission(Permission permission, Object context) { }
            public void checkConnect(String host, int port) { ++networkAttempts; throw new SecurityException("network denied"); }
            public void checkConnect(String host, int port, Object context) { checkConnect(host, port); }
            public void checkListen(int port) { ++networkAttempts; throw new SecurityException("listen denied"); }
            public void checkAccept(String host, int port) { ++networkAttempts; throw new SecurityException("accept denied"); }
            public void checkLink(String name) { ++nativeAttempts; throw new SecurityException("native link denied"); }
            public void checkExec(String command) { ++execAttempts; throw new SecurityException("exec denied"); }
            public void checkWrite(String name) { ++writeAttempts; throw new SecurityException("write denied"); }
            public void checkWrite(FileDescriptor descriptor) {
                if (descriptor != FileDescriptor.out && descriptor != FileDescriptor.err) {
                    ++writeAttempts;
                    throw new SecurityException("descriptor write denied");
                }
            }
            public void checkDelete(String name) { ++deleteAttempts; throw new SecurityException("delete denied"); }
            public void checkExit(int status) { ++exitAttempts; throw new SecurityException("explicit exit denied"); }
        });
        profiles();
        need(cases.size() == 22 && constructorCalls == 35 && readCalls == 39 && writeCalls == 27,
             "complete declared caller inventory; unexpected positive-profile failure is not a smaller success");
        need(networkAttempts == 0 && nativeAttempts == 0 && execAttempts == 0 && writeAttempts == 0 &&
             deleteAttempts == 0 && exitAttempts == 0, "no denied external-work attempt counted as a result");
        row("COMPLETE", map("schema", 1, "cases", cases, "case_count", cases.size(),
                            "constructor_calls", constructorCalls, "read_calls", readCalls, "write_calls", writeCalls,
                            "rows_before_complete", rows, "class_origins", origins,
                            "guards", map("network", networkAttempts, "native_link", nativeAttempts,
                                          "exec", execAttempts, "write", writeAttempts, "delete", deleteAttempts,
                                          "explicit_exit", exitAttempts),
                            "completed_script", true));
        need(!System.out.checkError(), "complete stdout transport");
    }
}
