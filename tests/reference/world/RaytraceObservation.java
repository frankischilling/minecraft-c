import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Reflection glue only. Uses the unchanged genuine World fixture constructor
 * from MutationObservation and the untouched JAR's real cached normal Chunk,
 * World getters, Block virtuals, Vec3 pools and HitResult constructors.
 * Authored cell bytes/cache membership and restored bounds are prepared inputs. */
public final class RaytraceObservation {
    private static Field field(Class<?> cl, String name) throws Exception {
        Field f = cl.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
    private static Method method(Class<?> cl, String name, Class<?>... args) throws Exception {
        Method m = cl.getDeclaredMethod(name, args);
        m.setAccessible(true);
        return m;
    }
    private static String vector(Object value, Field[] axes) throws Exception {
        if (value == null) return "-";
        StringBuilder text = new StringBuilder();
        for (Field axis : axes) {
            if (text.length() != 0) text.append(',');
            text.append(String.format(Locale.ROOT, "%016x",
                    Double.doubleToRawLongBits(axis.getDouble(value))));
        }
        return text.toString();
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        if (args.length != 2) throw new IllegalArgumentException("JAR new-owned-World-root");
        try (URLClassLoader original = new URLClassLoader(
                new java.net.URL[]{Path.of(args[0]).toRealPath().toUri().toURL()},
                ClassLoader.getPlatformClassLoader());
             BufferedReader input = new BufferedReader(new InputStreamReader(
                     System.in, StandardCharsets.US_ASCII))) {
            Class<?> modelType = Class.forName("MutationObservation$Model");
            Constructor<?> modelConstructor = modelType.getDeclaredConstructor(ClassLoader.class,
                    boolean.class);
            modelConstructor.setAccessible(true);
            Object model = modelConstructor.newInstance(original, true);
            method(modelType, "setup", Path.class).invoke(model, Path.of(args[1]).toAbsolutePath());
            Object world = field(modelType, "world").get(model);
            Object cache = field(modelType, "cache").get(model);
            Class<?> worldType = (Class<?>)field(modelType, "worldType").get(model);
            Class<?> chunkType = (Class<?>)field(modelType, "chunkType").get(model);
            Class<?> blockType = Class.forName("uu", true, original);
            Class<?> vecType = Class.forName("bt", true, original);
            Class<?> hitType = Class.forName("vf", false, original);
            Class<?> boxType = Class.forName("eq", true, original);
            // Resolve Nibble from its actual Chunk.e type, not a guessed symbol.
            Field metadataField = field(chunkType, "e");
            Class<?> nibbleType = metadataField.getType();
            byte[] cells = new byte[32768];
            Object chunk = chunkType.getConstructor(worldType, byte[].class, int.class, int.class)
                    .newInstance(world, cells, 0, 0);
            byte[] metadataBytes = (byte[])field(nibbleType, "a").get(metadataField.get(chunk));
            Map<Object, Object> map = (Map<Object, Object>)field(cache.getClass(), "e").get(cache);
            Method key = (Method)field(modelType, "keyMethod").get(model);
            map.put(key.invoke(null, 0, 0), chunk);
            List<Object> insertions = (List<Object>)field(cache.getClass(), "f").get(cache);
            insertions.add(chunk);
            if (world.getClass() != worldType || chunk.getClass() != chunkType)
                throw new IllegalStateException("genuine original concrete owners");
            Object[] blocks = (Object[])field(blockType, "m").get(null);
            Field[] bounds = new Field[6], axes = new Field[3];
            String[] names = {"bs", "bt", "bu", "bv", "bw", "bx"};
            for (int i = 0; i < 6; ++i) bounds[i] = field(blockType, names[i]);
            for (int i = 0; i < 3; ++i) axes[i] = field(vecType, String.valueOf((char)('a' + i)));
            double[][] initial = new double[97][6];
            for (int id = 1; id <= 96; ++id)
                for (int i = 0; i < 6; ++i) initial[id][i] = bounds[i].getDouble(blocks[id]);
            Method setBounds = method(blockType, "a", float.class, float.class, float.class,
                    float.class, float.class, float.class);
            Method direct = method(vecType, "a", double.class, double.class, double.class);
            Method pooled = method(vecType, "b", double.class, double.class, double.class);
            Method reset = method(vecType, "b");
            Field cursor = field(vecType, "e");
            Method boxReset = method(boxType, "b");
            Field boxCursor = field(boxType, "h");
            Method ray = method(worldType, "a", vecType, vecType, boolean.class, boolean.class);
            Field hitPoint = field(hitType, "f");
            int rows = 0;
            String line;
            while ((line = input.readLine()) != null) {
                if (++rows > 512) throw new IllegalArgumentException("row bound");
                String[] t = line.trim().split("\\s+");
                if (t.length != 12) throw new IllegalArgumentException("twelve columns");
                int id = Integer.parseInt(t[1]), md = Integer.parseInt(t[2]);
                boolean liquid = Integer.parseInt(t[3]) != 0, ignore = Integer.parseInt(t[4]) != 0;
                int mode = Integer.parseInt(t[5]);
                if (id < 0 || id > 255 || md < 0 || md > 15)
                    throw new IllegalArgumentException("cell domain");
                for (int n = 1; n <= 96; ++n) {
                    Object[] arguments = new Object[6];
                    for (int i = 0; i < 6; ++i) arguments[i] = (float)initial[n][i];
                    setBounds.invoke(blocks[n], arguments);
                }
                Arrays.fill(cells, (byte)0);
                Arrays.fill(metadataBytes, (byte)0);
                int index = (4 << 11) | (4 << 7) | 64;
                cells[index] = (byte)id;
                metadataBytes[index >> 1] = (byte)md;
                reset.invoke(null);
                boxReset.invoke(null);
                Method factory = mode == 1 ? pooled : direct;
                Object start = mode == 2 ? null : factory.invoke(null, Double.valueOf(t[6]),
                        Double.valueOf(t[7]), Double.valueOf(t[8]));
                Object end = mode == 3 ? null : factory.invoke(null, Double.valueOf(t[9]),
                        Double.valueOf(t[10]), Double.valueOf(t[11]));
                if (mode == 1) reset.invoke(null);
                Object hit = null;
                String outcome = "OK";
                try { hit = ray.invoke(world, start, end, liquid, ignore); }
                catch (InvocationTargetException failure) {
                    outcome = failure.getCause().getClass().getName();
                }
                String result = "-";
                if (hit != null) {
                    result = field(hitType, "b").getInt(hit) + "," + field(hitType, "c").getInt(hit)
                            + "," + field(hitType, "d").getInt(hit) + ","
                            + field(hitType, "e").getInt(hit) + ":"
                            + vector(hitPoint.get(hit), axes);
                }
                String shape = "-";
                if (id > 0 && id <= 96) shape = vector(blocks[id], bounds);
                System.out.println(String.join("\t", "RAY", t[0], outcome, result,
                        vector(start, axes), String.valueOf(cursor.getInt(null)),
                        String.valueOf(boxCursor.getInt(null)), shape));
            }
            if (rows == 0) throw new IllegalArgumentException("empty observations");
        }
    }
}
