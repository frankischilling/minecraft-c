import java.lang.reflect.*;
import java.net.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

/* Reads unchanged original ge/nw CPU bodies. Never creates a Display, invokes
 * a GL operation, initializes input, or substitutes a source descendant. */
public final class GeNwCpuReference {
    private static Object field(Class<?> type, Object object, String name) throws Exception {
        Field f = type.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(object);
    }
    private static Object call(Class<?> type, Object object, String name, Class<?>[] signature, Object... args) throws Exception {
        Method method = type.getDeclaredMethod(name, signature);
        method.setAccessible(true);
        try { return method.invoke(object, args); }
        catch (InvocationTargetException e) { throw (Exception)e.getCause(); }
    }
    private static String cursor(Buffer b) {
        return b.position() + "," + b.limit() + "," + b.capacity() + "," + b.isDirect();
    }
    private static String hex(int value) { return String.format(Locale.ROOT, "%08x", value); }
    private static String nwState(Class<?> type, Object object) throws Exception {
        StringBuilder s = new StringBuilder();
        for (String name : new String[]{"h", "o", "p", "r", "k", "l", "m", "n", "q", "w", "x", "z", "A", "B"})
            s.append(name).append('=').append(field(type, object, name)).append(';');
        s.append("d=").append(cursor((ByteBuffer)field(type, object, "d"))).append(';');
        s.append("e=").append(cursor((IntBuffer)field(type, object, "e"))).append(';');
        s.append("f=").append(cursor((FloatBuffer)field(type, object, "f"))).append(';');
        int[] data = (int[])field(type, object, "g");
        s.append("gLength=").append(data.length).append(";g=");
        int used = Math.min(Math.max((Integer)field(type, object, "o"), 0), data.length);
        for (int i = 0; i < used; ++i) {
            if (i != 0) s.append(',');
            s.append(hex(data[i]));
        }
        return s.toString();
    }
    private static void ge(Class<?> ge, String method, int n, String name) throws Exception {
        try {
            Buffer b = (Buffer)call(ge, null, method, new Class<?>[]{int.class}, n);
            String order = b instanceof ByteBuffer ? ((ByteBuffer)b).order().toString()
                : b instanceof IntBuffer ? ((IntBuffer)b).order().toString() : ((FloatBuffer)b).order().toString();
            System.out.println(name + "|OK|" + cursor(b) + "," + order);
        } catch (IllegalArgumentException e) {
            System.out.println(name + "|" + e.getClass().getName());
        }
    }
    public static void main(String[] args) throws Exception {
        URL[] urls = {Path.of(args[0]).toUri().toURL(), Path.of(args[1]).toUri().toURL()};
        try (URLClassLoader loader = new URLClassLoader(urls, ClassLoader.getPlatformClassLoader())) {
            Class<?> ge = Class.forName("ge", true, loader);
            Object listA = field(ge, null, "a"), listB = field(ge, null, "b");
            System.out.println("geLists|" + (listA != listB) + "|" + ((List<?>)listA).size() + "," + ((List<?>)listB).size());
            ge(ge, "c", 0, "geC0"); ge(ge, "c", 17, "geC17"); ge(ge, "c", -1, "geCNegative");
            ge(ge, "d", 0, "geD0"); ge(ge, "d", 1024, "geD1024");
            ge(ge, "d", 1073741824, "geDWrappedZero"); ge(ge, "d", -1, "geDNegative");
            ge(ge, "e", 3, "geE3"); ge(ge, "e", -1, "geENegative");
            ByteBuffer shared = (ByteBuffer)call(ge, null, "c", new Class<?>[]{int.class}, 32);
            IntBuffer ints = shared.asIntBuffer(); FloatBuffer floats = shared.asFloatBuffer();
            ints.put(0, 0x3f800001); floats.put(1, -2.25f);
            System.out.println("views|" + hex(Float.floatToRawIntBits(floats.get(0))) + "," + hex(ints.get(1))
                + "|" + cursor(shared) + "|" + cursor(ints) + "|" + cursor(floats));
            Class<?> nw = Class.forName("nw", true, loader);
            Object object = field(nw, null, "a");
            System.out.println("nwInitial|b=" + field(nw, null, "b") + ";c=" + field(nw, null, "c") + ";" + nwState(nw, object));
            call(nw, object, "b", new Class<?>[]{});
            System.out.println("nwBegin|" + nwState(nw, object));
            try { call(nw, object, "b", new Class<?>[]{}); throw new AssertionError("begin twice succeeded"); }
            catch (IllegalStateException e) { System.out.println("nwBeginAgain|" + e.getClass().getName() + "|" + e.getMessage() + "|" + nwState(nw, object)); }
            call(nw, object, "a", new Class<?>[]{int.class,int.class,int.class,int.class}, -1, 300, 17, 0);
            double[][] vertices = {{0, 7.99f, 0, 0, 7.99f / 128f},
                {7.99f, 7.99f, 0, 7.99f / 128f, 7.99f / 128f},
                {7.99f, 0, 0, 7.99f / 128f, 0}, {0, 0, 0, 0, 0}};
            for (int i = 0; i < vertices.length; ++i) {
                double[] v = vertices[i];
                call(nw, object, "a", new Class<?>[]{double.class,double.class,double.class,double.class,double.class}, v[0],v[1],v[2],v[3],v[4]);
                System.out.println("nwVertex" + i + "|" + nwState(nw, object));
            }
            /* Private actual reset body is an isolated CPU reference control;
             * it does not acknowledge nw.draw or any unissued GL operation. */
            call(nw, object, "d", new Class<?>[]{});
            System.out.println("nwResetBody|" + nwState(nw, object));
            System.out.println("geListsAfterCpu|" + (listA == field(ge,null,"a")) + "," + (listB == field(ge,null,"b"))
                + "|" + ((List<?>)listA).size() + "," + ((List<?>)listB).size());
        }
    }
}
