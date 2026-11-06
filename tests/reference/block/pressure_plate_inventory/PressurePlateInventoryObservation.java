import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.StringJoiner;

/** Scalar observations of registered, ordinarily constructed original Blocks. */
public final class PressurePlateInventoryObservation {
    private static final String JAR =
            "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    private static final int[] INPUTS = {Integer.MIN_VALUE, -1, 0, 1, 15, Integer.MAX_VALUE};

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static Map<String, Object> map(Object... pairs) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) out.put((String) pairs[i], pairs[i + 1]);
        return out;
    }

    private static String json(Object value) {
        if (value instanceof String) {
            String s = (String) value;
            return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        }
        if (value instanceof Number || value instanceof Boolean) return value.toString();
        if (value instanceof Map) {
            StringJoiner out = new StringJoiner(",", "{", "}");
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet())
                out.add(json(entry.getKey()) + ":" + json(entry.getValue()));
            return out.toString();
        }
        if (value instanceof Iterable) {
            StringJoiner out = new StringJoiner(",", "[", "]");
            for (Object entry : (Iterable<?>) value) out.add(json(entry));
            return out.toString();
        }
        throw new IllegalArgumentException("unsupported observation value");
    }

    private static List<String> bounds(Class<?> block, Object receiver) throws Exception {
        List<String> out = new ArrayList<>();
        for (String name : List.of("bs", "bt", "bu", "bv", "bw", "bx"))
            out.add(String.format(Locale.ROOT, "%016x",
                    Double.doubleToRawLongBits(block.getField(name).getDouble(receiver))));
        return out;
    }

    private static void origin(Class<?> cls, URLClassLoader loader, URL jar,
                               Map<String, Object> hashes) throws Exception {
        if (cls.getClassLoader() != loader ||
                !cls.getProtectionDomain().getCodeSource().getLocation().equals(jar))
            throw new IllegalStateException("original class loader/origin");
        try (var input = cls.getResourceAsStream("/" + cls.getName() + ".class")) {
            if (input == null) throw new IllegalStateException("original class bytes");
            hashes.put(cls.getName(), hash(input.readAllBytes()));
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("original client JAR required");
        Path jarPath = Path.of(args[0]).toRealPath();
        if (!hash(Files.readAllBytes(jarPath)).equals(JAR))
            throw new IllegalStateException("untouched original JAR");
        URL jar = jarPath.toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar},
                ClassLoader.getPlatformClassLoader())) {
            // Ordinary UU initialization constructs and publishes actual aL/aN.
            Class<?> block = Class.forName("uu", true, loader);
            Class<?> plate = Class.forName("bv", false, loader);
            Map<String, Object> hashes = new LinkedHashMap<>();
            origin(block, loader, jar, hashes);
            origin(plate, loader, jar, hashes);
            Map<String, Object> owners = new LinkedHashMap<>();
            Method color = plate.getMethod("b", int.class);
            Method type = plate.getMethod("b");
            Method shape = plate.getMethod("g");
            Method texture = plate.getMethod("a", int.class, int.class);
            Method side = plate.getMethod("a", int.class);
            Method setBounds = plate.getMethod("a", float.class, float.class, float.class,
                    float.class, float.class, float.class);
            // Resolve the setter owner only; never invoke it directly.
            for (Object[] entry : new Object[][] {{"color", color}, {"render_type", type},
                    {"shape", shape}, {"texture_metadata", texture}, {"texture_side", side},
                    {"bounds", setBounds}})
                owners.put((String) entry[0], ((Method) entry[1]).getDeclaringClass().getName());
            for (String name : List.of("aL", "aN")) {
                Object receiver = block.getField(name).get(null);
                int id = block.getField("bn").getInt(receiver);
                Object[] registered = (Object[]) block.getField("m").get(null);
                if (receiver.getClass() != plate || registered[id] != receiver)
                    throw new IllegalStateException("actual registered pressure plate");
                List<String> constructor = bounds(block, receiver);
                List<Object> colors = new ArrayList<>();
                List<Object> textures = new ArrayList<>();
                for (int metadata : INPUTS) {
                    colors.add(map("metadata", metadata, "color", color.invoke(receiver, metadata)));
                    for (int face : INPUTS)
                        textures.add(map("side", face, "metadata", metadata,
                                "value", texture.invoke(receiver, face, metadata)));
                }
                List<String> afterScalars = bounds(block, receiver);
                shape.invoke(receiver);
                List<String> first = bounds(block, receiver);
                shape.invoke(receiver);
                List<String> repeated = bounds(block, receiver);
                System.out.println("PRESSURE_PLATE_INVENTORY " + json(map("field", name,
                        "id", id, "runtime_class", receiver.getClass().getName(),
                        "superclass", plate.getSuperclass().getName(), "owners", owners,
                        "class_sha256", hashes, "registered_identity", true,
                        "render_type", type.invoke(receiver), "texture_field",
                        block.getField("bm").getInt(receiver), "constructor_bounds", constructor,
                        "after_scalar_bounds", afterScalars, "color_samples", colors,
                        "texture_samples", textures, "inventory_bounds", first,
                        "repeated_inventory_bounds", repeated)));
            }
        }
    }
}
