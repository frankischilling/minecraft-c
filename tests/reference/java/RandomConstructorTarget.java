import java.util.Random;

/** Actual API calls only; the separate observer reads the suspended boundaries. */
public final class RandomConstructorTarget {
    static Random A;
    static Random E;
    static Random B;
    static double lastGaussian;
    static double lastMath;

    static void before(int stage) {}
    static void after(int stage) {}

    public static void main(String[] args) throws ClassNotFoundException {
        if (args.length != 0) throw new IllegalArgumentException("no target arguments");

        before(0);
        Class.forName("java.util.Random", true, null);
        after(0);

        before(1);
        A = new Random();
        after(1);

        before(2);
        E = new Random(0L);
        after(2);

        before(3);
        lastGaussian = E.nextGaussian();
        after(3);

        before(4);
        E.setSeed(-1L);
        after(4);

        before(5);
        lastMath = Math.random();
        after(5);

        before(6);
        lastMath = Math.random();
        after(6);

        before(7);
        B = new Random();
        after(7);

        before(8);
        lastMath = Math.random();
        after(8);

        before(9);
        after(9);
    }
}
