import java.util.Locale;

/* Calls only unchanged original Item/Stack/Inventory bodies. No World or
 * graphical owner is constructed; nullable Inventory Player is genuine input. */
public final class MiningObservation {
    interface Speed { float run(); }
    interface Harvest { boolean run(); }
    static String speed(Speed call) {
        try { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(call.run())); }
        catch (NullPointerException e) { return "NULL"; }
    }
    static String harvest(Harvest call) {
        try { return call.run() ? "1" : "0"; }
        catch (NullPointerException e) { return "NULL"; }
    }
    public static void main(String[] args) throws Exception {
        uu stone = uu.u;
        ix inventory = new ix(null);
        int items = 0, rows = 0;
        for (int id = 0; id < gm.c.length; ++id) {
            final gm item = gm.c[id];
            if (item == null) continue;
            ++items;
            final iz stack = new iz(id, -3, -7);
            inventory.a(0, stack);
            inventory.c = 0;
            System.out.println("CLASS\t" + id + "\t" + item.getClass().getName() + "\t" +
                item.getClass().getMethod("a", iz.class, uu.class).getDeclaringClass().getName() +
                "\t" + item.getClass().getMethod("a", uu.class).getDeclaringClass().getName());
            for (int blockId = -1; blockId < uu.m.length; ++blockId) {
                final uu block = blockId < 0 ? null : uu.m[blockId];
                if (blockId >= 0 && block == null) continue;
                System.out.println("MINING\t" + id + "\t" + blockId + "\t" +
                    speed(() -> item.a(stack, block)) + "\t" + harvest(() -> item.a(block)) + "\t" +
                    speed(() -> stack.a(block)) + "\t" + harvest(() -> stack.b(block)) + "\t" +
                    speed(() -> inventory.a(block)) + "\t" + harvest(() -> inventory.b(block)));
                ++rows;
            }
            if (stack.c != id || stack.a != -3 || stack.h() != -7)
                throw new AssertionError("mining mutated actual Stack");
        }
        System.out.println("TOTAL\t" + items + "\t" + rows);
    }
}
