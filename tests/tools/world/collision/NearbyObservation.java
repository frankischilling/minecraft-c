import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Research setup only. Gather, Entity constructors and AABBs execute untouched JAR classes. */
public final class NearbyObservation {
    private static final Class<?>[] SIX = {double.class, double.class, double.class,
        double.class, double.class, double.class};
    private final Constructor<?> normalConstructor, blankConstructor, itemConstructor,
        fallingConstructor;
    private final Method gather, direct;
    private final Field slicesField, entityBounds, dirty, neverSave;
    private final Field[] coordinates = new Field[6];

    private NearbyObservation(ClassLoader loader, boolean client) throws Exception {
        Class<?> world = Class.forName(client ? "fd" : "dj", false, loader);
        Class<?> entity = Class.forName(client ? "sn" : "lq", true, loader);
        Class<?> box = Class.forName(client ? "eq" : "cz", true, loader);
        Class<?> normal = Class.forName(client ? "lm" : "hi", true, loader);
        Class<?> blank = Class.forName(client ? "li" : "hg", true, loader);
        Class<?> item = Class.forName(client ? "hl" : "ez", true, loader);
        Class<?> falling = Class.forName(client ? "ju" : "gl", true, loader);
        normalConstructor = normal.getDeclaredConstructor(world, int.class, int.class);
        blankConstructor = blank.getDeclaredConstructor(world, int.class, int.class);
        itemConstructor = item.getDeclaredConstructor(world);
        fallingConstructor = falling.getDeclaredConstructor(world);
        gather = normal.getDeclaredMethod("a", entity, box, List.class);
        direct = box.getDeclaredMethod("a", SIX);
        slicesField = normal.getDeclaredField("m");
        entityBounds = entity.getDeclaredField(client ? "aW" : "aZ");
        dirty = normal.getDeclaredField("q");
        neverSave = normal.getDeclaredField("p");
        for (int i = 0; i < 6; ++i) {
            coordinates[i] = box.getDeclaredField(String.valueOf((char)('a' + i)));
        }
        // Exercise and report all real constructors before consuming any corpus row.
        Object n = normalConstructor.newInstance(null, 7, -9);
        Object b = blankConstructor.newInstance(null, 7, -9);
        Object i = itemConstructor.newInstance(new Object[] {null});
        Object f = fallingConstructor.newInstance(new Object[] {null});
        if (((List<?>[])slicesField.get(n)).length != 8 ||
            ((List<?>[])slicesField.get(b)).length != 8 || !neverSave.getBoolean(b) ||
            entityBounds.get(i) == null || entityBounds.get(f) == null ||
            entityBounds.get(i) == entityBounds.get(f)) {
            throw new IllegalStateException("unexpected actual constructor objects");
        }
        System.err.println("SETUP\tactual-normal-and-blank-null-World-constructors\t" +
            "actual-Item-and-FallingSand-null-World-constructors\t" +
            "distinct-original-constructor-owned-bounds\tno-Unsafe");
    }

    private static double value(String bits) {
        if (!bits.matches("[0-9a-f]{16}")) {
            throw new IllegalArgumentException("expected lowercase binary64 raw bits");
        }
        return Double.longBitsToDouble(Long.parseUnsignedLong(bits, 16));
    }

    private static String[] boxTokens(String token) {
        String[] tokens = token.split(",", -1);
        if (tokens.length != 6) {
            throw new IllegalArgumentException("expected six box coordinates");
        }
        return tokens;
    }

    private void assign(Object box, String token) throws Exception {
        String[] bits = boxTokens(token);
        for (int i = 0; i < 6; ++i) {
            coordinates[i].setDouble(box, value(bits[i]));
        }
    }

    private Object box(String token) throws Exception {
        if (token.equals("-")) {
            return null;
        }
        String[] bits = boxTokens(token);
        Object[] values = new Object[6];
        for (int i = 0; i < 6; ++i) {
            values[i] = value(bits[i]);
        }
        return direct.invoke(null, values);
    }

    private static Object reference(String token, Object[] entities) {
        if (token.equals("n") || token.equals("-")) {
            return null;
        }
        if (token.equals("x")) {
            return "foreign-list-member";
        }
        return entities[Integer.parseInt(token)];
    }

    private static List<Object> list(String token, Object[] entities) {
        if (token.equals("!") || token.equals("-")) {
            return null;
        }
        List<Object> result = new ArrayList<>();
        if (!token.equals("_")) {
            for (String entry : token.split(",", -1)) {
                result.add(reference(entry, entities));
            }
        }
        return result;
    }

