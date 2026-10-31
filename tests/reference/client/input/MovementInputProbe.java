import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

/** Invoke unchanged original input methods, with a null unread Player argument. */
public final class MovementInputProbe {
    private static final String[] NAMED = {"m", "n", "o", "p", "q", "r", "s", "t", "u", "v"};

    private static Field named(int index) throws Exception {
        return kv.class.getField(NAMED[index]);
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("plan path");
        Locale.setDefault(Locale.ROOT);
        uo input = null;
        kv settings = null;
        boolean[] keysIdentity = null;
        Field keyField = lr.class.getDeclaredField("f");
        Field settingsField = lr.class.getDeclaredField("g");
        keyField.setAccessible(true);
        settingsField.setAccessible(true);
        int record = 0;
        for (String line : Files.readAllLines(Path.of(args[0]), StandardCharsets.US_ASCII)) {
            String[] parts = line.split(" ");
            String status = "OK";
            try {
                switch (parts[0]) {
                    case "create":
                        settings = parts[1].equals("options") ? new kv() : null;
                        input = parts[1].equals("base") ? new uo() : new lr(settings);
                        keysIdentity = input instanceof lr ? (boolean[])keyField.get(input) : null;
                        break;
                    case "fields":
                        input.a = Float.intBitsToFloat((int)Long.parseLong(parts[1], 16));
                        input.b = Float.intBitsToFloat((int)Long.parseLong(parts[2], 16));
                        input.c = !parts[3].equals("0");
                        input.d = !parts[4].equals("0");
                        input.e = !parts[5].equals("0");
                        break;
                    case "event": input.a(Integer.parseInt(parts[1]), !parts[2].equals("0")); break;
                    case "update": input.a((gs)null); break;
                    case "reset": input.a(); break;
                    case "array_null": settings.w = null; break;
                    case "code": ((qb)named(Integer.parseInt(parts[1])).get(settings)).b = Integer.parseInt(parts[2]); break;
                    case "alias": named(Integer.parseInt(parts[1])).set(settings, named(Integer.parseInt(parts[2])).get(settings)); break;
                    case "null": named(Integer.parseInt(parts[1])).set(settings, null); break;
                    case "replace": named(Integer.parseInt(parts[1])).set(settings, new qb("", Integer.parseInt(parts[2]))); break;
                    default: throw new IllegalArgumentException("plan operation");
                }
            } catch (NullPointerException exception) {
                if (!parts[0].equals("event")) throw exception;
                status = "NPE";
            }
            boolean options = input instanceof lr;
            boolean[] keys = options ? (boolean[])keyField.get(input) : null;
            if (keys != keysIdentity || (options && settingsField.get(input) != settings))
                throw new AssertionError("source identity changed");
            int mask = 0;
            if (keys != null)
                for (int i = 0; i < keys.length; ++i) if (keys[i]) mask |= 1 << i;
            System.out.printf(Locale.ROOT, "%d %s %s %08x %08x %d %d %d %d %d %d%n",
                record++, status, options ? "options" : "base",
                Float.floatToRawIntBits(input.a), Float.floatToRawIntBits(input.b),
                input.c ? 1 : 0, input.d ? 1 : 0, input.e ? 1 : 0,
                settings != null ? 1 : 0, keys != null ? keys.length : 0, mask);
        }
    }
}
