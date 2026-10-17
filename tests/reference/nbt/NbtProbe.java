import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/** Runs the supplied original classes without linking either side into this source. */
public final class NbtProbe {
    private final ClassLoader loader;
    private final Class<?> base;
    private final String compoundUtility;
    private final String packetClass;
    private final Method read;
    private final Method write;
    private final Method factory;
    private final Method type;
    private final Method name;
    private final Method setName;

    private NbtProbe(ClassLoader loader, boolean client) throws Exception {
        this.loader = loader;
        base = Class.forName(client ? "ij" : "fo", true, loader);
        compoundUtility = client ? "as" : "ag";
        packetClass = client ? "ki" : "gt";
        read = method(base, "b", DataInput.class);
        write = method(base, "a", base, DataOutput.class);
        factory = method(base, "a", byte.class);
        type = method(base, "a");
        name = method(base, "b");
        setName = method(base, "a", String.class);
    }

    private static Method method(Class<?> owner, String name, Class<?>... arguments)
            throws Exception {
        Method result = owner.getDeclaredMethod(name, arguments);
        result.setAccessible(true);
        return result;
    }

    private static Object invoke(Method method, Object owner, Object... arguments)
            throws Exception {
        try {
            return method.invoke(owner, arguments);
        } catch (InvocationTargetException error) {
            Throwable cause = error.getCause();
            if (cause instanceof Error) {
                throw (Error) cause;
            }
            if (cause instanceof Exception) {
                throw (Exception) cause;
            }
            throw error;
        }
    }

