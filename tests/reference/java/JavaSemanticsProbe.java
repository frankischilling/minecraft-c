import java.util.Locale;
import java.util.Random;

/** Local oracle: execute the platform's real Java operators and java.util.Random. */
public final class JavaSemanticsProbe {
    private static final int[] INTS = {
        0, 1, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, -7, 7,
        65535, 65536, 0x55555555, 0xaaaaaaaa
    };
    private static final long[] LONGS = {
        0, 1, -1, Long.MIN_VALUE, Long.MAX_VALUE, -7, 7,
        0x123456789abcdefL, -0x123456789abcdefL, 0x100000000L, 0xffffffffL
    };
    private static final int[] SHIFTS = {
        -65, -64, -33, -32, -31, -1, 0, 1, 7, 31, 32, 33, 63, 64, 65,
        Integer.MIN_VALUE, Integer.MAX_VALUE
    };
    private static final long[] SEEDS = {
        0, 1, -1, Long.MIN_VALUE, Long.MAX_VALUE, 0x123456789abcdefL, -123456789
    };
    private static final int[] BOUNDS = {1, 2, 3, 16, 17, 1073741825, Integer.MAX_VALUE};
    private static final long[] DOUBLE_BITS = {
        0, 0x8000000000000000L, 0x7ff0000000000000L, 0xfff0000000000000L,
        0x7ff8000000000000L, 0xfff8000000000001L,
        0x3fffffffffffffffL, 0xbfffffffffffffffL,
        0x41dfffffffffffffL, 0x41e0000000000000L, 0xc1e0000000000000L,
        0xc1e0000000000001L, 0x43dfffffffffffffL, 0x43e0000000000000L,
        0xc3e0000000000000L, 0xc3e0000000000001L, 1, 0x8000000000000001L
    };
    private static final int[] FLOAT_BITS = {
        0, 0x80000000, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00001,
        0x3fffffff, 0xbfffffff, 0x4effffff, 0x4f000000, 0xcf000000, 0xcf000001,
        0x5effffff, 0x5f000000, 0xdf000000, 0xdf000001, 1, 0x80000001
    };

    private static void integers() {
        for (int a : INTS) {
            for (int b : INTS) {
                System.out.printf("i32 %d %d %d %d %d", a, b, a + b, a - b, a * b);
                if (b == 0) {
                    boolean divisionThrows = false;
                    boolean remainderThrows = false;
                    try { int ignored = a / b; } catch (ArithmeticException ex) { divisionThrows = true; }
                    try { int ignored = a % b; } catch (ArithmeticException ex) { remainderThrows = true; }
                    if (!divisionThrows || !remainderThrows) throw new AssertionError("zero divisor");
                    System.out.println(" zero");
                } else {
                    System.out.printf(" %d %d%n", a / b, a % b);
                }
            }
            for (int shift : SHIFTS) {
                System.out.printf("s32 %d %d %d %d %d %d%n", a, shift, a << shift, a >> shift, a >>> shift, -a);
            }
        }
        for (long a : LONGS) {
            for (long b : LONGS) {
                System.out.printf("i64 %d %d %d %d %d", a, b, a + b, a - b, a * b);
                if (b == 0) {
                    boolean divisionThrows = false;
                    boolean remainderThrows = false;
                    try { long ignored = a / b; } catch (ArithmeticException ex) { divisionThrows = true; }
                    try { long ignored = a % b; } catch (ArithmeticException ex) { remainderThrows = true; }
                    if (!divisionThrows || !remainderThrows) throw new AssertionError("zero divisor");
                    System.out.println(" zero");
                } else {
                    System.out.printf(" %d %d%n", a / b, a % b);
                }
            }
            for (int shift : SHIFTS) {
                System.out.printf("s64 %d %d %d %d %d %d%n", a, shift, a << shift, a >> shift, a >>> shift, -a);
            }
            System.out.printf("narrow %d %d %d %d %d%n", a, (byte)a, (short)a, (int)(char)a, (int)a);
        }
        // Exhaust every byte/short/char result and one complete wrapping period.
        for (int a = -65536; a <= 65536; a++) {
            System.out.printf("n16 %d %d %d %d%n", a, (byte)a, (short)a, (int)(char)a);
        }
    }

    private static void casts() {
        for (long bits : DOUBLE_BITS) {
            double value = Double.longBitsToDouble(bits);
            System.out.printf("dcast %016x %d %d%n", bits, (int)value, (long)value);
        }
        for (int bits : FLOAT_BITS) {
            float value = Float.intBitsToFloat(bits);
            System.out.printf("fcast %08x %d %d%n", bits, (int)value, (long)value);
        }
    }

    private static void random() {
        for (long seed : SEEDS) {
            Random integers = new Random(seed);
            Random longs = new Random(seed);
            Random floats = new Random(seed);
            Random doubles = new Random(seed);
            for (int i = 0; i < 64; i++) {
                System.out.printf("random %d %d %d %d %08x %016x%n", seed, i,
                    integers.nextInt(), longs.nextLong(),
                    Float.floatToRawIntBits(floats.nextFloat()),
                    Double.doubleToRawLongBits(doubles.nextDouble()));
            }
            for (int bound : BOUNDS) {
                Random bounded = new Random(seed);
                for (int i = 0; i < 64; i++) {
                    System.out.printf("bound %d %d %d %d%n", seed, bound, i, bounded.nextInt(bound));
                }
            }
            Random mixed = new Random(seed);
            for (int i = 0; i < 64; i++) {
                int a = mixed.nextInt();
                int b = mixed.nextInt(1073741825);
                long c = mixed.nextLong();
                int d = Float.floatToRawIntBits(mixed.nextFloat());
                long e = Double.doubleToRawLongBits(mixed.nextDouble());
                System.out.printf("mixed %d %d %d %d %d %08x %016x%n", seed, i, a, b, c, d, e);
            }
        }
    }

    public static void main(String[] args) {
        Locale.setDefault(Locale.ROOT);
        integers();
        casts();
        random();
    }
}
