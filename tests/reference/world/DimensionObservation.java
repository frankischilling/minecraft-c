import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.IdentityHashMap;

/**
 * Observe original dimension construction, light tables, and celestial angles.
 *
 * CLI: DimensionObservation <client|server> <original-jar>. No stdin commands,
 * output files, World instance, custom subclass, native producer, or math replacement
 * are used. The outer recorder owns JDK/JAR/source/class hashes and processes.
 * All original classes must come from the supplied isolated archive. The
 * archive and selected JDK must be pinned before/after by that recorder.
 *
 * Fixed corpus: actual factory calls for -1,0,+1; four invalid-ID controls;
 * and one fresh normal provider for a separate null-World bind observation.
 * Table setup invokes the actual resolved e()V/client or c()V/server without
 * binding a World. Normal angle calls include every time -23999..23999 at
 * partial1, plus 35 selected signed times times 20 finite partials. Each other
 * kind gets 20 representative pairs, not that full grid. No cosine, angle,
 * remainder, table value, or expected return is computed by this helper.
 *
 * The normal null bind invokes the actual final base a(World) with null and
 * records its actual exception and field prefix. Source puts a before entering
 * xv/ph and the first World seed read fails before assigning b/table filling;
 * observing null both before/after cannot alone prove that same-value store.
 * The -1/+1 binds are deliberately skipped: their biome setup can initialize
 * original registries/assets and is outside this bounded experiment.
 *
 * Output is DIMENSION_OBSERVATION_V1, ASCII TAB fields, literal LF. Reference
 * 0 means null; positive IDs come from one strong IdentityHashMap. No original
 * object's hashCode/toString, identityHashCode, collection disabling, or JDK
 * field inspection is used.
 * Every float is raw eight-digit lowercase hex, including signed zero. Long
 * input hex has 16 digits; UTF-16 diagnostics use four digits per code unit.
 * Nullable text has length-1 and '-', nonnull empty length0 and '-'.
 *
 * Rows and fields after kind:
 * CORPUS: selected-time-count,partial-count,full-first,full-last,full-partial-bits
 * TIME: index,signed-long,raw-long-bits
 * PARTIAL: index,raw-float-bits
 * FACTORY: call-index,role,requested-id,actual-reference,actual-class-or-dash
 * STATE: stage,factory-index,provider-reference,actual-class,world-reference,
 *        world-class-or-dash,biome-reference,biome-class-or-dash,c,d,e,g
 * ARRAY: stage,factory-index,provider-reference,field,reference,class-or-dash,
 *        actual-length-or-minus1,concatenated-raw-float-bits-or-dash
 * REL: label,left-reference,right-reference,actual-same-reference0-or1
 * RESOLVED: factory-index,provider-reference,operation,declaring-class,
 *           member-name,JVM-descriptor
 * TABLE: factory-index,provider-reference,declaring-class,member-name
 * ANGLE: factory-index,provider-reference,corpus,zero-based-angle-call-index,
 *        actual-signed-time,actual-partial-bits,actual-return-bits
 * BIND_NULL: factory-index,provider-reference,RETURN-or-THROW,exception-class-
 *            or-dash,message-length,message-UTF16,first-frame-class-or-dash,
 *            first-frame-method-or-dash,first-frame-line-or-minus1
 * SKIP: dimension-id,operation,reason
 * END: factory-calls,actual-nonnull-factory-results,states,table-calls,
 *      angle-calls,relations,nonnull-reference-count,preceding-row-count
 *
 * STATE reads the eight actual base fields in declaration order; its two ARRAY
 * rows retain f/h bits and identities at that same snapshot. Only display bits
 * are copied. Neither original arrays nor fields are modified by inspection.
 * An invalid factory's result is printed as actually returned, not synthesized.
 * A target Error is fatal; an unexpected reflection/setup failure is fatal.
 * Successful stdout is withheld until the corpus and loader close complete.
 * Bounds: 4 MiB stdout,60000 rows,128 references,16 elements per observed
 * float array,4096 UTF-16 units per exception message. These are observer
 * bounds, not general game/JVM resource or native equivalence guarantees.
 * No full World, biome binding, wire order, or intrinsic-selection claim follows.
 */
