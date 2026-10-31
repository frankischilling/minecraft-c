import java.util.Locale;

/** JDK math vectors for the bytecode-derived h(F) render-distance expression. */
public final class FogBlendObservation {
    public static void main(String[] arguments) {
        Locale.setDefault(Locale.ROOT);
        int[] distances = {Integer.MIN_VALUE, 0, 1, 2, 3, 4, 5, Integer.MAX_VALUE};
        for (int distance : distances) {
            float reciprocal = 1.0f / (float)(4 - distance);
            float blend = 1.0f - (float)Math.pow((double)reciprocal, 0.25);
            if (Float.isNaN(blend)) System.out.printf("BLEND %d NAN%n", distance);
            else System.out.printf("BLEND %d %08x%n", distance, Float.floatToRawIntBits(blend));
        }
    }
}
