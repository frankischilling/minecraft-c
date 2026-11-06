import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Private observations of genuine DN draws; no source-field writes or seed input. */
public final class EffectRenderObservation {
    static final int[] PARTIALS = {0x00000000, 0x80000000, 0x3e800000, 0x3f000000,
        0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc12345, 0x7f812345};
    static int network, natives, exceptions;
    static Object baselineArray, baselineRandom;
    static Object[] baselineLists;
    static em particle;
    static ju picked, target;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }
    static Object get(Object object, Class<?> type, String name) throws Exception {
        return field(type, name).get(object);
    }
    static String hex(long value) {
        String text = Long.toHexString(value);
        return "0".repeat(16 - text.length()) + text;
    }
    static String fhex(int value) {
        String text = Integer.toHexString(value);
        return "0".repeat(8 - text.length()) + text;
    }
    static String scalar(Object value) {
        if (value instanceof Double) return hex(Double.doubleToRawLongBits((Double)value));
        if (value instanceof Float) return fhex(Float.floatToRawIntBits((Float)value));
        if (value instanceof Boolean) return (Boolean)value ? "1" : "0";
        return value.toString();
    }
    static String random(Random value) throws Exception {
        return hex(((AtomicLong)get(value, Random.class, "seed")).get()) + "," +
            scalar(get(value, Random.class, "haveNextNextGaussian")) + "," +
            scalar(get(value, Random.class, "nextNextGaussian"));
    }
    static long uniquifier() throws Exception {
        return ((AtomicLong)get(null, Random.class, "seedUniquifier")).get();
    }
    static String manager(dn owner) throws Exception {
        List<?>[] lists = (List<?>[])get(owner, dn.class, "b");
        Random rng = (Random)get(owner, dn.class, "d");
        require(lists.length == 4 && rng != null, "actual DN constructor shape");
        if (baselineArray == null) {
            baselineArray = lists;
            baselineRandom = rng;
            baselineLists = lists.clone();
        }
        boolean distinct = true;
        for (int i = 0; i < lists.length; ++i)
            for (int j = 0; j < i; ++j) distinct &= lists[i] != lists[j];
        StringBuilder row = new StringBuilder();
        row.append(lists == baselineArray ? 1 : 0).append('\t')
            .append(rng == baselineRandom ? 1 : 0).append('\t').append(distinct ? 1 : 0)
            .append('\t').append(get(owner, dn.class, "a") == null ? 1 : 0)
            .append('\t').append(get(owner, dn.class, "c") == null ? 1 : 0)
            .append('\t').append(random(rng)).append('\t').append(hex(uniquifier()));
        for (int i = 0; i < lists.length; ++i) {
            require(lists[i] instanceof ArrayList<?>, "actual constructor ArrayList");
            Object[] cells = (Object[])get(lists[i], ArrayList.class, "elementData");
            require(cells.length <= 32, "bounded actual small-profile backing");
            row.append('\t').append(lists[i] == baselineLists[i] ? 1 : 0)
                .append(',').append(lists[i].size()).append(',').append(cells.length)
                .append(',').append(get(lists[i], java.util.AbstractList.class, "modCount"))
                .append(',');
            if (cells.length == 0) row.append('-');
            for (Object cell : cells) {
                require(cell == null || cell == particle, "actual source row provenance");
                row.append(cell == null ? 'n' : 'p');
            }
        }
        return row.toString();
    }
    static void managerRow(String label, dn owner) throws Exception {
        System.out.println("DN_STATE\t" + label + "\t" + manager(owner));
    }
    static String entity(sn value) throws Exception {
        StringBuilder row = new StringBuilder(value.getClass().getName());
        for (Class<?> type = value.getClass(); type != Object.class; type = type.getSuperclass()) {
            Field[] fields = type.getDeclaredFields();
            String[] names = new String[fields.length];
            for (int i = 0; i < fields.length; ++i) names[i] = fields[i].getName();
            Arrays.sort(names);
            for (String name : names) {
                Field item = field(type, name);
                if (!Modifier.isStatic(item.getModifiers()) && item.getType().isPrimitive())
                    row.append('|').append(type.getName()).append('.').append(name).append('=')
                        .append(scalar(item.get(value)));
            }
        }
        Object box = get(value, sn.class, "aW");
        require(box != null, "real Entity constructor bounds");
        for (String name : new String[]{"a", "b", "c", "d", "e", "f"})
            row.append("|EQ.").append(name).append('=').append(scalar(get(box, eq.class, name)));
        row.append("|Random=").append(random((Random)get(value, sn.class, "bs")))
            .append("|WorldNull=").append(get(value, sn.class, "aI") == null ? 1 : 0);
        if (value == particle) row.append("|picked=")
            .append(get(value, em.class, "a") == picked ? 1 : 0).append("|target=")
            .append(get(value, em.class, "o") == target ? 1 : 0);
        return row.toString();
    }
    static String particles() throws Exception {
        // XW's genuine constructor already invoked Math.random and in.a(D)F.
        Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false,
                                       EffectRenderObservation.class.getClassLoader());
        Random math = (Random)get(null, holder, "randomNumberGenerator");
        return entity(picked) + "\t" + (target == null ? "null" : entity(target)) + "\t" +
            entity(particle) + "\tMath=" + random(math) + "\tSN.counter=" + get(null, sn.class, "a");
    }
    static String nwState() throws Exception {
        nw singleton = nw.a; // Reached only after the actual DN.b getstatic17.
        require(singleton != null, "actual source NW singleton");
        int[] words = (int[])get(singleton, nw.class, "g");
        ByteBuffer bytes = (ByteBuffer)get(singleton, nw.class, "d");
        IntBuffer integers = (IntBuffer)get(singleton, nw.class, "e");
        Buffer floats = (Buffer)get(singleton, nw.class, "f");
        long hash = 0xcbf29ce484222325L;
        boolean zero = true;
        for (int word : words) { zero &= word == 0; hash = (hash ^ (word & 0xffffffffL)) * 0x100000001b3L; }
        require(zero && !(Boolean)get(singleton, nw.class, "x") &&
                !(Boolean)get(null, nw.class, "c"), "genuine default NW CPU closure");
        StringBuilder row = new StringBuilder();
        for (String name : new String[]{"h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "z", "A", "B"})
            row.append(name).append('=').append(scalar(get(singleton, nw.class, name))).append('|');
        row.append("words=").append(words.length).append(',').append(hex(hash));
        for (Buffer buffer : new Buffer[]{bytes, integers, floats})
            row.append('|').append(buffer.position()).append(',').append(buffer.limit())
                .append(',').append(buffer.capacity());
        return row.toString();
    }
    static void exception(String label, NullPointerException actual, String owner, String method) {
        StackTraceElement top = actual.getStackTrace()[0];
        require(top.getClassName().equals(owner) && top.getMethodName().equals(method),
                "actual reached original failure " + label);
        ++exceptions;
        System.out.println("DN_EXCEPTION_BEGIN\t" + label);
        actual.printStackTrace(System.out);
        System.out.println("DN_EXCEPTION_END\t" + label);
        System.out.println("DN_FAILURE\t" + label + "\t" + top.getClassName() + "\t" +
                           top.getMethodName() + "\t" + top.getFileName() + "\t" + top.getLineNumber());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "original archive and declared profile");
        Path jar = Path.of(args[0]).toRealPath();
        String profile = args[1];
        require(profile.equals("empty") || profile.equals("nulltarget") || profile.equals("nullworld"),
                "declared finite profile");
        Locale.setDefault(Locale.ROOT);
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException("network forbidden"); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException("listener forbidden"); }
            @Override public void checkLink(String library) { ++natives; throw new SecurityException("native link forbidden: " + library); }
            @Override public void checkWrite(String file) { throw new SecurityException("write forbidden"); }
            @Override public void checkDelete(String file) { throw new SecurityException("delete forbidden"); }
            @Override public void checkExec(String command) { throw new SecurityException("subprocess forbidden"); }
        });
        for (String name : new String[]{"dn", "em", "xw", "sn", "ju", "eq", "in", "nw", "ge"}) {
            Class<?> type = Class.forName(name, false, EffectRenderObservation.class.getClassLoader());
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar), "unchanged original class origin " + name);
        }
        dn renderer = new dn(null, null);
        managerRow("constructed", renderer);
        if (profile.equals("empty")) {
            for (int i = 0; i < PARTIALS.length; ++i) {
                String before = manager(renderer);
                renderer.b((sn)null, Float.intBitsToFloat(PARTIALS[i]));
                require(before.equals(manager(renderer)), "empty DN.b state/Random passivity");
                System.out.println("DN_DRAW\tb\t" + i + "\t" + fhex(PARTIALS[i]) + "\treturn");
                try {
                    renderer.a((sn)null, Float.intBitsToFloat(PARTIALS[i]));
                    throw new AssertionError("missing actual DN.a null camera failure");
                } catch (NullPointerException actual) { exception("a" + i, actual, "dn", "a"); }
                require(before.equals(manager(renderer)), "null DN.a state/Random passivity");
                managerRow("after" + i, renderer);
            }
        } else {
            picked = new ju((fd)null);
            picked.e(1.25, 2.5, -3.75); // Actual source position setter, declared primitive operands.
            if (profile.equals("nullworld")) {
                target = new ju((fd)null);
                target.e(4.0, 5.0, 6.0);
            }
            particle = new em(null, picked, target, .375f);
            System.out.println("DN_INPUT\t" + profile + "\tpicked=JU(null),position(1.25,2.5,-3.75)\ttarget=" +
                (target == null ? "null" : "JU(null),position(4,5,6)") + "\tEMheight=3ec00000");
            renderer.a(particle);
            managerRow("added", renderer);
            System.out.println("DN_PARTICLES\tconstructed\t" + particles());
            String nwBaseline = null;
            for (int i = 0; i < PARTIALS.length; ++i) {
                String before = manager(renderer), beforeParticles = particles();
                try {
                    renderer.b((sn)null, Float.intBitsToFloat(PARTIALS[i]));
                    throw new AssertionError("missing actual EM reached failure");
                } catch (NullPointerException actual) { exception("b" + i, actual, "em", "a"); }
                require(before.equals(manager(renderer)) && beforeParticles.equals(particles()),
                        "DN/EM/JU source fields and actual Randoms remain unchanged");
                String currentNW = nwState();
                if (nwBaseline == null) nwBaseline = currentNW;
                require(nwBaseline.equals(currentNW), "same genuine NW state after each failure");
                System.out.println("DN_DRAW\tb\t" + i + "\t" + fhex(PARTIALS[i]) + "\tthrow");
                managerRow("after" + i, renderer);
                System.out.println("DN_PARTICLES\tafter" + i + "\t" + beforeParticles);
            }
            System.out.println("DN_NW\t" + nwBaseline);
        }
        renderer.a((fd)null); // Actual World store then all four real List.clear calls.
        managerRow("cleared", renderer);
        List<?>[] lists = (List<?>[])get(renderer, dn.class, "b");
        for (List<?> list : lists) require(list.isEmpty(), "actual source World-clear cleanup");
        String before = manager(renderer);
        renderer.b((sn)null, Float.intBitsToFloat(0x7f812345));
        require(before.equals(manager(renderer)), "actual post-clear empty DN.b return");
        require(network == 0 && natives == 0 && exceptions == PARTIALS.length,
                "complete actual failure inventory and zero external work");
        System.out.println("Original DN render: " + profile + "; source-world-clear-complete; network=0; native=0; " +
                           "no-context; exceptions=" + exceptions);
        require(!System.out.checkError(), "complete stdout capture");
    }
}