    private static String identities(List<?> values, Object[] entities) {
        if (values == null) {
            return "-";
        }
        StringBuilder text = new StringBuilder();
        for (Object value : values) {
            if (text.length() != 0) {
                text.append(',');
            }
            if (value == null) {
                text.append('n');
                continue;
            }
            int index = 0;
            while (index < entities.length && entities[index] != value) {
                ++index;
            }
            text.append(index == entities.length ? "x" : String.valueOf(index));
        }
        return text.length() == 0 ? "_" : text.toString();
    }

    private void appendBox(StringBuilder text, Object box) throws Exception {
        if (box == null) {
            text.append("null;");
            return;
        }
        for (Field coordinate : coordinates) {
            text.append(String.format(Locale.ROOT, "%016x",
                Double.doubleToRawLongBits(coordinate.getDouble(box)))).append(',');
        }
        text.append(';');
    }

    private String snapshot(Object chunk, Object query, Object[] entities) throws Exception {
        StringBuilder text = new StringBuilder();
        text.append(dirty.getBoolean(chunk)).append('/').append(neverSave.getBoolean(chunk));
        appendBox(text, query);
        for (Object entity : entities) {
            appendBox(text, entityBounds.get(entity));
        }
        List<?>[] slices = (List<?>[])slicesField.get(chunk);
        if (slices == null) {
            text.append("null-m");
        } else {
            text.append(slices.length).append(':');
            for (List<?> slice : slices) {
                text.append(identities(slice, entities)).append('|');
            }
        }
        return text.toString();
    }

    private String row(String input) throws Exception {
        String[] tokens = input.split("\t", -1);
        if (tokens.length != 7 || (!tokens[1].equals("N") && !tokens[1].equals("B"))) {
            throw new IllegalArgumentException("expected seven TSV fields and N/B kind");
        }
        Object chunk = (tokens[1].equals("N") ? normalConstructor : blankConstructor)
            .newInstance(null, 7, -9);
        String[] specifications = tokens[5].equals("_") ? new String[0]
            : tokens[5].split(";", -1);
        Object[] entities = new Object[specifications.length];
        for (int i = 0; i < entities.length; ++i) {
            String[] spec = specifications[i].split(":", -1);
            Constructor<?> constructor = spec[0].equals("I") ? itemConstructor
                : spec[0].equals("F") ? fallingConstructor : null;
            if (constructor == null || spec.length != 2) {
                throw new IllegalArgumentException("expected I/F:box Entity specification");
            }
            entities[i] = constructor.newInstance(new Object[] {null});
            // Keep each actual final constructor-owned bounds reference; only set its doubles.
            assign(entityBounds.get(entities[i]), spec[1]);
        }
        if (tokens[6].equals("-")) {
            slicesField.set(chunk, null);
        } else if (tokens[6].equals("_")) {
            slicesField.set(chunk, new List<?>[0]);
        } else {
            String[] sliceTokens = tokens[6].split("\\|", -1);
            List<?>[] slices = new List<?>[sliceTokens.length];
            for (int i = 0; i < slices.length; ++i) {
                slices[i] = list(sliceTokens[i], entities);
            }
            slicesField.set(chunk, slices);
        }
        Object excluded = reference(tokens[2], entities);
        List<Object> destination = list(tokens[3], entities);
        Object query = box(tokens[4]);
        int before = destination == null ? -1 : destination.size();
        String sourceBefore = snapshot(chunk, query, entities);
        String outcome = "OK";
        try {
            // Invoke the original superclass Method on the actual receiver: ordinary virtual
            // dispatch reaches the genuine Blank override for B, not a helper-side shortcut.
            gather.invoke(chunk, excluded, query, destination);
        } catch (InvocationTargetException error) {
            outcome = error.getCause().getClass().getName();
        }
        return tokens[0] + '\t' + tokens[1] + '\t' + outcome + '\t' + before + '\t' +
            (destination == null ? -1 : destination.size()) + '\t' +
            identities(destination, entities) + '\t' +
            (sourceBefore.equals(snapshot(chunk, query, entities)) ? "1" : "0");
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 ||
            (!arguments[1].equals("CLIENT") && !arguments[1].equals("SERVER"))) {
            throw new IllegalArgumentException("expected original JAR path and CLIENT/SERVER");
        }
        URL jar = Path.of(arguments[0]).toAbsolutePath().toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar},
                ClassLoader.getPlatformClassLoader());
             BufferedReader input = new BufferedReader(new InputStreamReader(System.in,
                 StandardCharsets.US_ASCII));
             PrintWriter output = new PrintWriter(System.out, false, StandardCharsets.US_ASCII)) {
            NearbyObservation observer = new NearbyObservation(loader,
                arguments[1].equals("CLIENT"));
            String line;
            while ((line = input.readLine()) != null) {
                output.println(observer.row(line));
            }
            output.flush();
            if (output.checkError()) {
                throw new IllegalStateException("stdout write failed");
            }
        }
    }
}