    private static byte[] bytes(String hex) {
        if ((hex.length() & 1) != 0) {
            throw new IllegalArgumentException("odd byte hex length");
        }
        byte[] result = new byte[hex.length() / 2];
        for (int i = 0; i < result.length; i++) {
            result[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
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

    private static String units(String hex) {
        if ((hex.length() & 3) != 0) {
            throw new IllegalArgumentException("invalid UTF-16 hex length");
        }
        char[] result = new char[hex.length() / 4];
        for (int i = 0; i < result.length; i++) {
            result[i] = (char) Integer.parseInt(hex.substring(i * 4, i * 4 + 4), 16);
        }
        return new String(result);
    }

    private static String unitHex(String value) {
        char[] alphabet = "0123456789abcdef".toCharArray();
        char[] result = new char[value.length() * 4];
        for (int i = 0; i < value.length(); i++) {
            int unit = value.charAt(i);
            for (int digit = 0; digit < 4; digit++) {
                result[i * 4 + digit] = alphabet[(unit >>> (12 - digit * 4)) & 15];
            }
        }
        return new String(result);
    }

    private int tagType(Object tag) throws Exception {
        return ((Byte) invoke(type, tag)).intValue() & 255;
    }

    private String tagName(Object tag) throws Exception {
        return (String) invoke(name, tag);
    }

    private static Object payload(Object tag) throws Exception {
        Field field = tag.getClass().getDeclaredField("a");
        field.setAccessible(true);
        return field.get(tag);
    }

    private String tree(Object tag) throws Exception {
        int id = tagType(tag);
        String prefix = "[" + id + ",\"" + (id == 0 ? "" : unitHex(tagName(tag))) + "\",";
        if (id == 0) {
            return prefix + "null]";
        }
        Object value = payload(tag);
        switch (id) {
            case 1: case 2: case 3: case 4:
                return prefix + value + "]";
            case 5:
                return prefix + "\"" + String.format(Locale.ROOT, "%08x",
                        Float.floatToRawIntBits((Float) value)) + "\"]";
            case 6:
                return prefix + "\"" + String.format(Locale.ROOT, "%016x",
                        Double.doubleToRawLongBits((Double) value)) + "\"]";
            case 7:
                return prefix + "\"" + hex((byte[]) value) + "\"]";
            case 8:
                return prefix + "\"" + unitHex((String) value) + "\"]";
            case 9: {
                List<?> children = (List<?>) value;
                int elementType = children.isEmpty() ? 1 : tagType(children.get(0));
                StringBuilder result = new StringBuilder(prefix).append('[')
                        .append(elementType).append(",[");
                for (int i = 0; i < children.size(); i++) {
                    if (i != 0) result.append(',');
                    result.append(tree(children.get(i)));
                }
                return result.append("]]]").toString();
            }
            case 10: {
                Collection<?> values = (Collection<?>) invoke(method(tag.getClass(), "c"), tag);
                List<Object> children = new ArrayList<>(values);
                // String.compareTo compares unsigned UTF-16 units, including lone surrogates.
                children.sort(Comparator.comparing(child -> {
                    try { return tagName(child); }
                    catch (Exception error) { throw new IllegalStateException(error); }
                }));
                StringBuilder result = new StringBuilder(prefix).append('[');
                for (int i = 0; i < children.size(); i++) {
                    if (i != 0) result.append(',');
                    result.append(tree(children.get(i)));
                }
                return result.append("]]").toString();
            }
            default:
                throw new IllegalArgumentException("unsupported reflected tag " + id);
        }
    }

    private Object construct(int id, String tagName, String text) throws Exception {
        Object tag = invoke(factory, null, (byte) id);
        invoke(setName, tag, tagName);
        if (id == 0) return tag;
        if (id == 9) {
            Method append = method(tag.getClass(), "a", base);
            if (!text.isEmpty()) {
                for (String child : text.split(",", -1)) {
                    String[] fields = child.split(":", 2);
                    invoke(append, tag, construct(Integer.parseInt(fields[0]), "",
                            fields.length == 1 ? "" : fields[1]));
                }
            }
            return tag;
        }
        if (id == 10) {
            Method put = method(tag.getClass(), "a", String.class, base);
            if (text.equals("duplicate")) {
                invoke(put, tag, "same", construct(1, "", "1"));
                invoke(put, tag, "same", construct(3, "", "2"));
            } else if (text.equals("multikey")) {
                for (String key : new String[] {"z", "Aa", "BB", "a", "\u0000", "\ud800"}) {
                    invoke(put, tag, key, construct(8, "", unitHex(key)));
                }
            } else if (!text.isEmpty()) {
                throw new IllegalArgumentException("unknown compound construction");
            }
            return tag;
        }
        Field field = tag.getClass().getDeclaredField("a");
        field.setAccessible(true);
        switch (id) {
            case 1: field.setByte(tag, Byte.parseByte(text)); break;
            case 2: field.setShort(tag, Short.parseShort(text)); break;
            case 3: field.setInt(tag, Integer.parseInt(text)); break;
            case 4: field.setLong(tag, Long.parseLong(text)); break;
            case 5: field.setFloat(tag, Float.intBitsToFloat(Integer.parseUnsignedInt(text, 16))); break;
            case 6: field.setDouble(tag, Double.longBitsToDouble(Long.parseUnsignedLong(text, 16))); break;
            case 7: field.set(tag, bytes(text)); break;
            case 8: field.set(tag, units(text)); break;
            default: throw new IllegalArgumentException("unknown construction type");
        }
        return tag;
    }

    private String packet(String value) throws Exception {
        Class<?> packet = Class.forName(packetClass, true, loader);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        invoke(method(packet, "a", String.class, DataOutputStream.class), null,
                value, new DataOutputStream(output));
        byte[] encoded = output.toByteArray();
        DataInputStream input = new DataInputStream(new ByteArrayInputStream(encoded));
        String decoded = (String) invoke(method(packet, "a", DataInputStream.class, int.class),
                null, input, 32767);
        return "OK\t" + (encoded.length - input.available()) + "\t" + hex(encoded)
                + "\t\"" + unitHex(decoded) + "\"";
    }

    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1);
        if (fields[0].equals("packet") && fields.length == 2) {
            return packet(units(fields[1]));
        }
        Object tag;
        int consumed;
        if (fields[0].equals("construct") && fields.length == 4) {
            tag = construct(Integer.parseInt(fields[1]), units(fields[2]), fields[3]);
            consumed = 0;
        } else if ((fields[0].equals("generic") || fields[0].equals("compound"))
                && fields.length == 2) {
            byte[] wire = bytes(fields[1]);
            DataInputStream input = new DataInputStream(new ByteArrayInputStream(wire));
            Method parser = fields[0].equals("generic") ? read
                    : method(Class.forName(compoundUtility, true, loader), "a", DataInput.class);
            tag = invoke(parser, null, input);
            consumed = wire.length - input.available();
        } else {
            throw new IllegalArgumentException("invalid request");
        }
        String logical = tree(tag);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        invoke(write, null, tag, new DataOutputStream(output));
        return "OK\t" + consumed + "\t" + hex(output.toByteArray()) + "\t" + logical;
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 || !(arguments[0].equals("client") || arguments[0].equals("server"))) {
            System.err.println("Usage: NbtProbe client|server path-to-original.jar");
            System.exit(1);
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
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            NbtProbe probe = new NbtProbe(loader, client);
            BufferedReader input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String line;
            while ((line = input.readLine()) != null) {
                try {
                    System.out.println(probe.request(line));
                } catch (Exception error) {
                    System.out.println("ERR\t" + error.getClass().getName());
                }
            }
        }
    }
}