public final class DimensionObservation {
    private static final int BYTE_LIMIT = 4 * 1024 * 1024;
    private static final int ROW_LIMIT = 60000;
    private static final char[] HEX = "0123456789abcdef".toCharArray();
    private static final long[] TIMES = {
        Long.MIN_VALUE, Long.MIN_VALUE + 1,
        -48001, -48000, -47999, -24001, -24000, -23999,
        -18001, -18000, -17999, -6001, -6000, -5999, -1, 0, 1,
        5998, 5999, 6000, 6001, 11999, 12000, 12001,
        17999, 18000, 18001, 23999, 24000, 24001, 47999, 48000, 48001,
        Long.MAX_VALUE - 1, Long.MAX_VALUE
    };
    private static final float[] PARTIALS = {
        -0.0f, 0.0f, Float.intBitsToFloat(0x00000001), Float.intBitsToFloat(0x80000001),
        Float.intBitsToFloat(0x3effffff), 0.5f, Float.intBitsToFloat(0x3f000001),
        Float.intBitsToFloat(0x3f7fffff), 1.0f, Float.intBitsToFloat(0x3f800001),
        -1.0f, 2.0f, -2.0f, 30000.0f, 54000.0f, 78000.0f,
        Float.MAX_VALUE, -Float.MAX_VALUE, Float.MIN_NORMAL, -Float.MIN_NORMAL
    };
    private static final int[] REPRESENTATIVE_TIMES = {
        0, 1, 33, 34, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29
    };

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static String hex(long value, int digits) {
        char[] text = new char[digits];
        for (int i = 0; i < digits; ++i)
            text[i] = HEX[(int) ((value >>> (4 * (digits - 1 - i))) & 15)];
        return new String(text);
    }

    private static String bits(float value) {
        return hex(Float.floatToRawIntBits(value) & 0xffffffffL, 8);
    }

    private static String units(String value) {
        if (value == null || value.isEmpty()) return "-";
        require(value.length() <= 4096, "exception message bound");
        char[] text = new char[value.length() * 4];
        for (int i = 0; i < value.length(); ++i) {
            int unit = value.charAt(i);
            for (int j = 0; j < 4; ++j)
                text[i * 4 + j] = HEX[(unit >>> (12 - j * 4)) & 15];
        }
        return new String(text);
    }

    private static Object invoke(Method method, Object owner, Object... arguments) throws Exception {
        try {
            return method.invoke(owner, arguments);
        } catch (InvocationTargetException exception) {
            Throwable cause = exception.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            if (cause instanceof Error) throw (Error) cause;
            throw new AssertionError(cause);
        }
    }

    private static final class Binding {
        final Path jar;
        final URLClassLoader loader;
        final Class<?> base, world, biome;
        final Method factory, bind;
        final Field a, b, c, d, e, f, g, h;
        final String lightName, worldDescriptor;

        Binding(URLClassLoader loader, Path jar, boolean client) throws Exception {
            this.loader = loader;
            this.jar = jar;
            base = type(client ? "xa" : "os");
            world = type(client ? "fd" : "dj");
            biome = type(client ? "xv" : "ph");
            require(Modifier.isPublic(base.getModifiers()) && Modifier.isAbstract(base.getModifiers()),
                    "dimension base binding");
            factory = base.getDeclaredMethod("a", int.class);
            require(factory.getReturnType() == base && Modifier.isPublic(factory.getModifiers())
                    && Modifier.isStatic(factory.getModifiers()), "factory binding");
            bind = base.getDeclaredMethod("a", world);
            require(bind.getReturnType() == void.class && Modifier.isPublic(bind.getModifiers())
                    && Modifier.isFinal(bind.getModifiers()) && !Modifier.isStatic(bind.getModifiers()),
                    "World bind binding");
            a = field("a", world, false);
            b = field("b", biome, false);
            c = field("c", boolean.class, false);
            d = field("d", boolean.class, false);
            e = field("e", boolean.class, false);
            f = field("f", float[].class, false);
            g = field("g", int.class, false);
            h = field("h", float[].class, true);
            lightName = client ? "e" : "c";
            worldDescriptor = client ? "(Lfd;)V" : "(Ldj;)V";
        }

