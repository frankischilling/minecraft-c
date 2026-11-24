import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Execute unchanged inherited turning on actual null-World Item/Falling owners. */
public final class EntityTurnProbe {
    private static final class Guard {
        final List<Field> fields = new ArrayList<>();
        final List<Object> values = new ArrayList<>();
        final List<Object> receivers = new ArrayList<>();
        final long seed;
        final long gaussian;
        final boolean cached;

        Guard(sn entity) throws Exception {
            for (Class<?> owner = entity.getClass(); owner != Object.class; owner = owner.getSuperclass())
                capture(owner, entity, owner == sn.class);
            capture(eq.class, entity.aW, false);
            Field seedField = Random.class.getDeclaredField("seed");
            Field gaussianField = Random.class.getDeclaredField("nextNextGaussian");
            Field cachedField = Random.class.getDeclaredField("haveNextNextGaussian");
            seedField.setAccessible(true);
            gaussianField.setAccessible(true);
            cachedField.setAccessible(true);
            seed = ((AtomicLong)seedField.get(entity.bs)).get();
            gaussian = Double.doubleToRawLongBits(gaussianField.getDouble(entity.bs));
            cached = cachedField.getBoolean(entity.bs);
        }

        void capture(Class<?> owner, Object receiver, boolean skipAngles) throws Exception {
            for (Field field : owner.getDeclaredFields()) {
                if (Modifier.isStatic(field.getModifiers())) continue;
                String name = field.getName();
                if (skipAngles && (name.equals("aS") || name.equals("aT") || name.equals("aU") || name.equals("aV"))) continue;
                field.setAccessible(true);
                Object value;
                if (field.getType() == float.class) value = Float.floatToRawIntBits(field.getFloat(receiver));
                else if (field.getType() == double.class) value = Double.doubleToRawLongBits(field.getDouble(receiver));
                else value = field.get(receiver);
                fields.add(field);
                values.add(value);
                receivers.add(receiver);
            }
        }

        void check(sn entity) throws Exception {
            Guard after = new Guard(entity);
            if (after.seed != seed || after.gaussian != gaussian || after.cached != cached || after.fields.size() != fields.size())
                throw new AssertionError("Random or source field set changed");
            for (int i = 0; i < fields.size(); ++i) {
                if (!fields.get(i).equals(after.fields.get(i)) || receivers.get(i) != after.receivers.get(i))
                    throw new AssertionError("source owner identity changed");
                Object before = values.get(i), current = after.values.get(i);
                if (fields.get(i).getType().isPrimitive() ? !before.equals(current) : before != current)
                    throw new AssertionError("unrelated source field changed: " + fields.get(i));
            }
        }
    }

    private static float input(String value) {
        return Float.intBitsToFloat((int)Long.parseLong(value, 16));
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("plan path");
        int ordinal = 0;
        for (String line : Files.readAllLines(Path.of(args[0]), StandardCharsets.US_ASCII)) {
            String[] values = line.split(" ");
            sn entity = values[0].equals("0") ? new hl((fd)null) : new ju((fd)null);
            entity.aS = input(values[1]);
            entity.aT = input(values[2]);
            entity.aU = input(values[3]);
            entity.aV = input(values[4]);
            float yaw = input(values[5]), pitch = input(values[6]);
            int repeat = Integer.parseInt(values[7]);
            for (int step = 0; step < repeat; ++step) {
                Guard guard = new Guard(entity);
                entity.d(yaw, pitch);
                guard.check(entity);
                System.out.printf(Locale.ROOT, "%d %d %08x %08x %08x %08x 1%n", ordinal, step,
                    Float.floatToRawIntBits(entity.aS), Float.floatToRawIntBits(entity.aT),
                    Float.floatToRawIntBits(entity.aU), Float.floatToRawIntBits(entity.aV));
            }
            ++ordinal;
        }
    }
}
