import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Calls unchanged real constructors and methods; no substituted game receiver. */
public final class ColorLookupObservation {
    public static void main(String[] args) throws Exception {
        if (args.length != 2) throw new IllegalArgumentException("directory profile");
        String profile = args[1];
        LookupObservation o = new LookupObservation(args[0], true);
        System.err.println("LOOKUP_BOUNDARY profile-before");
        List<Object> events = new ArrayList<>();
        if (profile.equals("default") || profile.equals("zip")) {
            o.requirePack(profile);
            for (String[] call : new String[][] {
                {"water", "/misc/watercolor.png"}, {"cache", "/misc/watercolor.png"},
                {"grass", "/misc/grasscolor.png"}, {"foliage", "/misc/foliagecolor.png"},
                {"missing", "/lookup-absent.png"}, {"clamp", "%clamp%/misc/watercolor.png"},
                {"blur", "%blur%/misc/watercolor.png"}})
                events.add(o.color(call[0], call[1]));
        } else if (profile.equals("frontiers")) {
            for (String[] call : new String[][] {{"nonimage", "/font.txt"},
                {"strip", "##/misc/watercolor.png"}, {"corrupt", "%blur%/lookup-truncated.png"},
                {"null-key", null}}) events.add(o.color(call[0], call[1]));
        } else if (profile.equals("priority")) {
            events.add(o.color("water", "/misc/watercolor.png"));
            Field list = LookupObservation.field(o.engine.getClass(), "l");
            list.set(o.engine, null);
            events.add(o.color("null-list-cache", "/misc/watercolor.png"));
            list.set(o.engine, o.packs);
            Field selected = LookupObservation.field(o.packs.getClass(), "a");
            Object saved = selected.get(o.packs);
            selected.set(o.packs, null);
            try {
                events.add(o.color("null-pack-cache", "/misc/watercolor.png"));
                events.add(o.color("null-pack-miss", "/different-absent.png"));
            } finally { selected.set(o.packs, saved); }
        } else if (profile.equals("statics") || profile.equals("first-setter")) {
            Object array = LookupObservation.invoke(o.engine, "a", new Class<?>[]{String.class},
                new Object[]{"/misc/watercolor.png"});
            for (String name : new String[]{"hw", "ia", "jh"}) {
                Class<?> owner = Class.forName(name, false, o.loader);
                Field slot = LookupObservation.field(owner, "a");
                Method setter = owner.getDeclaredMethod("a", int[].class);
                if (profile.equals("statics")) {
                    LookupObservation.boundary("static-before " + name);
                    int[] initial = (int[])slot.get(null);
                    LookupObservation.boundary("static-after " + name);
                    events.add(LookupObservation.map("label", name + "-initial", "outcome", "return",
                        "array", o.array(initial)));
                }
                LookupObservation.boundary("setter-before " + name);
                setter.invoke(null, array); // Actual use; logs prove cold or report warm.
                LookupObservation.boundary("setter-after " + name);
                events.add(LookupObservation.map("label", name + "-set", "outcome", "return",
                    "array", o.array((int[])slot.get(null)), "same", slot.get(null) == array));
            }
            ((int[])array)[0] ^= 0x00123456; // Declared actual-array fixture mutation.
            for (String name : new String[]{"hw", "ia", "jh"}) {
                Class<?> owner = Class.forName(name, false, o.loader);
                events.add(LookupObservation.map("label", name + "-mutated", "outcome", "return",
                    "array", o.array((int[])LookupObservation.field(owner, "a").get(null))));
            }
            events.add(o.color("cache-mutated", "/misc/watercolor.png"));
        } else if (profile.equals("helpers")) {
            BufferedImage image = new BufferedImage(3, 2, BufferedImage.TYPE_INT_ARGB);
            for (int y = 0; y < 2; ++y) for (int x = 0; x < 3; ++x)
                image.setRGB(x, y, ((x + y) % 2 == 0 ? 0 : 0xff000000) |
                    ((x * 31 + 1) << 16) | ((y * 67 + 1) << 8) | (x + y + 1));
            events.add(o.helper("new", image, null, false));
            for (int length : new int[]{6, 8, 0, 1, 5}) {
                int[] destination = new int[length];
                java.util.Arrays.fill(destination, 0x11223344);
                events.add(o.helper("fill-" + length, image, destination, true));
            }
            events.add(o.helper("fill-null", image, null, true));
            events.add(o.helper("null-image-new", null, null, false));
            events.add(o.helper("null-image-fill", null, new int[1], true));
        } else throw new IllegalArgumentException("unknown color profile");
        System.err.println("LOOKUP_BOUNDARY profile-after");
        o.finish("color", profile, events, null);
    }
}

