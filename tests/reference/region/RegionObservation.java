import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Reads untouched original region methods over separately owned fixture copies.
 * The original constructor may mutate its copy. No world or chunk loader is
 * constructed, and root parsing and stream draining use separate actual reads.
 */
public final class RegionObservation {
    private static final int MAX_REQUESTS = 256;
    private static final int MAX_REQUEST_BYTES = 512 * 1024;
    private static final int MAX_LINE_BYTES = 8192;
    private static final int MAX_FILE_BYTES = 65536;
    private static final int MAX_STREAM_BYTES = 1024 * 1024;
    private static final int MAX_JSON_BYTES = 32 * 1024 * 1024;
    private static final String CLIENT_SHA =
            "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    private static final String SERVER_SHA =
            "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";

    private static final class ObservationFailure extends RuntimeException {
        private static final long serialVersionUID = 1L;
        ObservationFailure(String message) { super(message); }
    }

    private static final class OriginalException extends Exception {
        private static final long serialVersionUID = 1L;
        final Exception original;
        OriginalException(Exception cause) { super(cause); original = cause; }
    }

    private static void require(boolean condition, String message) {
        if (!condition) throw new ObservationFailure(message);
    }

    private static String hex(byte[] value) {
        char[] result = new char[value.length * 2];
        char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < value.length; ++i) {
            result[i * 2] = alphabet[(value[i] & 255) >>> 4];
            result[i * 2 + 1] = alphabet[value[i] & 15];
        }
        return new String(result);
    }

    private static String hash(byte[] value) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(value));
    }

    private static Map<String, Object> object(Object... entries) {
        require(entries.length % 2 == 0, "JSON object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < entries.length; i += 2) {
            require(entries[i] instanceof String && !result.containsKey(entries[i]), "unique JSON key");
            result.put((String) entries[i], entries[i + 1]);
        }
        return result;
    }

    private static Map<String, Object> fingerprint(byte[] value) throws Exception {
        return object("bytes", value.length, "sha256", hash(value));
    }

    private static void append(StringBuilder target, String value) {
        require(value.length() <= MAX_JSON_BYTES - target.length(), "complete JSON output bound");
        target.append(value);
    }

    private static void json(StringBuilder target, Object value) {
        if (value == null) { append(target, "null"); return; }
        if (value instanceof String) {
            append(target, "\"");
            String text = (String) value;
            for (int i = 0; i < text.length(); ++i) {
                char c = text.charAt(i);
                if (c == '"' || c == '\\') append(target, "\\" + c);
                else if (c < 32 || c > 126) append(target, String.format(Locale.ROOT, "\\u%04x", (int) c));
                else append(target, Character.toString(c));
            }
            append(target, "\"");
            return;
        }
        if (value instanceof Boolean || value instanceof Integer || value instanceof Long) {
            append(target, value.toString());
            return;
        }
        if (value instanceof Map<?, ?>) {
            append(target, "{");
            boolean first = true;
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                if (!first) append(target, ",");
                first = false;
                json(target, entry.getKey()); append(target, ":"); json(target, entry.getValue());
            }
            append(target, "}");
            return;
        }
        throw new ObservationFailure("unsupported JSON value");
    }

    private static byte[] boundedFile(Path file, int limit) throws IOException {
        require(Files.isRegularFile(file, LinkOption.NOFOLLOW_LINKS), "ordinary file required: " + file);
        require(Files.size(file) <= limit, "file input bound: " + file);
        try (InputStream input = Files.newInputStream(file)) {
            byte[] data = input.readNBytes(limit + 1);
            require(data.length <= limit, "file grew beyond bound: " + file);
            return data;
        }
    }

    private static Path ordinary(Path input, boolean directory) throws IOException {
        require(input.isAbsolute() && input.equals(input.normalize()), "absolute normalized path required");
        Path current = input.getRoot();
        for (Path part : input) {
            current = current.resolve(part);
            require(!Files.isSymbolicLink(current), "symlink path component: " + current);
        }
        Path real = input.toRealPath();
        require(real.equals(input), "canonical nonalias path required: " + input);
        require(directory ? Files.isDirectory(real, LinkOption.NOFOLLOW_LINKS)
                          : Files.isRegularFile(real, LinkOption.NOFOLLOW_LINKS), "ordinary path type");
        return real;
    }

    private static void singleLink(Path file) throws IOException {
        if (file.getFileSystem().supportedFileAttributeViews().contains("unix")) {
            Object count = Files.getAttribute(file, "unix:nlink", LinkOption.NOFOLLOW_LINKS);
            require(count instanceof Number && ((Number) count).longValue() == 1, "owned single-link fixture");
        }
    }

    private static final class Request {
        final Path path;
        final int x, z;
        final byte[] initial;
        Request(Path path, int x, int z, byte[] initial) {
            this.path = path; this.x = x; this.z = z; this.initial = initial;
        }
    }

    private static List<Request> requests(Path root, Path input, Path jar) throws Exception {
        byte[] encoded = boundedFile(input, MAX_REQUEST_BYTES);
        String text = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(encoded)).toString();
        require(text.endsWith("\n"), "request file must end with newline");
        String[] lines = text.split("\n", -1);
        require(lines.length >= 2 && lines.length <= MAX_REQUESTS + 1, "request count bound");
        List<Request> result = new ArrayList<>();
        for (int i = 0; i < lines.length - 1; ++i) {
            String line = lines[i];
            if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
            require(!line.isEmpty() && line.getBytes(StandardCharsets.UTF_8).length <= MAX_LINE_BYTES,
                    "request line bound");
            String[] fields = line.split("\t", -1);
            require(fields.length == 3 && fields[1].matches("-?(0|[1-9][0-9]*)")
                    && fields[2].matches("-?(0|[1-9][0-9]*)"), "request fields");
            Path file = ordinary(Path.of(fields[0]), false);
            require(!file.equals(root) && file.startsWith(root) && !Files.isSameFile(file, input)
                    && !Files.isSameFile(file, jar), "fixture must be an owned descendant");
            singleLink(file);
            for (Request prior : result) require(!Files.isSameFile(file, prior.path), "distinct per-query copies");
            byte[] bytes = boundedFile(file, MAX_FILE_BYTES - 1);
            result.add(new Request(file, Integer.parseInt(fields[1]), Integer.parseInt(fields[2]), bytes));
        }
        return result;
    }

    private static OriginalException original(InvocationTargetException failure) {
        Throwable cause = failure.getCause();
        if (cause instanceof ObservationFailure) throw (ObservationFailure) cause;
        if (cause instanceof Error) throw (Error) cause;
        require(cause instanceof Exception, "unexpected original throwable");
        return new OriginalException((Exception) cause);
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) { throw original(failure); }
    }

    private static Map<String, Object> exception(Exception cause) {
        return object("status", "exception", "class", cause.getClass().getName());
    }

    private static final class BoundedInput extends FilterInputStream {
        private int consumed;
        BoundedInput(InputStream input) { super(input); }
        private void count(long value) {
            require(value >= 0 && value <= MAX_STREAM_BYTES - consumed, "decompressed stream bound");
            consumed += (int) value;
        }
        @Override public int read() throws IOException {
            int value = in.read(); if (value >= 0) count(1); return value;
        }
        @Override public int read(byte[] data, int offset, int length) throws IOException {
            int allowed = Math.min(length, MAX_STREAM_BYTES - consumed + 1);
            int value = in.read(data, offset, allowed); if (value > 0) count(value); return value;
        }
        @Override public long skip(long length) throws IOException {
            long value = in.skip(Math.min(length, (long) MAX_STREAM_BYTES - consumed + 1));
            count(value); return value;
        }
    }

    private static final class BoundedOutput extends OutputStream {
        final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        @Override public void write(int value) {
            require(bytes.size() < MAX_STREAM_BYTES, "canonical root bound"); bytes.write(value);
        }
        @Override public void write(byte[] data, int offset, int length) {
            require(length <= MAX_STREAM_BYTES - bytes.size(), "canonical root bound");
            bytes.write(data, offset, length);
        }
    }

    private static final class Model {
        final Class<?> regionType, utilityType, compoundType;
        final Constructor<?> constructor;
        final Method read, present, close, rawRead, rawWrite;
        final Field locations, timestamps, free, growth, stream, file;
        final Map<String, Object> origins;

        private static Class<?> type(String name, URLClassLoader loader, Path jar) throws Exception {
            Class<?> type = Class.forName(name, true, loader);
            require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null,
                    "isolated original class: " + name);
            Path origin = Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            require(origin.equals(jar), "original class origin: " + name);
            return type;
        }

        private static Map<String, Object> origin(Class<?> type) throws Exception {
            return object("name", type.getName(), "origin",
                    type.getProtectionDomain().getCodeSource().getLocation().toURI().toASCIIString(),
                    "loader", "isolated");
        }

        private static Method method(Class<?> owner, String name, Class<?> result, int modifiers,
                                     Class<?>... arguments) throws Exception {
            Method method = owner.getDeclaredMethod(name, arguments);
            require(method.getDeclaringClass() == owner && method.getReturnType() == result
                    && method.getModifiers() == modifiers, "exact original method: " + owner.getName() + "." + name);
            method.setAccessible(true);
            return method;
        }

        private static Field field(Class<?> owner, String name, Class<?> type, int modifiers) throws Exception {
            Field field = owner.getDeclaredField(name);
            require(field.getDeclaringClass() == owner && field.getType() == type
                    && field.getModifiers() == modifiers, "exact original field: " + owner.getName() + "." + name);
            field.setAccessible(true);
            return field;
        }

        Model(URLClassLoader loader, Path jar, boolean client) throws Exception {
            require(loader.getParent() == ClassLoader.getPlatformClassLoader(), "platform loader parent");
            regionType = type(client ? "qj" : "jx", loader, jar);
            utilityType = type(client ? "as" : "ag", loader, jar);
            compoundType = type(client ? "nu" : "iq", loader, jar);
            constructor = regionType.getDeclaredConstructor(File.class);
            require(constructor.getDeclaringClass() == regionType && constructor.getModifiers() == 1,
                    "exact public original region constructor");
            read = method(regionType, "a", DataInputStream.class, 33, int.class, int.class);
            present = method(regionType, "c", boolean.class, 1, int.class, int.class);
            close = method(regionType, "b", void.class, 1);
            rawRead = method(utilityType, "a", compoundType, 9, DataInput.class);
            rawWrite = method(utilityType, "a", void.class, 9, compoundType, DataOutput.class);
            locations = field(regionType, "d", int[].class, 18);
            timestamps = field(regionType, "e", int[].class, 18);
            free = field(regionType, "f", ArrayList.class, 2);
            growth = field(regionType, "g", int.class, 2);
            stream = field(regionType, "c", RandomAccessFile.class, 2);
            file = field(regionType, "b", File.class, 18);
            origins = object("region", origin(regionType), "utility", origin(utilityType), "compound", origin(compoundType));
        }

        String integers(Field field, Object receiver) throws Exception {
            int[] values = (int[]) field.get(receiver);
            require(values != null && values.length == 1024, "complete original region header array");
            byte[] bytes = new byte[4096];
            for (int i = 0; i < values.length; ++i)
                for (int j = 0; j < 4; ++j) bytes[i * 4 + j] = (byte) (values[i] >>> (24 - j * 8));
            return hex(bytes);
        }

        Object root(Object region) throws Exception {
            try {
                Object returned = invoke(read, region, currentX, currentZ);
                if (returned == null) return object("status", "null");
                require(returned.getClass() == DataInputStream.class, "actual original read stream type");
                try (DataInputStream input = new DataInputStream(new BoundedInput((DataInputStream) returned))) {
                    Object tag = invoke(rawRead, null, input);
                    require(tag != null && tag.getClass() == compoundType && tag.getClass().getClassLoader()
                            == compoundType.getClassLoader(), "actual original root receiver");
                    BoundedOutput output = new BoundedOutput();
                    invoke(rawWrite, null, tag, new DataOutputStream(output));
                    return object("status", "root", "canonical_hex", hex(output.bytes.toByteArray()));
                }
            } catch (OriginalException failure) { return exception(failure.original); }
            catch (IOException failure) { return exception(failure); }
        }

        Object drain(Object region) throws Exception {
            try {
                Object returned = invoke(read, region, currentX, currentZ);
                if (returned == null) return object("status", "null");
                require(returned.getClass() == DataInputStream.class, "actual original read stream type");
                try (InputStream input = new BoundedInput((DataInputStream) returned)) {
                    ByteArrayOutputStream output = new ByteArrayOutputStream();
                    byte[] buffer = new byte[4096];
                    int count;
                    while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
                    return object("status", "drain", "hex", hex(output.toByteArray()));
                }
            } catch (OriginalException failure) { return exception(failure.original); }
            catch (IOException failure) { return exception(failure); }
        }

        private int currentX, currentZ;

        Map<String, Object> observe(int index, Request request) throws Exception {
            Path actual = ordinary(request.path, false);
            byte[] before = boundedFile(actual, MAX_FILE_BYTES - 1);
            require(java.util.Arrays.equals(before, request.initial), "fixture changed after preflight");
            currentX = request.x; currentZ = request.z;
            Object region = null;
            Object constructorResult;
            try { region = constructor.newInstance(actual.toFile()); constructorResult = object("status", "ok"); }
            catch (InvocationTargetException failure) { constructorResult = exception(original(failure).original); }
            Map<String, Object> result = object("index", index, "x", request.x, "z", request.z,
                    "input_before", fingerprint(before), "constructor", constructorResult,
                    "locations", null, "timestamps", null, "growth", null, "free_sectors", null,
                    "present", null, "root", null, "drain", null, "close", object("status", "not_created"));
            if (region != null) {
                try {
                    require(region.getClass() == regionType && region.getClass().getClassLoader()
                            == regionType.getClassLoader(), "actual original region receiver");
                    Object backing = file.get(region), opened = stream.get(region), sectors = free.get(region);
                    require(backing != null && backing.getClass() == File.class
                            && ((File) backing).toPath().toRealPath().equals(actual), "retained fixture file identity");
                    require(opened != null && opened.getClass() == RandomAccessFile.class, "original file stream relationship");
                    require(sectors != null && sectors.getClass() == ArrayList.class
                            && ((ArrayList<?>) sectors).size() <= 32, "bounded original free sector list");
                    StringBuilder bits = new StringBuilder();
                    for (Object value : (ArrayList<?>) sectors) {
                        require(value != null && value.getClass() == Boolean.class, "actual free-sector boolean");
                        bits.append((Boolean) value ? '1' : '0');
                    }
                    result.put("locations", integers(locations, region));
                    result.put("timestamps", integers(timestamps, region));
                    result.put("growth", growth.getInt(region));
                    result.put("free_sectors", bits.toString());
                    try { result.put("present", object("status", "ok", "value", invoke(present, region, request.x, request.z))); }
                    catch (OriginalException failure) { result.put("present", exception(failure.original)); }
                    result.put("root", root(region));
                    result.put("drain", drain(region));
                } finally {
                    try { invoke(close, region); result.put("close", object("status", "ok")); }
                    catch (OriginalException failure) { result.put("close", exception(failure.original)); }
                }
            }
            ordinary(actual, false);
            result.put("input_after", fingerprint(boundedFile(actual, MAX_FILE_BYTES * 2)));
            return result;
        }
    }

    private static void run(String[] args) throws Exception {
        require(args.length == 5 && (args[1].equals("client") || args[1].equals("server")),
                "arguments: jar side copies_root requests_tsv output_json");
        Path jar = ordinary(Path.of(args[0]), false), root = ordinary(Path.of(args[2]), true);
        Path requests = ordinary(Path.of(args[3]), false), output = Path.of(args[4]);
        require(root.getParent() != null && !jar.startsWith(root), "owned copies root separate from original JAR");
        require(output.isAbsolute() && output.equals(output.normalize()) && output.getParent() != null,
                "absolute normalized output path");
        ordinary(output.getParent(), true);
        require(!output.startsWith(root) && !Files.exists(output, LinkOption.NOFOLLOW_LINKS),
                "new output outside mutable fixture copies");
        byte[] jarBytes = boundedFile(jar, 2 * 1024 * 1024);
        boolean client = args[1].equals("client");
        require(hash(jarBytes).equals(client ? CLIENT_SHA : SERVER_SHA), "pinned supplied JAR hash");
        List<Request> planned = requests(root, requests, jar);
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            Model model = new Model(loader, jar, client);
            StringBuilder encoded = new StringBuilder();
            Map<String, Object> header = object("schema", 1, "side", args[1], "jar", fingerprint(jarBytes),
                    "platform_parent", true, "class_origins", model.origins);
            json(encoded, header);
            encoded.setLength(encoded.length() - 1);
            append(encoded, ",\"records\":[");
            for (int i = 0; i < planned.size(); ++i) {
                if (i > 0) append(encoded, ",");
                json(encoded, model.observe(i, planned.get(i)));
            }
            append(encoded, "]}\n");
            require(hash(boundedFile(jar, 2 * 1024 * 1024)).equals(client ? CLIENT_SHA : SERVER_SHA),
                    "original JAR preserved");
            try (OutputStream target = Files.newOutputStream(output, StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
                target.write(encoded.toString().getBytes(StandardCharsets.UTF_8));
            }
        }
    }

    public static void main(String[] args) {
        int status = 1;
        try {
            run(args);
            status = 0;
        } catch (Throwable failure) {
            System.err.println("FAILURE: original region observation");
            failure.printStackTrace(System.err);
        }
        System.exit(status);
    }
}
