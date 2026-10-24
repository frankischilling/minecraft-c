import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.IdentityHashMap;

/** Observes untouched original numeric Stack construction and cloning only. */
public final class StackValuesObservation {
    /* Inputs, not expected field values. Column order: id, count, damage, animation input. */
    private static final int[][] INPUTS = {
        {0, 0, 0, 0},
        {1, 1, 0, 7},
        {-1, -1, -1, -7},
        {Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE, Integer.MIN_VALUE},
        {Integer.MIN_VALUE, Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE},
        {32000, 0, -1, 1},
        {-2147483647, 2, 65535, 42},
        {2147483646, -64, -65536, -42},
        {123456789, 64, 32768, 11},
        {-987654321, 65, -32769, -11},
        {256, Integer.MAX_VALUE, Integer.MIN_VALUE, 19},
        {-42, Integer.MIN_VALUE, Integer.MAX_VALUE, -19}
    };

    private final StringBuilder output = new StringBuilder();
    private final IdentityHashMap<Object, Integer> identities = new IdentityHashMap<>();
    private int rows;

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private void row(Object... values) {
        for (int index = 0; index < values.length; ++index) {
            if (index != 0) output.append('\t');
            output.append(values[index]);
        }
        output.append('\n');
        ++rows;
        require(output.length() <= 16384, "bounded ASCII observation");
    }

    private String identity(Object value) {
        require(value != null, "nonnull observed Stack");
        Integer id = identities.get(value);
        if (id == null) {
            require(identities.size() < 24, "bounded retained identities");
            id = identities.size() + 1;
            identities.put(value, id); // Keep every observed owner live through END.
        }
        return "R:" + id;
    }

    private static String digest(Path path) throws Exception {
        return HexFormat.of().formatHex(
                MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path)));
    }

    private static Field field(Class<?> owner, String name, int modifiers) throws Exception {
        Field result = owner.getDeclaredField(name);
        require(result.getDeclaringClass() == owner && result.getType() == int.class
                && result.getModifiers() == modifiers, "exact original int field " + name);
        result.setAccessible(true);
        return result;
    }

    private void fields(String kind, int index, Object object, Field[] fields) throws Exception {
        row(kind, index, identity(object), fields[0].getInt(object), fields[1].getInt(object),
                fields[2].getInt(object), fields[3].getInt(object));
    }

    private void observe(String side, Path jar, String expected) throws Exception {
        URL url = jar.toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url},
                ClassLoader.getPlatformClassLoader())) {
            String name = side.equals("client") ? "iz" : "fy";
            Class<?> stack = Class.forName(name, false, loader);
            require(stack.getClassLoader() == loader
                    && stack.getProtectionDomain().getCodeSource() != null
                    && stack.getProtectionDomain().getCodeSource().getLocation().equals(url)
                    && stack.getName().equals(name)
                    && stack.getSuperclass() == Object.class
                    && stack.getInterfaces().length == 0
                    && stack.getModifiers() == (Modifier.PUBLIC | Modifier.FINAL),
                    "exact supplied-JAR Stack class");
            require(stack.getDeclaredFields().length == 4, "exact four Stack fields");
            Field[] fields = {field(stack, "a", Modifier.PUBLIC),
                    field(stack, "b", Modifier.PUBLIC), field(stack, "c", Modifier.PUBLIC),
                    field(stack, "d", Modifier.PRIVATE)};
            Constructor<?> numeric = stack.getDeclaredConstructor(int.class, int.class, int.class);
            require(numeric.getDeclaringClass() == stack && numeric.getModifiers() == Modifier.PUBLIC,
                    "original (III)V constructor");
            numeric.setAccessible(true);
            Method copy = stack.getDeclaredMethod(side.equals("client") ? "k" : "j");
            require(copy.getDeclaringClass() == stack && copy.getReturnType() == stack
                    && copy.getParameterCount() == 0 && copy.getModifiers() == Modifier.PUBLIC,
                    "original no-argument Stack clone descriptor");
            copy.setAccessible(true);
            row("STACK_VALUES_V1", side, name);
            for (int index = 0; index < INPUTS.length; ++index) {
                int[] input = INPUTS[index];
                row("INPUT", index, input[0], input[1], input[2], input[3]);
                Object source = numeric.newInstance(input[0], input[1], input[2]);
                require(source.getClass() == stack && !identities.containsKey(source),
                        "fresh actual source identity");
                fields("CONSTRUCTED", index, source, fields);
                row("ANIMATION_INPUT", index, identity(source), input[3]);
                fields[1].setInt(source, input[3]); // Explicit input on a constructed original owner.
                row("CLONE_INPUT", index, identity(source));
                Object clone = copy.invoke(source);
                require(clone != source && clone != null && clone.getClass() == stack
                        && !identities.containsKey(clone), "distinct actual clone identity");
                fields("SOURCE", index, source, fields);
                fields("COPY", index, clone, fields);
            }
            require(identities.size() == 24, "actual retained source/copy owner count");
            require(digest(jar).equals(expected), "original JAR unchanged after observation");
            row("END", INPUTS.length, identities.size(), rows);
        }
        for (int index = 0; index < output.length(); ++index) {
            require(output.charAt(index) <= 0x7f, "ASCII-only protocol");
        }
        byte[] bytes = output.toString().getBytes(StandardCharsets.US_ASCII);
        System.out.write(bytes);
        System.out.flush();
        require(!System.out.checkError(), "observation stdout write");
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: StackValuesObservation client|server jar");
        }
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        require(Files.size(jar) == (client ? 1465375L : 503100L), "pinned original JAR size");
        require(digest(jar).equals(expected), "pinned original JAR SHA-256 before classes");
        new StackValuesObservation().observe(args[0], jar, expected);
    }
}