        private void archiveClass(Class<?> value) throws Exception {
            require(value.getClassLoader() == loader, "foreign class loader: " + value.getName());
            require(value.getProtectionDomain().getCodeSource() != null
                    && Path.of(value.getProtectionDomain().getCodeSource().getLocation().toURI())
                            .toRealPath().equals(jar), "foreign code source: " + value.getName());
        }

        private Class<?> type(String name) throws Exception {
            Class<?> value = Class.forName(name, false, loader);
            archiveClass(value);
            return value;
        }

        private Field field(String name, Class<?> type, boolean isPrivate) throws Exception {
            Field value = base.getDeclaredField(name);
            int modifiers = value.getModifiers();
            require(value.getType() == type && !Modifier.isStatic(modifiers)
                    && !Modifier.isFinal(modifiers)
                    && (isPrivate ? Modifier.isPrivate(modifiers) : Modifier.isPublic(modifiers)),
                    "field binding: " + name);
            value.setAccessible(true);
            return value;
        }

        Method virtual(Object owner, String name, Class<?> result, Class<?>... parameters)
                throws Exception {
            require(owner != null && base.isInstance(owner), "dimension receiver");
            archiveClass(owner.getClass());
            for (Class<?> current = owner.getClass(); base.isAssignableFrom(current);
                 current = current.getSuperclass()) {
                Method value;
                try {
                    value = current.getDeclaredMethod(name, parameters);
                } catch (NoSuchMethodException absent) {
                    continue;
                }
                int modifiers = value.getModifiers();
                require(value.getReturnType() == result && !Modifier.isStatic(modifiers)
                        && !Modifier.isAbstract(modifiers)
                        && (Modifier.isPublic(modifiers) || Modifier.isProtected(modifiers)),
                        "virtual member binding: " + name);
                archiveClass(current);
                value.setAccessible(true);
                return value;
            }
            throw new NoSuchMethodException(name);
        }
    }

    private static final class Snapshot {
        final Object world, biome;
        final boolean c, d, e;
        final float[] f, h;
        final int g;
        final int[] fBits, hBits;

        Snapshot(Binding binding, Object owner) throws Exception {
            world = binding.a.get(owner);
            biome = binding.b.get(owner);
            c = binding.c.getBoolean(owner);
            d = binding.d.getBoolean(owner);
            e = binding.e.getBoolean(owner);
            f = (float[]) binding.f.get(owner);
            g = binding.g.getInt(owner);
            h = (float[]) binding.h.get(owner);
            require(world == null || binding.world.isInstance(world), "World field type");
            require(biome == null || binding.biome.isInstance(biome), "biome field type");
            fBits = raw(f);
            hBits = raw(h);
        }

        private static int[] raw(float[] values) {
            if (values == null) return null;
            require(values.length <= 16, "array snapshot bound");
            int[] result = new int[values.length];
            for (int i = 0; i < values.length; ++i)
                result[i] = Float.floatToRawIntBits(values[i]);
            return result;
        }
    }

    private final Binding binding;
    private final StringBuilder output = new StringBuilder();
    private final IdentityHashMap<Object, Integer> references = new IdentityHashMap<>();
    private int rows, factories, nonnullFactories, states, tables, angles, relations;

    private DimensionObservation(Binding binding) { this.binding = binding; }

    private void row(Object... fields) {
        require(rows < ROW_LIMIT, "row bound");
        for (int i = 0; i < fields.length; ++i) {
            String text = String.valueOf(fields[i]);
            for (int j = 0; j < text.length(); ++j)
                require(text.charAt(j) >= 32 && text.charAt(j) <= 126, "non-ASCII field");
            require(output.length() + text.length() + (i == 0 ? 1 : 2) <= BYTE_LIMIT,
                    "protocol byte bound");
            if (i != 0) output.append('\t');
            output.append(text);
        }
        output.append('\n');
        ++rows;
    }

    private int reference(Object value) {
        if (value == null) return 0;
        Integer previous = references.get(value);
        if (previous != null) return previous;
        require(references.size() < 128, "reference bound");
        int result = references.size() + 1;
        references.put(value, result);
        return result;
    }

