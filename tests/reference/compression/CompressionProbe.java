import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.zip.DeflaterOutputStream;
import java.util.zip.GZIPInputStream;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

/** Executes the pinned original gzip wrappers and separate JDK container controls. */
public final class CompressionProbe {
    private static final int MAX_BYTES = 256 * 1024;
    private static final int MAX_REQUESTS = 1024;
    private final Method gzipRead;
    private final Method gzipWrite;
    private final Method rawRead;
    private final Method rawWrite;

    private CompressionProbe(ClassLoader loader, boolean client) throws Exception {
        Class<?> utility = Class.forName(client ? "as" : "ag", true, loader);
        Class<?> compound = Class.forName(client ? "nu" : "iq", true, loader);
        gzipRead = method(utility, InputStream.class);
        gzipWrite = method(utility, compound, OutputStream.class);
        rawRead = method(utility, DataInput.class);
        rawWrite = method(utility, compound, DataOutput.class);
    }

    private static Method method(Class<?> owner, Class<?>... arguments) throws Exception {
        Method result = owner.getDeclaredMethod("a", arguments);
        result.setAccessible(true);
        return result;
    }

    private static Object call(Method method, Object... arguments) throws Exception {
        try {
            return method.invoke(null, arguments);
        } catch (InvocationTargetException error) {
            Throwable cause = error.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            throw (Error) cause;
        }
    }

    private static byte[] bytes(String text) {
        if ((text.length() & 1) != 0 || text.length() > MAX_BYTES * 2) {
            throw new IllegalArgumentException("invalid or oversized input hex");
        }
        byte[] result = new byte[text.length() / 2];
        for (int i = 0; i < result.length; i++) {
            result[i] = (byte) Integer.parseInt(text.substring(i * 2, i * 2 + 2), 16);
        }
        return result;
    }

    private static String hex(byte[] value) {
        char[] alphabet = "0123456789abcdef".toCharArray();
        char[] result = new char[value.length * 2];
        for (int i = 0; i < value.length; i++) {
            int octet = value[i] & 255;
            result[i * 2] = alphabet[octet >>> 4];
            result[i * 2 + 1] = alphabet[octet & 15];
        }
        return new String(result);
    }

    private static final class InputSpy extends InputStream {
        final byte[] data;
        final int chunk;
        int position;
        int closes;
        InputSpy(byte[] data, int chunk) { this.data = data; this.chunk = chunk; }
        @Override public int read() { return position == data.length ? -1 : data[position++] & 255; }
        @Override public int read(byte[] output, int offset, int length) {
            if (length == 0) return 0;
            if (position == data.length) return -1;
            int count = Math.min(Math.min(length, chunk), data.length - position);
            System.arraycopy(data, position, output, offset, count);
            position += count;
            return count;
        }
        @Override public int available() { return data.length - position; }
        @Override public void close() { closes++; }
    }

    private static final class OutputSpy extends OutputStream {
        final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        int closes;
        int flushes;
        @Override public void write(int value) throws IOException {
            if (bytes.size() >= MAX_BYTES) throw new IOException("probe output limit");
            bytes.write(value);
        }
        @Override public void write(byte[] value, int offset, int length) throws IOException {
            if (length > MAX_BYTES - bytes.size()) throw new IOException("probe output limit");
            bytes.write(value, offset, length);
        }
        @Override public void close() { closes++; }
        @Override public void flush() { flushes++; }
    }

    private static byte[] drain(InputStream input) throws IOException {
        OutputSpy output = new OutputSpy();
        byte[] chunk = new byte[4096];
        int count;
        while ((count = input.read(chunk)) != -1) output.write(chunk, 0, count);
        return output.bytes.toByteArray();
    }

    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1);
        if (fields.length < 2 || fields.length > 3) throw new IllegalArgumentException("invalid request");
        int chunk = fields.length == 3 ? Integer.parseInt(fields[2]) : MAX_BYTES;
        if (chunk < 1 || chunk > MAX_BYTES) throw new IllegalArgumentException("invalid chunk size");
        byte[] data = bytes(fields[1]);
        InputSpy input = new InputSpy(data, chunk);
        OutputSpy output = new OutputSpy();
        int consumed = 0;
        String error = null;
        try {
            if (fields[0].equals("original-read")) {
                Object root = call(gzipRead, input);
                call(rawWrite, root, new DataOutputStream(output));
                consumed = input.position;
            } else if (fields[0].equals("raw-root")) {
                Object root = call(rawRead, new DataInputStream(input));
                consumed = input.position;
                call(rawWrite, root, new DataOutputStream(output));
            } else if (fields[0].equals("original-write")) {
                Object root = call(rawRead, new DataInputStream(input));
                consumed = input.position;
                call(gzipWrite, root, output);
            } else if (fields[0].equals("drain-gzip")) {
                try (GZIPInputStream gzip = new GZIPInputStream(input)) {
                    output.write(drain(gzip));
                }
                consumed = input.position;
            } else if (fields[0].equals("drain-zlib")) {
                Inflater inflater = new Inflater();
                try {
                    try (InflaterInputStream stream = new InflaterInputStream(input, inflater)) {
                        output.write(drain(stream));
                    }
                    consumed = Math.toIntExact(inflater.getBytesRead());
                } finally {
                    inflater.end();
                }
            } else if (fields[0].equals("write-zlib")) {
                try (DeflaterOutputStream stream = new DeflaterOutputStream(output)) {
                    stream.write(data);
                }
                consumed = data.length;
            } else {
                throw new IllegalArgumentException("unknown request mode");
            }
        } catch (Exception failure) {
            error = failure.getClass().getName();
        }
        String metadata = "{\"input_bytes_delivered\":" + input.position
                + ",\"input_close_calls\":" + input.closes
                + ",\"output_close_calls\":" + output.closes
                + ",\"output_flush_calls\":" + output.flushes
                + ",\"partial_output_hex\":\"" + hex(output.bytes.toByteArray()) + "\"}";
        if (error != null) return "ERR\t" + error + "\t" + metadata;
        return "OK\t" + consumed + "\t" + hex(output.bytes.toByteArray()) + "\t" + metadata;
    }

    private static String boundedLine(BufferedReader reader) throws IOException {
        StringBuilder line = new StringBuilder();
        int value;
        while ((value = reader.read()) != -1 && value != '\n') {
            if (line.length() >= MAX_BYTES * 2 + 128) throw new IOException("probe request limit");
            line.append((char) value);
        }
        if (value == -1 && line.length() == 0) return null;
        if (line.length() > 0 && line.charAt(line.length() - 1) == '\r') line.setLength(line.length() - 1);
        return line.toString();
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 || !(arguments[0].equals("client") || arguments[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: CompressionProbe client|server original.jar");
        }
        boolean client = arguments[0].equals("client");
        Path jar = Path.of(arguments[1]).toAbsolutePath().normalize();
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!Files.isRegularFile(jar)
                || !hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar))).equals(expected)) {
            System.err.println("UNAVAILABLE: original JAR missing or SHA-256 mismatch: " + jar);
            System.exit(77);
        }
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            CompressionProbe probe = new CompressionProbe(loader, client);
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String line;
            int requests = 0;
            while ((line = boundedLine(reader)) != null) {
                if (++requests > MAX_REQUESTS) throw new IOException("probe request count limit");
                System.out.println(probe.request(line));
            }
        }
    }
}
