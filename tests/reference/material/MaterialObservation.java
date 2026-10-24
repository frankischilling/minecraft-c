import java.io.PrintWriter;
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

/** Observes only the original material and palette constructors and receiver methods. */
public final class MaterialObservation {
    private static Object field(Class<?> type, String name, Class<?> expected, Object receiver)
            throws Exception {
        Field field = type.getDeclaredField(name);
        if (field.getType() != expected
                || Modifier.isStatic(field.getModifiers()) != (receiver == null))
            throw new AssertionError("field descriptor: " + name);
        field.setAccessible(true);
        return field.get(receiver);
    }

    private static void observe(PrintWriter output, IdentityHashMap<Object, Integer> palettes,
            Class<?> materialClass, Class<?> paletteClass, Object material, char name,
            String phase, Method[] methods, Method integerMethod) throws Exception {
        Object palette = field(materialClass, "C", paletteClass, material);
        Integer paletteId = palettes.get(palette);
        if (paletteId == null) throw new AssertionError("unregistered palette");
        output.print("M\t" + phase + "\t" + name + "\t" + paletteId + "\t");
        for (Method method : methods) {
            Object value = method.invoke(material);
            if (!(value instanceof Boolean)) throw new AssertionError("predicate descriptor");
            output.print((Boolean)value ? '1' : '0');
        }
        output.print("\t" + integerMethod.invoke(material) + "\n");
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server")))
            throw new IllegalArgumentException("Usage: MaterialObservation client|server jar");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        String actual = HexFormat.of().formatHex(
                MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)));
        if (!actual.equals(expected)) throw new IllegalArgumentException("original JAR hash");
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            Class<?> paletteClass = Class.forName(client ? "dx" : "cm", true, loader);
            Class<?> materialClass = Class.forName(client ? "ln" : "hj", true, loader);
            IdentityHashMap<Object, Integer> palettes = new IdentityHashMap<>();
            IdentityHashMap<Object, Boolean> materials = new IdentityHashMap<>();
            Method[] methods = new Method[8];
            String[] names = {"a", "b", "c", "d", "e", "g", "h", "i"};
            for (int index = 0; index < methods.length; ++index) {
                methods[index] = materialClass.getDeclaredMethod(names[index]);
                if (methods[index].getReturnType() != boolean.class
                        || Modifier.isStatic(methods[index].getModifiers()))
                    throw new AssertionError("predicate descriptor");
                methods[index].setAccessible(true);
            }
            Method integerMethod = materialClass.getDeclaredMethod("j");
            Method mutator = materialClass.getDeclaredMethod("f");
            if (integerMethod.getReturnType() != int.class || mutator.getReturnType() != materialClass
                    || Modifier.isStatic(integerMethod.getModifiers())
                    || Modifier.isStatic(mutator.getModifiers()))
                throw new AssertionError("receiver descriptor");
            integerMethod.setAccessible(true);
            mutator.setAccessible(true);
            PrintWriter output = new PrintWriter(System.out, false, StandardCharsets.US_ASCII);
            output.print("MATERIAL_V1\n");
            for (int id = 0; id < 14; ++id) {
                char name = (char)('b' + id);
                Object palette = field(paletteClass, String.valueOf(name), paletteClass, null);
                if (palette == null || palettes.put(palette, id) != null)
                    throw new AssertionError("distinct palette identity");
                output.print("P\t" + name + "\t" + field(paletteClass, "q", int.class, palette)
                        + "\t" + field(paletteClass, "p", int.class, palette) + "\n");
            }
            Class<?> arrayClass = java.lang.reflect.Array.newInstance(paletteClass, 0).getClass();
            Object[] table = (Object[])field(paletteClass, "a", arrayClass, null);
            if (table.length != 16) throw new AssertionError("palette table length");
            for (int slot = 0; slot < table.length; ++slot) {
                Integer id = table[slot] == null ? Integer.valueOf(-1) : palettes.get(table[slot]);
                if (id == null) throw new AssertionError("palette table identity");
                output.print("T\t" + slot + "\t" + id + "\n");
            }
            for (int id = 0; id < 28; ++id) {
                char name = (char)(id < 26 ? 'a' + id : 'A' + id - 26);
                Object material = field(materialClass, String.valueOf(name), materialClass, null);
                if (material == null || materials.put(material, Boolean.TRUE) != null)
                    throw new AssertionError("distinct material identity");
                observe(output, palettes, materialClass, paletteClass, material, name,
                        "initial", methods, integerMethod);
                Object alias = mutator.invoke(material);
                output.print("F\t" + name + "\t" + (alias == material ? 1 : 0) + "\n");
                observe(output, palettes, materialClass, paletteClass, alias, name,
                        "after-f", methods, integerMethod);
            }
            output.print("END\t28\t14\t16\n");
            output.flush();
            if (output.checkError()) throw new IllegalStateException("output failure");
        }
    }
}
