import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.client.Minecraft;

/**
 * Authored observer over genuine supplied objects. No replacement game class,
 * Unsafe allocation, renderer substitute or copied constructor implementation.
 */
public final class NoticeObservation {
    static Object get(Object receiver, Class<?> type, String name) throws Exception {
        Field f = type.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(receiver);
    }

    static String f(Object receiver, Class<?> type, String name) throws Exception {
        return String.format(Locale.ROOT, "%08x",
                             Float.floatToRawIntBits((Float)get(receiver, type, name)));
    }

    static String text(String text) {
        if (text == null)
            return "null";
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < text.length(); i++)
            out.append(String.format(Locale.ROOT, "%04x", (int)text.charAt(i)));
        return out.toString();
    }

    static int b(Object receiver, Class<?> type, String name) throws Exception {
        return (Boolean)get(receiver, type, name) ? 1 : 0;
    }

    static void geometry(vu notice) throws Exception {
        Object item = get(notice, vu.class, "h"), model = get(item, bw.class, "a");
        Object inherited = get(item, bw.class, "e"), own = get(item, bb.class, "e");
        Random random = (Random)get(item, bb.class, "f");
        boolean gaussian = (Boolean)get(random, Random.class, "haveNextNextGaussian");
        long payload =
            Double.doubleToRawLongBits((Double)get(random, Random.class, "nextNextGaussian"));
        System.out.println("MODEL " + f(item, bw.class, "c") + " " + f(item, bw.class, "d") + " " +
                           b(item, bb.class, "a") + " " + (inherited != own ? 1 : 0) + " " +
                           (get(item, bw.class, "b") == null ? 1 : 0) + " " +
                           (!gaussian && payload == 0 ? 1 : 0));
        System.out.println("BIPED " + f(model, ko.class, "m") + " " + b(model, ko.class, "n") +
                           " " + b(model, fh.class, "j") + " " + b(model, fh.class, "k") + " " +
                           b(model, fh.class, "l"));
        for (int p = 0; p < 9; p++) {
            Object part = get(model, fh.class, String.valueOf((char)('a' + p)));
            StringBuilder row = new StringBuilder("PART " + p + " " + get(part, ps.class, "l") +
                                                  " " + get(part, ps.class, "m"));
            for (String name : new String[] {"g", "h", "i", "n"})
                row.append(" ").append(b(part, ps.class, name));
            row.append(" ").append(get(part, ps.class, "o"));
            for (String name : new String[] {"a", "b", "c", "d", "e", "f"})
                row.append(" ").append(f(part, ps.class, name));
            System.out.println(row);
            Object[] vertices = (Object[])get(part, ps.class, "j"),
                     quads = (Object[])get(part, ps.class, "k");
            for (int i = 0; i < 8; i++) {
                Object v = vertices[i], position = get(v, ib.class, "a");
                row = new StringBuilder("VERTEX " + p + " " + i);
                for (String name : new String[] {"a", "b", "c"})
                    row.append(String.format(
                        Locale.ROOT, " %016x",
                        Double.doubleToRawLongBits((Double)get(position, bt.class, name))));
                row.append(" ").append(f(v, ib.class, "b")).append(" ").append(f(v, ib.class, "c"));
                System.out.println(row);
            }
            for (int q = 0; q < 6; q++) {
                Object quad = quads[q];
                row = new StringBuilder("QUAD " + p + " " + q + " " + get(quad, tz.class, "b") +
                                        " " + b(quad, tz.class, "c"));
                for (Object v : (Object[])get(quad, tz.class, "a")) {
                    int index = -1;
                    boolean distinct = true;
                    for (int j = 0; j < 8; j++) {
                        if (get(v, ib.class, "a") == get(vertices[j], ib.class, "a"))
                            index = j;
                        if (v == vertices[j])
                            distinct = false;
                    }
                    row.append(" ")
                        .append(index)
                        .append(" ")
                        .append(distinct ? 1 : 0)
                        .append(" ")
                        .append(f(v, ib.class, "b"))
                        .append(" ")
                        .append(f(v, ib.class, "c"));
                }
                System.out.println(row);
            }
        }
        int ordinal = 0;
        for (Object blocks : new Object[] {inherited, own}) {
            boolean zero = true;
            for (Field field : cv.class.getDeclaredFields()) {
                if (Modifier.isStatic(field.getModifiers()) ||
                    Set.of("d", "b", "O").contains(field.getName()))
                    continue;
                field.setAccessible(true);
                Object value = field.get(blocks);
                if (value instanceof Float)
                    zero &= Float.floatToRawIntBits((Float)value) == 0;
                else if (value instanceof Integer)
                    zero &= (Integer)value == 0;
                else if (value instanceof Boolean)
                    zero &= !(Boolean)value;
                else
                    zero &= value == null;
            }
            System.out.println("BLOCKS " + ordinal++ + " " + get(blocks, cv.class, "d") + " " +
                               b(blocks, cv.class, "b") + " " + get(blocks, cv.class, "O") + " " +
                               (zero ? 1 : 0));
        }
    }

    static void state(String label, vu notice) throws Exception {
        ny achievement = (ny)get(notice, vu.class, "f");
        System.out.println("STATE " + label + " " + text((String)get(notice, vu.class, "d")) + " " +
                           text((String)get(notice, vu.class, "e")) + " " +
                           (achievement == null ? -1 : achievement.e) + " " +
                           b(notice, vu.class, "i"));
    }

    static void require(boolean value, String message) {
        if (!value)
            throw new AssertionError(message);
    }

    static void reminder(vu notice, ny achievement) throws Exception {
        long before = System.currentTimeMillis();
        notice.b(achievement);
        long after = System.currentTimeMillis();
        long time = (Long)get(notice, vu.class, "g");
        require(time >= before - 2500 && time <= after - 2500, "actual reminder clock");
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 1, "geometry|callbacks");
        Path origin = Path.of(vu.class.getProtectionDomain().getCodeSource().getLocation().toURI());
        String hash = HexFormat.of().formatHex(
            MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(origin)));
        require(hash.equals("af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"),
                "actual vu JAR origin");
        System.out.println("ORIGIN " + origin.toUri() + " " + hash + " " +
                           System.getProperty("java.runtime.version"));
        if (args[0].equals("geometry")) {
            AtomicLong u = (AtomicLong)get(null, Random.class, "seedUniquifier");
            long before = u.get();
            vu notice = new vu(null);
            require(u.get() == before * 1181783497276652981L, "one genuine default Random");
            geometry(notice);
        } else {
            kq client = new kq(null, null, null, 854, 480, false, null);
            try {
                require(client.I == null, "constructor alone does not publish stats");
                client.z = new kv();
                vu notice = client.u;
                geometry(notice);
                ep.f.a(new kh(client));
                reminder(notice, ep.f);
                state("E", notice);
                client.z.r.b = 21;
                client.z.w = null;
                reminder(notice, ep.f);
                state("Y_NULL_W", notice);
                client.z = new kv();
                reminder(notice, ep.f);
                state("SETTINGS_REPLACED", notice);
                client.z.r.b = 84;
                reminder(notice, ep.f);
                state("HOLE", notice);
                client.z.r.b = 256;
                try {
                    notice.b(ep.f);
                    throw new AssertionError("missing key index failure");
                } catch (ArrayIndexOutOfBoundsException expected) {
                }
                state("KEY_INDEX", notice);
                kv heldSettings = client.z;
                client.z = null;
                try {
                    notice.b(ep.f);
                    throw new AssertionError("missing settings failure");
                } catch (NullPointerException expected) {
                }
                state("NULL_SETTINGS", notice);
                client.z = heldSettings;
                qb heldBinding = client.z.r;
                client.z.r = null;
                try {
                    notice.b(ep.f);
                    throw new AssertionError("missing binding failure");
                } catch (NullPointerException expected) {
                }
                state("NULL_BINDING", notice);
                client.z.r = heldBinding;
                Object oldE = get(notice, vu.class, "e"), oldF = get(notice, vu.class, "f"),
                       oldI = get(notice, vu.class, "i"), oldG = get(notice, vu.class, "g");
                ep.g.a(new kh(null));
                try {
                    notice.b(ep.g);
                    throw new AssertionError("missing kh null failure");
                } catch (NullPointerException expected) {
                }
                require(get(notice, vu.class, "d") == ep.g.f &&
                            get(notice, vu.class, "e") == oldE &&
                            get(notice, vu.class, "f") == oldF &&
                            get(notice, vu.class, "i").equals(oldI) &&
                            get(notice, vu.class, "g").equals(oldG),
                        "reminder failed prefix");
                state("NULL_CLIENT_PREFIX", notice);
                ep.g.a((gt)null);
                require(ep.g.e() == get(ep.g, ny.class, "l"), "ny.e stored String identity");
                try {
                    notice.b((ny)null);
                    throw new AssertionError("missing reminder null failure");
                } catch (NullPointerException expected) {
                }
                require(get(notice, vu.class, "d") == ep.g.f, "reminder null changes no title");
                try {
                    notice.a((ny)null);
                    throw new AssertionError("missing completion null failure");
                } catch (NullPointerException expected) {
                }
                require(get(notice, vu.class, "e") == oldE && get(notice, vu.class, "f") == oldF &&
                            get(notice, vu.class, "i").equals(oldI) &&
                            get(notice, vu.class, "g").equals(oldG),
                        "completion null prefix");
                state("NULL_COMPLETE_PREFIX", notice);
                long before = System.currentTimeMillis();
                notice.a(ep.g);
                long after = System.currentTimeMillis();
                long time = (Long)get(notice, vu.class, "g");
                require(time >= before && time <= after, "actual completion clock");
                state("COMPLETE", notice);
                client.z.r.b = 18;
                for (int n = 0; n < 1000; n++)
                    reminder(notice, ep.f);
                System.out.println("OWNERSHIP repeated1000");
            } finally {
                client.J = false;
                for (Thread thread : Thread.getAllStackTraces().keySet())
                    if (thread.getName().equals("Timer hack thread")) {
                        thread.interrupt();
                        thread.join(2000);
                        require(!thread.isAlive(), "real timer daemon retired");
                    }
            }
        }
        System.out.println("LIFETIME clean");
    }
}