    private static String className(Object value) {
        return value == null ? "-" : value.getClass().getName();
    }

    private Object factory(String role, int requested) throws Exception {
        Object value = invoke(binding.factory, null, requested);
        require(value == null || binding.base.isInstance(value), "factory return type");
        if (value != null) {
            binding.archiveClass(value.getClass());
            ++nonnullFactories;
        }
        row("FACTORY", factories, role, requested, reference(value), className(value));
        ++factories;
        return value;
    }

    private void array(String stage, int factory, Object owner, String field,
                       float[] value, int[] raw) {
        StringBuilder encoded = new StringBuilder();
        if (raw != null)
            for (int bits : raw) encoded.append(hex(bits & 0xffffffffL, 8));
        row("ARRAY", stage, factory, reference(owner), field, reference(value), className(value),
                value == null ? -1 : value.length,
                encoded.length() == 0 ? "-" : encoded.toString());
    }

    private Snapshot state(String stage, int factory, Object owner) throws Exception {
        require(owner != null && binding.base.isInstance(owner), "snapshot receiver");
        Snapshot value = new Snapshot(binding, owner);
        row("STATE", stage, factory, reference(owner), className(owner),
                reference(value.world), className(value.world),
                reference(value.biome), className(value.biome),
                value.c ? 1 : 0, value.d ? 1 : 0, value.e ? 1 : 0, value.g);
        array(stage, factory, owner, "f", value.f, value.fBits);
        array(stage, factory, owner, "h", value.h, value.hBits);
        ++states;
        return value;
    }

    private void relation(String label, Object left, Object right) {
        row("REL", label, reference(left), reference(right), left == right ? 1 : 0);
        ++relations;
    }

    private void resolved(int factory, Object owner, String operation, Method method,
                          String descriptor) {
        row("RESOLVED", factory, reference(owner), operation,
                method.getDeclaringClass().getName(), method.getName(), descriptor);
    }

    private void angle(int factory, Object owner, Method method, String corpus,
                       long time, float partial) throws Exception {
        String partialBits = bits(partial);
        Object value = invoke(method, owner, time, partial);
        require(value != null && value.getClass() == Float.class, "angle return type");
        row("ANGLE", factory, reference(owner), corpus, angles, time, partialBits,
                bits(((Float) value).floatValue()));
        ++angles;
    }

    private void nullBind(int factory, Object owner) throws Exception {
        resolved(factory, owner, "BIND_NULL", binding.bind, binding.worldDescriptor);
        Exception actual = null;
        try {
            binding.bind.invoke(owner, (Object) null);
        } catch (InvocationTargetException exception) {
            Throwable cause = exception.getCause();
            if (cause instanceof Error) throw (Error) cause;
            require(cause instanceof Exception, "original bind throwable type");
            actual = (Exception) cause;
        }
        String message = actual == null ? null : actual.getMessage();
        StackTraceElement first = null;
        if (actual != null) {
            StackTraceElement[] trace = actual.getStackTrace();
            if (trace.length != 0) first = trace[0];
        }
        row("BIND_NULL", factory, reference(owner), actual == null ? "RETURN" : "THROW",
                actual == null ? "-" : actual.getClass().getName(),
                message == null ? -1 : message.length(), units(message),
                first == null ? "-" : first.getClassName(),
                first == null ? "-" : first.getMethodName(),
                first == null ? -1 : first.getLineNumber());
    }

