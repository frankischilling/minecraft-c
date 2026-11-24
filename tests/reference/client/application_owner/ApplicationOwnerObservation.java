import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** Authored CPU observer of unchanged Minecraft.<clinit> and prepared kg owners. */
public final class ApplicationOwnerObservation {
    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Field field(Class<?> owner, String name) throws Exception {
        Field result = owner.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    private static boolean zero(byte[] values) {
        byte combined = 0;
        for (byte value : values) combined |= value;
        return combined == 0;
    }

    private static boolean zero(long[] values) {
        long combined = 0;
        for (long value : values) combined |= value;
        return combined == 0;
    }

    private static int bit(boolean value) {
        return value ? 1 : 0;
    }

    private static void joined(Thread worker) throws Exception {
        worker.join(2000L);
        require(!worker.isAlive() && worker.getState() == Thread.State.TERMINATED,
                "original kg did not terminate and join");
    }

    private static void sleeping(Thread worker) throws Exception {
        long start = System.nanoTime();
        while (worker.getState() != Thread.State.TIMED_WAITING) {
            require(worker.isAlive(), "original kg stopped before sleep");
            require(System.nanoTime() - start < 2000000000L, "original kg sleep timeout");
            Thread.sleep(1L);
        }
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 0, "observer takes no game or launcher arguments");
        Class<?> minecraft = Class.forName("net.minecraft.client.Minecraft");
        byte[] b = (byte[])field(minecraft, "b").get(null);
        long[] e = (long[])field(minecraft, "E").get(null);
        long[] f = (long[])field(minecraft, "F").get(null);
        System.out.println("CLASS arrays " + b.length + " " + e.length + " " + f.length +
                " " + bit(zero(b)) + " " + bit(zero(e)) + " " + bit(zero(f)) + " " + bit(e != f));
        System.out.println("CLASS fields " + field(minecraft, "G").getInt(null) + " " +
                field(minecraft, "H").getLong(null) + " " +
                bit(field(minecraft, "a").get(null) == null) + " " +
                bit(field(minecraft, "af").get(null) == null));

        // The unchanged original concrete kq has inherited Minecraft JVM
        // defaults. Unsafe does not call either instance constructor;
        // this prepared kg profile invokes no kq virtual/frame methods.
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Field unsafeField = unsafeType.getDeclaredField("theUnsafe");
        unsafeField.setAccessible(true);
        Object unsafe = unsafeField.get(null);
        Method allocate = unsafeType.getMethod("allocateInstance", Class.class);
        Class<?> receiver = Class.forName("kq");
        require(receiver.getSuperclass() == minecraft &&
                !java.lang.reflect.Modifier.isAbstract(receiver.getModifiers()),
                "actual original concrete kq receiver");
        Object app = allocate.invoke(unsafe, receiver);
        Field running = field(minecraft, "J");
        Class<?> kg = Class.forName("kg");
        Constructor<?> constructor = kg.getDeclaredConstructor(minecraft, String.class);
        constructor.setAccessible(true);
        String name = "Timer hack thread";
        Thread worker = null;
        try {
            worker = (Thread)constructor.newInstance(app, name);
            joined(worker);
            System.out.println("THREAD zero " + bit(worker.isDaemon()) + " " +
                    bit(worker.getName() == name) + " " + bit(!worker.isAlive()) + " " +
                    bit(running.getBoolean(app)));
            running.setBoolean(app, true);
            worker = (Thread)constructor.newInstance(app, name);
            sleeping(worker);
            System.out.println("THREAD live " + bit(worker.isDaemon()) + " " +
                    bit(worker.getName() == name) + " " +
                    bit(worker.getState() == Thread.State.TIMED_WAITING) + " " +
                    bit(running.getBoolean(app)));
            running.setBoolean(app, false);
            worker.interrupt();
            joined(worker);
            System.out.println("THREAD source-stop " + bit(!worker.isAlive()) + " " +
                    bit(running.getBoolean(app)));
        } finally {
            running.setBoolean(app, false);
            if (worker != null) {
                worker.interrupt();
                joined(worker);
            }
        }
    }
}
