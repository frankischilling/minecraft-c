import java.io.ByteArrayOutputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/** Observes the unmodified saved-data ID manager; writes only beneath a fresh workroot. */
public final class SavedDataIdObservation {
    private static final int BYTE_LIMIT = 1 << 20;
    private static final int DIAGNOSTIC_LIMIT = 1 << 16;
    private static final char[] HEX = "0123456789abcdef".toCharArray();

    private interface Action<T> { T run() throws Exception; }

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static Object invoke(Method method, Object owner, Object... arguments) throws Exception {
        try {
            return method.invoke(owner, arguments);
        } catch (InvocationTargetException exception) {
            return rethrow(exception.getCause());
        }
    }

    private static Object construct(Constructor<?> constructor, Object... arguments) throws Exception {
        try {
            return constructor.newInstance(arguments);
        } catch (InvocationTargetException exception) {
            return rethrow(exception.getCause());
        }
    }

    private static Object rethrow(Throwable cause) throws Exception {
        if (cause instanceof Exception) throw (Exception) cause;
        if (cause instanceof Error) throw (Error) cause;
        throw new AssertionError(cause);
    }

    private static String hex(byte[] bytes) {
        char[] text = new char[bytes.length * 2];
        for (int i = 0; i < bytes.length; ++i) {
            int value = bytes[i] & 255;
            text[i * 2] = HEX[value >>> 4];
            text[i * 2 + 1] = HEX[value & 15];
        }
        return new String(text);
    }

    private static String units(String value) {
        if (value == null || value.isEmpty()) return "-";
        char[] text = new char[value.length() * 4];
        for (int i = 0; i < value.length(); ++i) {
            int unit = value.charAt(i);
            for (int j = 0; j < 4; ++j) text[i * 4 + j] = HEX[(unit >>> (12 - j * 4)) & 15];
        }
        return new String(text);
    }

