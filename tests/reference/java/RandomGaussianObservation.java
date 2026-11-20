import java.io.BufferedInputStream;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Actual ordinary Random calls with read-only, single-threaded state snapshots. */
public final class RandomGaussianObservation {
    private static final int MAX_LINE = 160;
    private static final int MAX_OPERATIONS = 100000;

    private static final class Lines {
        private final BufferedInputStream input = new BufferedInputStream(System.in);
        private final char[] characters = new char[MAX_LINE];
        private boolean skipLf;

        String next() throws IOException {
            int length = 0;
            for (;;) {
                int value = input.read();
                if (value == -1) {
                    return length == 0 ? null : new String(characters, 0, length);
                }
                if (skipLf) {
                    skipLf = false;
                    if (value == '\n') continue;
                }
                if (value == '\r') {
                    skipLf = true;
                    return new String(characters, 0, length);
                }
                if (value == '\n') return new String(characters, 0, length);
                if (value < 0x20 || value > 0x7e || length == MAX_LINE) {
                    throw new IllegalArgumentException("ASCII line limit");
                }
                characters[length++] = (char)value;
            }
        }
    }

    private static final class StateReader {
        private final Field seed = field("seed", AtomicLong.class);
        private final Field cached = field("nextNextGaussian", double.class);
        private final Field valid = field("haveNextNextGaussian", boolean.class);

        StateReader() throws ReflectiveOperationException {
        }

        private static Field field(String name, Class<?> type) throws ReflectiveOperationException {
            Field result = Random.class.getDeclaredField(name);
            if (result.getType() != type) throw new IllegalStateException("Random field type");
            result.setAccessible(true);
            return result;
        }

        String snapshot(Random owner) throws IllegalAccessException {
            if (owner == null) return "absent";
            long state = ((AtomicLong)seed.get(owner)).get();
            long cachedBits = Double.doubleToRawLongBits(cached.getDouble(owner));
            boolean haveCached = valid.getBoolean(owner);
            return hex64(state) + ":" + hex64(cachedBits) + ":" + (haveCached ? "1" : "0");
        }
    }

    private static String hex32(int value) {
        return String.format(Locale.ROOT, "%08x", value);
    }

    private static String hex64(long value) {
        return String.format(Locale.ROOT, "%016x", value);
    }

    private static double positiveFinite64(String text) {
        if (text.length() != 16) throw new IllegalArgumentException("binary64 hex width");
        for (int index = 0; index < text.length(); ++index) {
            char value = text.charAt(index);
            if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
                  (value >= 'A' && value <= 'F'))) {
                throw new IllegalArgumentException("binary64 hex digit");
            }
        }
        long bits = Long.parseUnsignedLong(text, 16);
        if (bits <= 0 || bits >= 0x7ff0000000000000L) {
            throw new IllegalArgumentException("positive finite binary64 required");
        }
        return Double.longBitsToDouble(bits);
    }

    private static void fields(String[] words, int count) {
        if (words.length != count) throw new IllegalArgumentException("command fields");
    }

    private static int ownerId(String text) {
        int id = Integer.parseInt(text);
        if (id < 0 || id > 1) throw new IllegalArgumentException("owner identity");
        return id;
    }

    private static Random owner(Random[] owners, String text) {
        Random result = owners[ownerId(text)];
        if (result == null) throw new IllegalArgumentException("absent owner");
        return result;
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 0) throw new IllegalArgumentException("no arguments");
        StateReader reader = new StateReader();
        Random[] owners = new Random[2];
        Lines input = new Lines();
        BufferedWriter output = new BufferedWriter(
            new OutputStreamWriter(System.out, StandardCharsets.US_ASCII));
        int operations = 0;
        String line;
        while ((line = input.next()) != null) {
            if (++operations > MAX_OPERATIONS) throw new IllegalArgumentException("operation limit");
            String[] words = line.split(" ", -1);
            String result = "-";
            switch (words[0]) {
                case "new": {
                    fields(words, 3);
                    int id = ownerId(words[1]);
                    long seed = Long.parseLong(words[2]);
                    owners[id] = new Random(seed);
                    break;
                }
                case "seed": {
                    fields(words, 3);
                    Random random = owner(owners, words[1]);
                    long seed = Long.parseLong(words[2]);
                    random.setSeed(seed);
                    break;
                }
                case "gaussian":
                    fields(words, 2);
                    result = hex64(Double.doubleToRawLongBits(owner(owners, words[1]).nextGaussian()));
                    break;
                case "int":
                    fields(words, 2);
                    result = hex32(owner(owners, words[1]).nextInt());
                    break;
                case "long":
                    fields(words, 2);
                    result = hex64(owner(owners, words[1]).nextLong());
                    break;
                case "float":
                    fields(words, 2);
                    result = hex32(Float.floatToRawIntBits(owner(owners, words[1]).nextFloat()));
                    break;
                case "double":
                    fields(words, 2);
                    result = hex64(Double.doubleToRawLongBits(owner(owners, words[1]).nextDouble()));
                    break;
                case "bounded": {
                    fields(words, 3);
                    Random random = owner(owners, words[1]);
                    int bound = Integer.parseInt(words[2]);
                    int value;
                    try {
                        value = random.nextInt(bound);
                    } catch (IllegalArgumentException invalidBound) {
                        result = "bound-error";
                        break;
                    }
                    result = hex32(value);
                    break;
                }
                case "snapshot":
                    fields(words, 2);
                    ownerId(words[1]);
                    break;
                case "log":
                    fields(words, 2);
                    result = hex64(Double.doubleToRawLongBits(StrictMath.log(positiveFinite64(words[1]))));
                    break;
                case "sqrt":
                    fields(words, 2);
                    result = hex64(Double.doubleToRawLongBits(StrictMath.sqrt(positiveFinite64(words[1]))));
                    break;
                default:
                    throw new IllegalArgumentException("unknown command");
            }
            String first = reader.snapshot(owners[0]);
            String second = reader.snapshot(owners[1]);
            output.write(words[0] + " " + result + " " + first + " " + second + "\n");
            output.flush();
            if (System.out.checkError()) throw new IOException("output failure");
        }
    }
}
