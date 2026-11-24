/* Authored observation glue. All view/trig/vector bodies execute from caller JAR. */
import java.io.BufferedReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

public final class ViewObservation {
    private static final class Living extends ls {
        Living() { super(null); }
    }

    private static double d(String bits) {
        return Double.longBitsToDouble(Long.parseUnsignedLong(bits, 16));
    }

    private static float f(String bits) {
        return Float.intBitsToFloat(Integer.parseUnsignedInt(bits, 16));
    }

    private static String vector(bt value) {
        return String.format(Locale.ROOT, " %016x %016x %016x",
            Double.doubleToRawLongBits(value.a), Double.doubleToRawLongBits(value.b),
            Double.doubleToRawLongBits(value.c));
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("authored input path required");
        Living receiver = new Living();
        try (BufferedReader input = Files.newBufferedReader(Path.of(args[0]), StandardCharsets.US_ASCII)) {
            String line;
            while ((line = input.readLine()) != null) {
                String[] fields = line.split(" ");
                if (fields.length != 12) throw new IllegalArgumentException("input field count");
                float partial = f(fields[1]);
                receiver.aM = d(fields[2]); receiver.aN = d(fields[3]); receiver.aO = d(fields[4]);
                receiver.aJ = d(fields[5]); receiver.aK = d(fields[6]); receiver.aL = d(fields[7]);
                receiver.aS = f(fields[8]); receiver.aT = f(fields[9]);
                receiver.aU = f(fields[10]); receiver.aV = f(fields[11]);
                bt.b(); // Explicit caller pool reset, never inserted inside a view helper.
                bt position = receiver.e(partial);
                bt look = receiver.f(partial);
                bt current = receiver.ac();
                System.out.println(line + vector(position) + vector(look) + vector(current));
            }
        }
    }
}