    private static String hash(Path path) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream input = Files.newInputStream(path)) {
            byte[] buffer = new byte[65536];
            for (int count; (count = input.read(buffer)) != -1;) digest.update(buffer, 0, count);
        }
        return hex(digest.digest());
    }

    private static final class Binding {
        final URLClassLoader loader;
        final Path jar;
        final Class<?> manager, storage, handler, base, compound, shortTag, intTag, stringTag;
        final Constructor<?> managerConstructor, handlerConstructor, compoundConstructor;
        final Constructor<?> shortConstructor, intConstructor, stringConstructor;
        final Method next, filename, name, writeTag, writeRoot;
        final Field counters, storageField, sessionToken;

        Binding(URLClassLoader loader, Path jar, boolean client) throws Exception {
            this.loader = loader;
            this.jar = jar;
            manager = type(client ? "hc" : "es");
            storage = type(client ? "wt" : "om");
            handler = type(client ? "fm" : "dq");
            base = type(client ? "ij" : "fo");
            compound = type(client ? "nu" : "iq");
            shortTag = type(client ? "ul" : "mv");
            intTag = type(client ? "pp" : "jq");
            stringTag = type(client ? "xb" : "ou");
            Class<?> codec = type(client ? "as" : "ag");
            require(storage.isInterface() && storage.isAssignableFrom(handler), "storage binding");
            require(base.isAssignableFrom(compound) && base.isAssignableFrom(shortTag)
                    && base.isAssignableFrom(intTag) && base.isAssignableFrom(stringTag), "NBT binding");
            managerConstructor = constructor(manager, storage);
            handlerConstructor = constructor(handler, File.class, String.class, boolean.class);
            compoundConstructor = constructor(compound);
            shortConstructor = constructor(shortTag, short.class);
            intConstructor = constructor(intTag, int.class);
            stringConstructor = constructor(stringTag, String.class);
            next = method(manager, "a", int.class, false, String.class);
            filename = method(storage, client ? "a" : "b", File.class, false, String.class);
            name = method(base, "a", base, false, String.class);
            writeTag = method(base, "a", void.class, true, base, DataOutput.class);
            writeRoot = method(codec, "a", void.class, true, compound, DataOutput.class);
            counters = field(manager, "d", Map.class, false);
            storageField = field(manager, "a", storage, false);
            sessionToken = field(handler, "e", long.class, true);
        }

        private Class<?> type(String name) throws Exception {
            Class<?> result = Class.forName(name, false, loader);
            require(result.getClassLoader() == loader, "foreign class loader: " + name);
            require(result.getProtectionDomain().getCodeSource() != null
                    && Path.of(result.getProtectionDomain().getCodeSource().getLocation().toURI())
                            .toRealPath().equals(jar), "foreign code source: " + name);
            return result;
        }

        private static Constructor<?> constructor(Class<?> owner, Class<?>... arguments) throws Exception {
            Constructor<?> result = owner.getDeclaredConstructor(arguments);
            require(Modifier.isPublic(result.getModifiers()), "constructor visibility");
            return result;
        }

        private static Method method(Class<?> owner, String name, Class<?> resultType,
                                     boolean isStatic, Class<?>... arguments) throws Exception {
            Method result = owner.getDeclaredMethod(name, arguments);
            require(result.getReturnType() == resultType && Modifier.isPublic(result.getModifiers())
                    && Modifier.isStatic(result.getModifiers()) == isStatic, "method binding: " + name);
            return result;
        }

        private static Field field(Class<?> owner, String name, Class<?> type, boolean isFinal)
                throws Exception {
            Field result = owner.getDeclaredField(name);
            require(result.getType() == type && Modifier.isPrivate(result.getModifiers())
                    && !Modifier.isStatic(result.getModifiers())
                    && Modifier.isFinal(result.getModifiers()) == isFinal, "field binding: " + name);
            result.setAccessible(true);
            return result;
        }
    }

    private static final class LimitedBytes extends ByteArrayOutputStream {
        @Override public synchronized void write(int value) {
            require(count < DIAGNOSTIC_LIMIT, "diagnostic bound");
            super.write(value);
        }

        @Override public synchronized void write(byte[] bytes, int offset, int length) {
            require(length >= 0 && length <= DIAGNOSTIC_LIMIT - count, "diagnostic bound");
            super.write(bytes, offset, length);
        }
    }

    private static final class Owner {
        final Object value;
        final int id;
        int calls;

        Owner(Object value, int id) { this.value = value; this.id = id; }
    }

    private final Binding binding;
    private final Path root;
    private final StringBuilder output = new StringBuilder();
    private final IdentityHashMap<String, Integer> keyIds = new IdentityHashMap<>();
    private final List<Owner> owners = new ArrayList<>();
    private final List<Object> stores = new ArrayList<>();
    private int rows, cases, maps, files, paths, diagnostics, nextRows, nextCalls;
    private String active;

    private SavedDataIdObservation(Binding binding, Path root) throws Exception {
        this.binding = binding;
        this.root = root;
        Files.createDirectory(root.resolve("snapshots"));
        Files.createDirectory(root.resolve("diagnostics"));
        Files.createDirectory(root.resolve("session-created"));
    }

    private void row(Object... fields) {
        require(rows < 4096, "row bound");
        for (int i = 0; i < fields.length; ++i) {
            String text = String.valueOf(fields[i]);
            for (int j = 0; j < text.length(); ++j)
                require(text.charAt(j) >= 32 && text.charAt(j) <= 126, "non-ASCII field");
            require(output.length() + text.length() + (i == 0 ? 1 : 2) <= BYTE_LIMIT,
                    "protocol byte bound");
            if (i != 0) output.append('\t');
            output.append(text);
        }
        output.append('\n');
        ++rows;
    }

    private int key(String value) {
        if (value == null) return 0;
        Integer old = keyIds.get(value);
        if (old != null) return old;
        require(keyIds.size() < 128 && value.length() <= 21846, "key bound");
        int id = keyIds.size() + 1;
        keyIds.put(value, id);
        row("KEY", id, value.length(), units(value));
        return id;
    }

    private static String diagnosticClass(byte[] bytes) {
        if (bytes.length == 0) return "-";
        String text = new String(bytes, StandardCharsets.UTF_8);
        int end = 0;
        while (end < text.length() && text.charAt(end) != ':' && text.charAt(end) != '\r'
                && text.charAt(end) != '\n') ++end;
        String result = text.substring(0, end);
        require(result.matches("[A-Za-z_$][A-Za-z0-9_.$]*"), "diagnostic first line");
        return result;
    }

    private <T> T observe(String label, Action<T> action) throws Exception {
        require(active == null && diagnostics < 128, "nested observation or diagnostic bound");
        LimitedBytes bytes = new LimitedBytes();
        PrintStream previous = System.err;
        PrintStream capture = new PrintStream(bytes, true, StandardCharsets.UTF_8);
        active = label;
        System.setErr(capture);
        try {
            return action.run();
        } finally {
            capture.flush();
            boolean failed = capture.checkError();
            System.setErr(previous);
            active = null;
            capture.close();
            require(!failed, "diagnostic output failure");
            byte[] raw = bytes.toByteArray();
            String relative = "diagnostics/" + label + ".stderr";
            Files.write(root.resolve(relative), raw, StandardOpenOption.CREATE_NEW);
            row("DIAG", label, diagnosticClass(raw), raw.length, relative);
            ++diagnostics;
        }
    }

    private Path caseDirectory(String label) throws Exception {
        require(cases < 16, "case bound");
        ++cases;
        row("CASE", label);
        return Files.createDirectory(root.resolve(label));
    }

    private Object realStorage(String label, Path directory) throws Exception {
        Object store = observe(label + ".store", () -> construct(binding.handlerConstructor,
                directory.getParent().toFile(), directory.getFileName().toString(), false));
        stores.add(store);
        byte[] raw = readBounded(directory.resolve("session.lock"));
        require(raw.length == Long.BYTES && ByteBuffer.wrap(raw).getLong()
                == binding.sessionToken.getLong(store), "actual session token/file mismatch");
        String relative = "session-created/" + label + ".bin";
        Files.write(root.resolve(relative), raw, StandardOpenOption.CREATE_NEW);
        row("STORAGE", label, "REAL");
        row("SESSION_CAPTURE", label, relative, raw.length);
        return store;
    }

    private Object proxyStorage(String label, Path directory) throws Exception {
        Files.createDirectory(directory.resolve("data"));
        Object store = Proxy.newProxyInstance(binding.loader, new Class<?>[]{binding.storage},
                (proxy, method, arguments) -> {
                    require(method.equals(binding.filename), "unexpected storage method: " + method);
                    require(active != null && arguments != null && arguments.length == 1
                            && arguments[0] instanceof String, "filename arguments");
                    String supplied = (String) arguments[0];
                    require(supplied.equals("idcounts") && paths < 128, "unexpected filename or path bound");
                    String relative = label + "/data/" + supplied + ".dat";
                    row("PATH", label, active, ++paths, supplied.length(), units(supplied), relative);
                    return root.resolve(relative).toFile();
                });
        stores.add(store);
        row("STORAGE", label, "PROXY_PATH_ONLY");
        return store;
    }

    private Owner owner(String label, Object store) throws Exception {
        require(owners.size() < 32, "owner bound");
        Object value = observe(label, () -> construct(binding.managerConstructor, store));
        require(binding.storageField.get(value) == store, "storage reference changed");
        for (Owner previous : owners) require(previous.value != value, "owner identity reused");
        Owner result = new Owner(value, owners.size() + 1);
        owners.add(result);
        row("NEW", label, result.id, store == null ? 0 : 1);
        return result;
    }

    private Map<?, ?> counters(Owner owner) throws Exception {
        Object value = binding.counters.get(owner.value);
        require(value != null && value.getClass() == HashMap.class, "counter map binding");
        return (Map<?, ?>) value;
    }

    private void map(String label, Owner owner) throws Exception {
        Map<?, ?> map = counters(owner);
        require(map.size() <= 16, "map bound");
        List<String> names = new ArrayList<>();
        for (Object name : map.keySet()) {
            require(name == null || name instanceof String, "counter key type");
            names.add((String) name);
        }
        // Only the display is canonical. No serialized file bytes are reordered.
        names.sort(Comparator.nullsFirst(Comparator.naturalOrder()));
        for (String name : names) key(name);
        row("MAP", label, owner.id, map.size());
        for (int i = 0; i < names.size(); ++i) {
            String name = names.get(i);
            Object value = map.get(name);
            require(value != null && value.getClass() == Short.class, "counter value type");
            row("VALUE", label, owner.id, i, key(name), ((Short) value).shortValue());
        }
        ++maps;
    }

    private int rawNext(String label, Owner owner, String name, boolean emit) throws Exception {
        int keyId = key(name);
        Object result = invoke(binding.next, owner.value, (Object) name);
        require(result instanceof Integer, "next return type");
        int actual = (Integer) result;
        require(actual >= Short.MIN_VALUE && actual <= Short.MAX_VALUE, "next signed-short range");
        int index = owner.calls++;
        ++nextCalls;
        if (emit) {
            row("NEXT", label, owner.id, index, keyId, actual);
            ++nextRows;
        }
        return actual;
    }

    private void next(String label, Owner owner, String name) throws Exception {
        key(name);
        observe(label, () -> rawNext(label, owner, name, true));
        map(label, owner);
    }

    private static byte[] readBounded(Path path) throws Exception {
        require(Files.isRegularFile(path, LinkOption.NOFOLLOW_LINKS)
                && Files.size(path) <= BYTE_LIMIT, "regular file/size bound");
        byte[] bytes = Files.readAllBytes(path);
        require(bytes.length <= BYTE_LIMIT, "file grew past bound");
        return bytes;
    }

    private void file(String label, Path path) throws Exception {
        require(files < 128 && path.normalize().startsWith(root), "file scope/bound");
        String relative = root.relativize(path).toString().replace(File.separatorChar, '/');
        if (!Files.exists(path, LinkOption.NOFOLLOW_LINKS)) {
            row("FILE", label, relative, "MISSING", 0, "-");
        } else if (Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) {
            row("FILE", label, relative, "DIRECTORY", 0, "-");
        } else {
            byte[] bytes = readBounded(path);
            Files.write(root.resolve("snapshots/" + label + ".bin"), bytes,
                    StandardOpenOption.CREATE_NEW);
            row("FILE", label, relative, "REGULAR", bytes.length,
                    bytes.length == 0 ? "-" : hex(bytes));
        }
        ++files;
    }

    private Object tag(Constructor<?> constructor, Object value, String name) throws Exception {
        Object tag = construct(constructor, value);
        require(invoke(binding.name, tag, (Object) name) == tag, "name setter identity");
        return tag;
    }

    private void prepare(Path path, String unicode) throws Exception {
        Object rootTag = construct(binding.compoundConstructor);
        invoke(binding.name, rootTag, "prepared");
        ByteArrayOutputStream framingBytes = new ByteArrayOutputStream();
        try (DataOutputStream framing = new DataOutputStream(framingBytes)) {
            invoke(binding.writeRoot, null, rootTag, framing);
        }
        byte[] emptyRoot = framingBytes.toByteArray();
        require(emptyRoot.length > 1 && emptyRoot[emptyRoot.length - 1] == 0, "empty compound framing");
        try (DataOutputStream output = new DataOutputStream(Files.newOutputStream(path,
                StandardOpenOption.CREATE_NEW))) {
            output.write(emptyRoot, 0, emptyRoot.length - 1);
            Object[] tags = {
                tag(binding.shortConstructor, (short) 32767, "Aa"),
                tag(binding.intConstructor, 0x12345678, "skip"),
                tag(binding.shortConstructor, (short) 7, "dup"),
                tag(binding.shortConstructor, (short) -8, "dup"),
                tag(binding.shortConstructor, (short) 99, "gone"),
                tag(binding.stringConstructor, "not-short", "gone"),
                tag(binding.shortConstructor, (short) -32768, unicode),
                tag(binding.shortConstructor, (short) -1, "BB")
            };
            for (Object tag : tags) invoke(binding.writeTag, null, tag, output);
            output.writeByte(emptyRoot[emptyRoot.length - 1]);
            output.write(new byte[]{(byte) 0xde, (byte) 0xad, (byte) 0xc0, (byte) 0xde});
        }
    }

    private static String text(String value) { return new String(value); }

    private void run(String side) throws Exception {
        row("SAVED_DATA_IDS_V1", side);
        row("KEY", 0, -1, "-");

        caseDirectory("null-store");
        Owner noStore = owner("null-store.new", null);
        map("null-store.initial", noStore);
        String nullStoreKey = text("solo");
        key(nullStoreKey);
        observe("null-store.draws", () -> {
            for (int i = 0; i < 65538; ++i) {
                boolean selected = i <= 1 || (i >= 32766 && i <= 32769) || i >= 65534;
                rawNext("null-store.draw", noStore, nullStoreKey, selected);
            }
            return null;
        });
        map("null-store.after-draws", noStore);
        next("null-store.continue", noStore, text("solo"));
        next("null-store.null", noStore, null);
        next("null-store.empty", noStore, text(""));
        String memoryUnicode = new String(new char[]{0, '\ud800', 'n', '\udc00', '\uffff'});
        next("null-store.unicode", noStore, memoryUnicode);
        next("null-store.equal-unicode", noStore, new String(memoryUnicode));

        Path directory = caseDirectory("missing");
        Object store = realStorage("missing", directory);
        Path data = directory.resolve("data/idcounts.dat");
        Owner current = owner("missing.new", store);
        map("missing.initial", current);
        file("missing.before", data);
        next("missing.next", current, text("solo"));
        file("missing.after", data);

        directory = caseDirectory("prepared");
        store = realStorage("prepared", directory);
        data = directory.resolve("data/idcounts.dat");
        String unicode = new String(new char[]{'x', 0, '\ud800', '\udc00', '\udfff', '\uffff'});
        prepare(data, unicode);
        file("prepared.input", data);
        current = owner("prepared.new", store);
        map("prepared.loaded", current);
        file("prepared.loaded-file", data);
        next("prepared.next-Aa", current, text("Aa"));
        file("prepared.after-Aa", data);
        next("prepared.next-unicode", current, unicode);
        file("prepared.after-unicode", data);

        directory = caseDirectory("raw-reload");
        store = realStorage("raw-reload", directory);
        data = directory.resolve("data/idcounts.dat");
        current = owner("raw-reload.first", store);
        map("raw-reload.initial", current);
        String aa = text("Aa"), bb = text("BB");
        next("raw-reload.Aa0", current, aa);
        file("raw-reload.Aa0", data);
        next("raw-reload.BB0", current, bb);
        file("raw-reload.BB0", data);
        next("raw-reload.Aa1", current, aa);
        file("raw-reload.Aa1", data);
        Owner second = owner("raw-reload.second", store);
        map("raw-reload.reloaded", second);
        next("raw-reload.BB1", second, bb);
        file("raw-reload.BB1", data);
        map("raw-reload.first-retained", current);

        directory = caseDirectory("stale-session");
        store = realStorage("stale-session", directory);
        data = directory.resolve("data/idcounts.dat");
        Path lock = directory.resolve("session.lock");
        long actualToken = binding.sessionToken.getLong(store);
        long staleToken = 0x0123456789abcdefL;
        require(actualToken != staleToken, "stale-session test input equals actual token");
        try (DataOutputStream output = new DataOutputStream(Files.newOutputStream(lock))) {
            output.writeLong(staleToken);
        }
        file("stale-session.before", lock);
        current = owner("stale-session.new", store);
        map("stale-session.initial", current);
        next("stale-session.next", current, text("solo"));
        file("stale-session.ids", data);
        file("stale-session.after", lock);

        directory = caseDirectory("two-owners");
        store = realStorage("two-owners", directory);
        data = directory.resolve("data/idcounts.dat");
        current = owner("two-owners.first", store);
        second = owner("two-owners.second", store);
        row("REL", "two-owners.same-counter-map", counters(current) == counters(second) ? 1 : 0);
        map("two-owners.first-initial", current);
        map("two-owners.second-initial", second);
        String sharedKey = text("solo");
        next("two-owners.A0", current, sharedKey);
        file("two-owners.A0", data);
        next("two-owners.B0", second, sharedKey);
        file("two-owners.B0", data);
        next("two-owners.A1", current, sharedKey);
        file("two-owners.A1", data);
        map("two-owners.B-still0", second);
        next("two-owners.B1", second, sharedKey);
        file("two-owners.B1", data);
        next("two-owners.A2", current, sharedKey);
        file("two-owners.A2", data);
        map("two-owners.B-still1", second);

        directory = caseDirectory("malformed");
        store = realStorage("malformed", directory);
        data = directory.resolve("data/idcounts.dat");
        Files.write(data, new byte[]{10}, StandardOpenOption.CREATE_NEW);
        file("malformed.input", data);
        current = owner("malformed.new", store);
        map("malformed.initial", current);
        file("malformed.after-load", data);
        next("malformed.next", current, text("solo"));
        file("malformed.after-next", data);

        directory = caseDirectory("directory");
        store = realStorage("directory", directory);
        data = Files.createDirectory(directory.resolve("data/idcounts.dat"));
        file("directory.input", data);
        current = owner("directory.new", store);
        map("directory.initial", current);
        next("directory.next", current, text("solo"));
        file("directory.after", data);

        directory = caseDirectory("null-empty");
        store = realStorage("null-empty", directory);
        data = directory.resolve("data/idcounts.dat");
        current = owner("null-empty.first", store);
        map("null-empty.initial", current);
        String empty = text("");
        next("null-empty.null0", current, null);
        file("null-empty.null0", data);
        next("null-empty.empty0", current, empty);
        file("null-empty.empty0", data);
        next("null-empty.null1", current, null);
        file("null-empty.null1", data);
        second = owner("null-empty.second", store);
        map("null-empty.reloaded", second);
        next("null-empty.reloaded-next", second, empty);
        file("null-empty.reloaded-next", data);
        map("null-empty.first-retained", current);

        directory = caseDirectory("long-utf");
        store = realStorage("long-utf", directory);
        data = directory.resolve("data/idcounts.dat");
        current = owner("long-utf.first", store);
        next("long-utf.solo", current, text("solo"));
        file("long-utf.before", data);
        String longName = "\u0800".repeat(21846);
        next("long-utf.first-failure", current, longName);
        file("long-utf.partial1", data);
        next("long-utf.second-failure", current, longName);
        file("long-utf.partial2", data);
        second = owner("long-utf.reload", store);
        map("long-utf.reload", second);
        map("long-utf.first-retained", current);
        file("long-utf.after-reload", data);

        directory = caseDirectory("proxy-path");
        store = proxyStorage("proxy-path", directory);
        data = directory.resolve("data/idcounts.dat");
        current = owner("proxy-path.new", store);
        map("proxy-path.initial", current);
        file("proxy-path.before", data);
        next("proxy-path.next", current, text("solo"));
        file("proxy-path.after", data);
        second = owner("proxy-path.reload", store);
        map("proxy-path.reloaded", second);

        row("END", cases, owners.size(), nextCalls, nextRows, maps, files, paths,
                diagnostics, keyIds.size() + 1, rows);
    }

    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 3, "usage: <client|server> <fresh-workroot> <original-jar>");
        boolean client = arguments[0].equals("client");
        require(client || arguments[0].equals("server"), "side");
        Path root = Path.of(arguments[1]).toAbsolutePath().normalize();
        require(root.getParent() != null && Files.isDirectory(root.getParent()), "workroot parent");
        Path jar = Path.of(arguments[2]).toRealPath();
        long expectedSize = client ? 1465375 : 503100;
        String expectedHash = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        require(Files.size(jar) == expectedSize && hash(jar).equals(expectedHash), "original JAR pin");
        Files.createDirectory(root);
        SavedDataIdObservation observer;
        try (URLClassLoader loader = new URLClassLoader(new URL[]{jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            observer = new SavedDataIdObservation(new Binding(loader, jar, client), root);
            observer.run(arguments[0]);
        }
        require(Files.size(jar) == expectedSize && hash(jar).equals(expectedHash), "original JAR changed");
        byte[] raw = observer.output.toString().getBytes(StandardCharsets.US_ASCII);
        require(raw.length <= BYTE_LIMIT && raw.length != 0 && raw[raw.length - 1] == 10, "protocol bound/LF");
        System.out.write(raw);
        System.out.flush();
        require(!System.out.checkError(), "stdout failure");
    }
}