/** Shared authored serializer/setup. Every game object is genuinely constructed. */
final class LookupObservation {
    final ClassLoader loader = LookupObservation.class.getClassLoader();
    final Object app, settings, packs, engine;
    final IdentityHashMap<int[], Integer> arrays = new IdentityHashMap<>();
    LookupObservation(String directory, boolean color) throws Exception {
        System.err.println("LOOKUP_BOUNDARY setup-before");
        Class<?> mc = Class.forName("net.minecraft.client.Minecraft", false, loader);
        Class<?> kq = Class.forName("kq", false, loader);
        Class<?> applet = Class.forName("net.minecraft.client.MinecraftApplet", false, loader);
        Constructor<?> constructor = kq.getDeclaredConstructor(java.awt.Component.class,
            java.awt.Canvas.class, applet, int.class, int.class, boolean.class, java.awt.Frame.class);
        constructor.setAccessible(true);
        app = constructor.newInstance(null, null, null, 854, 480, false, null);
        Class<?> kv = Class.forName("kv", false, loader);
        settings = kv.getDeclaredConstructor(mc, File.class).newInstance(app, new File(directory));
        field(mc, "z").set(app, settings); // Explicit genuine prepared-field publication, not startup.
        Class<?> ik = Class.forName("ik", false, loader);
        packs = ik.getDeclaredConstructor(mc, File.class).newInstance(app, new File(directory));
        engine = Class.forName("ji", false, loader).getDeclaredConstructor(ik, kv)
            .newInstance(packs, color ? null : settings);
        System.err.println("LOOKUP_BOUNDARY setup-after");
    }
    static void boundary(String name) {
        System.err.println("LOOKUP_BOUNDARY " + name);
        System.err.flush();
    }
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object get(Object value, String name) throws Exception {
        return field(value.getClass(), name).get(value);
    }
    static Object invoke(Object receiver, String name, Class<?>[] signature, Object[] arguments)
            throws Exception {
        Method m = receiver.getClass().getDeclaredMethod(name, signature); m.setAccessible(true);
        try { return m.invoke(receiver, arguments); }
        catch (InvocationTargetException e) {
            Throwable cause = e.getCause();
            if (cause instanceof Exception) throw (Exception)cause;
            if (cause instanceof Error) throw (Error)cause;
            throw e;
        }
    }
    void requirePack(String profile) throws Exception {
        Object selected = get(packs, "a");
        if (selected == null || !selected.getClass().getName().equals(profile.equals("zip") ? "vp" : "pa"))
            throw new IllegalStateException("actual selected Pack refusal/fallback");
    }
    static Map<String, Object> map(Object... entries) {
        if (entries.length % 2 != 0) throw new IllegalArgumentException("map pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < entries.length; i += 2) {
            String key = (String)entries[i];
            if (result.containsKey(key)) throw new IllegalArgumentException("duplicate key");
            result.put(key, entries[i + 1]);
        }
        return result;
    }
    static String hexBytes(ByteBuffer bytes, int length) {
        StringBuilder text = new StringBuilder(length * 2);
        for (int i = 0; i < length; ++i) hex(text, bytes.get(i) & 255, 2);
        return text.toString();
    }
    static void hex(StringBuilder text, int value, int digits) {
        final String alphabet = "0123456789abcdef";
        for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
            text.append(alphabet.charAt((value >>> shift) & 15));
    }
    static String key(String value) {
        if (value == null) return null;
        StringBuilder text = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i)
            hex(text, value.charAt(i), 4);
        return text.toString();
    }
    int identity(int[] value) {
        Integer found = arrays.get(value);
        if (found == null) { found = arrays.size() + 1; arrays.put(value, found); }
        return found;
    }
    Object array(int[] value) {
        if (value == null) return null;
        StringBuilder text = new StringBuilder(value.length * 8);
        for (int word : value) hex(text, word, 8);
        return map("id", identity(value), "length", value.length, "hex", text.toString());
    }
    Object state() throws Exception {
        Map<?, ?> b = (Map<?, ?>)get(engine, "b"), c = (Map<?, ?>)get(engine, "c");
        List<Object> bs = new ArrayList<>(), cs = new ArrayList<>();
        for (Map.Entry<?, ?> entry : b.entrySet()) bs.add(map("key", key((String)entry.getKey()), "id", entry.getValue()));
        for (Map.Entry<?, ?> entry : c.entrySet()) cs.add(map("key", key((String)entry.getKey()), "id", identity((int[])entry.getValue())));
        IntBuffer e = (IntBuffer)get(engine, "e"); ByteBuffer f = (ByteBuffer)get(engine, "f");
        Class<?> ge = Class.forName("ge", false, loader);
        return map("b", bs, "c", cs, "d_count", ((Map<?, ?>)get(engine, "d")).size(),
            "j", get(engine, "j"), "k", get(engine, "k"), "ledger", new ArrayList<>((List<?>)field(ge, "b").get(null)),
            "e", List.of(e.position(), e.limit(), e.capacity()), "e0", e.get(0), "f", List.of(f.position(), f.limit(), f.capacity()),
            "l_null", get(engine, "l") == null, "selected_null", get(packs, "a") == null);
    }
    Object color(String label, String name) throws Exception {
        System.err.println("LOOKUP_BOUNDARY call-before " + label);
        int[] result = null; String exception = null;
        try { result = (int[])invoke(engine, "a", new Class<?>[]{String.class}, new Object[]{name}); }
        catch (Exception e) { exception = e.getClass().getName(); }
        System.err.println("LOOKUP_BOUNDARY call-after " + label);
        return map("label", label, "outcome", exception == null ? "return" : "throw", "exception", exception,
            "array", array(result), "state", state());
    }
    Object helper(String label, BufferedImage image, int[] destination, boolean fill) throws Exception {
        int[] result = null; String exception = null;
        System.err.println("LOOKUP_BOUNDARY call-before " + label);
        try {
            result = (int[])invoke(engine, fill ? "a" : "b",
                fill ? new Class<?>[]{BufferedImage.class, int[].class} : new Class<?>[]{BufferedImage.class},
                fill ? new Object[]{image, destination} : new Object[]{image});
        } catch (Exception e) { exception = e.getClass().getName(); }
        System.err.println("LOOKUP_BOUNDARY call-after " + label);
        Map<String,Object> record = map("label", label, "outcome", exception == null ? "return" : "throw", "exception", exception,
            "array", array(result), "destination", array(destination));
        if (label.equals("new") && result != null && result.length != 0) {
            int saved = result[0], pixel = image.getRGB(0, 0);
            result[0] ^= 0x00123456;
            record.put("image_plane_independent", image.getRGB(0, 0) == pixel);
            result[0] = saved;
        }
        return record;
    }
    void finish(String mode, String profile, List<Object> events, Object device) throws Exception {
        Object selected = get(packs, "a");
        Object record = map("schema", 1, "runtime", "original", "mode", mode, "profile", profile,
            "setup", map("actual_app", app.getClass().getName().equals("kq"),
                "app_z_alias", field(app.getClass().getSuperclass(), "z").get(app) == settings,
                "pack", selected == null ? null : selected.getClass().getName(), "engine_i_null", get(engine, "i") == null),
            "device", device, "events", events);
        System.out.println("LOOKUP_RECORD " + json(record));
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Boolean || value instanceof Number) return value.toString();
        if (value instanceof String) {
            StringBuilder text = new StringBuilder("\"");
            for (char c : ((String)value).toCharArray()) {
                if (c == '"' || c == '\\') text.append('\\').append(c);
                else if (c < 32 || c > 126) text.append(String.format(java.util.Locale.ROOT, "\\u%04x", (int)c));
                else text.append(c);
            }
            return text.append('"').toString();
        }
        if (value instanceof Map<?, ?>) {
            StringBuilder text = new StringBuilder("{");
            for (Map.Entry<?, ?> entry : ((Map<?, ?>)value).entrySet()) {
                if (text.length() > 1) text.append(',');
                text.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return text.append('}').toString();
        }
        if (value instanceof List<?>) {
            StringBuilder text = new StringBuilder("[");
            for (Object entry : (List<?>)value) { if (text.length() > 1) text.append(','); text.append(json(entry)); }
            return text.append(']').toString();
        }
        throw new IllegalArgumentException("unknown authored JSON type " + value.getClass());
    }
}