    private void run(String side) throws Exception {
        require(TIMES.length == 35 && PARTIALS.length == 20
                && REPRESENTATIVE_TIMES.length == PARTIALS.length, "fixed input shape");
        row("DIMENSION_OBSERVATION_V1", side);
        row("CORPUS", TIMES.length, PARTIALS.length, -23999, 23999, bits(1.0f));
        for (int i = 0; i < TIMES.length; ++i)
            row("TIME", i, TIMES[i], hex(TIMES[i], 16));
        for (int i = 0; i < PARTIALS.length; ++i) {
            require(Float.isFinite(PARTIALS[i]), "finite input partial");
            row("PARTIAL", i, bits(PARTIALS[i]));
        }

        int[] requested = {-1, 0, 1};
        Snapshot[] constructed = new Snapshot[requested.length];
        for (int i = 0; i < requested.length; ++i) {
            Object owner = factory("MAIN", requested[i]);
            require(owner != null, "selected factory returned null");
            Snapshot before = state("CONSTRUCTED", i, owner);
            constructed[i] = before;
            relation("factory-" + i + ".f-h", before.f, before.h);
            for (int previous = 0; previous < i; ++previous) {
                relation("factory-" + i + ".f-vs-" + previous + ".f",
                        before.f, constructed[previous].f);
                relation("factory-" + i + ".h-vs-" + previous + ".h",
                        before.h, constructed[previous].h);
            }

            Method light = binding.virtual(owner, binding.lightName, void.class);
            Method celestial = binding.virtual(owner, "a", float.class, long.class, float.class);
            resolved(i, owner, "LIGHT", light, "()V");
            resolved(i, owner, "ANGLE", celestial, "(JF)F");
            require(invoke(light, owner) == null, "void table return");
            row("TABLE", i, reference(owner), light.getDeclaringClass().getName(), light.getName());
            ++tables;
            Snapshot after = state("AFTER_LIGHT", i, owner);
            relation("factory-" + i + ".light-f-stable", before.f, after.f);
            relation("factory-" + i + ".light-h-stable", before.h, after.h);

            if (requested[i] == 0) {
                for (int time = -23999; time <= 23999; ++time)
                    angle(i, owner, celestial, "FULL", (long) time, 1.0f);
                for (long time : TIMES)
                    for (float partial : PARTIALS)
                        angle(i, owner, celestial, "SELECTED", time, partial);
            } else {
                for (int p = 0; p < PARTIALS.length; ++p) {
                    int time = REPRESENTATIVE_TIMES[p];
                    require(time >= 0 && time < TIMES.length, "representative input index");
                    angle(i, owner, celestial, "REPRESENTATIVE", TIMES[time], PARTIALS[p]);
                }
            }
            Snapshot finalState = state("AFTER_ANGLES", i, owner);
            relation("factory-" + i + ".angle-f-stable", after.f, finalState.f);
            relation("factory-" + i + ".angle-h-stable", after.h, finalState.h);
        }

        for (int invalid : new int[]{Integer.MIN_VALUE, -2, 2, Integer.MAX_VALUE})
            factory("INVALID_CONTROL", invalid);

        int controlIndex = factories;
        Object control = factory("NULL_BIND_NORMAL_CONTROL", 0);
        require(control != null, "normal control factory returned null");
        Snapshot before = state("BEFORE_NULL_BIND", controlIndex, control);
        relation("null-bind.f-h", before.f, before.h);
        nullBind(controlIndex, control);
        Snapshot after = state("AFTER_NULL_BIND", controlIndex, control);
        relation("null-bind.f-stable", before.f, after.f);
        relation("null-bind.h-stable", before.h, after.h);
        row("SKIP", -1, "BIND_NULL", "FIXED_BIOME_REGISTRY_CLOSURE_NOT_ADMITTED");
        row("SKIP", 1, "BIND_NULL", "FIXED_BIOME_REGISTRY_CLOSURE_NOT_ADMITTED");
        row("END", factories, nonnullFactories, states, tables, angles, relations,
                references.size(), rows);
    }

    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 2, "usage: <client|server> <original-jar>");
        boolean client = arguments[0].equals("client");
        require(client || arguments[0].equals("server"), "side");
        Path jar = Path.of(arguments[1]).toRealPath();
        require(Files.isRegularFile(jar), "regular supplied archive");
        DimensionObservation observer;
        try (URLClassLoader loader = new URLClassLoader(new URL[]{jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            observer = new DimensionObservation(new Binding(loader, jar, client));
            observer.run(arguments[0]);
        }
        byte[] raw = observer.output.toString().getBytes(StandardCharsets.US_ASCII);
        require(raw.length <= BYTE_LIMIT && raw.length != 0 && raw[raw.length - 1] == 10,
                "complete bounded protocol");
        System.out.write(raw);
        System.out.flush();
        require(!System.out.checkError(), "stdout failure");
    }
}
