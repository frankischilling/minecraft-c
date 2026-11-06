import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/** Authored scalar caller of unchanged registered original Block methods. */
public final class TorchTextureObservation {
    static int network, natives, execs, writes;
    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static void origin(Class<?> type, Path jar) throws Exception {
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
            .toRealPath().equals(jar), "unchanged original origin");
    }
    static String hex(int value) {
        return String.format(Locale.ROOT, "%08x", value);
    }
    static void deny(Runnable call) {
        try { call.run(); throw new AssertionError("declared guard denial"); }
        catch (SecurityException expected) { }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        need(args.length == 2, "original jar and fixed profiles");
        Path jar = Path.of(args[0]).toRealPath();
        List<String> profiles = Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) { }
            public void checkConnect(String h, int p) { ++network; throw new SecurityException("network"); }
            public void checkListen(int p) { ++network; throw new SecurityException("listen"); }
            public void checkLink(String n) { ++natives; throw new SecurityException("native"); }
            public void checkExec(String n) { ++execs; throw new SecurityException("exec"); }
            public void checkWrite(String n) { ++writes; throw new SecurityException("write"); }
            public void checkDelete(String n) { ++writes; throw new SecurityException("delete"); }
        });
        for (Class<?> type : new Class<?>[] {uu.class, vm.class, db.class, sm.class}) origin(type, jar);
        uu[] slots = uu.m.clone();
        uu dust = uu.aw;
        need(dust != null && dust == slots[55] && dust.getClass() == sm.class && dust.bn == 55,
             "actual live final Dust singleton");
        need(uu.class.getField("aw").getModifiers() == 25, "actual final static field");
        need(vm.class.getSuperclass() == uu.class && db.class.getSuperclass() == vm.class,
             "actual metadata superclass resolution");
        int initialDust = dust.bm, initial75 = slots[75].bm, initial76 = slots[76].bm;
        int index = 0;
        for (String profile : profiles) {
            String[] cells = profile.split("\t");
            need(cells.length == 7, "frozen row width");
            int[] p = new int[6];
            for (int i = 0; i < p.length; ++i) p[i] = Integer.parseInt(cells[i + 1]);
            uu block = uu.m[p[0]];
            need((p[0] == 75 || p[0] == 76) && block != null && block.getClass() == db.class &&
                 block.bn == p[0] && (p[3] == 0 || p[3] == 1), "genuine DB receiver and setup");
            Class<?> selfOwner = block.getClass().getMethod("a", int.class, int.class).getDeclaringClass();
            Class<?> dustOwner = dust.getClass().getMethod("a", int.class, int.class).getDeclaringClass();
            need(selfOwner == db.class && dustOwner == sm.class, "actual metadata declaring classes");
            int savedSelf = block.bm, savedDust = dust.bm, enteredSelf, enteredDust, returned;
            try {
                if (p[3] == 1) { block.bm = p[4]; dust.bm = p[5]; }
                enteredSelf = block.bm; enteredDust = dust.bm;
                returned = block.a(p[1], p[2]);
                need(block.bm == enteredSelf && dust.bm == enteredDust && uu.aw == dust &&
                     Arrays.equals(slots, uu.m), "metadata call keeps instance fields and registered aliases");
            } finally {
                block.bm = savedSelf; dust.bm = savedDust;
            }
            need(block.bm == savedSelf && dust.bm == savedDust, "live instance bm restored");
            System.out.println("DB_TEXTURE\t" + index++ + "\t" + cells[0] + "\t" + p[0] +
                "\t" + p[1] + "\t" + p[2] + "\t" + p[3] + "\t" + p[4] + "\t" + p[5] +
                "\t" + savedSelf + "\t" + savedDust + "\t" + enteredSelf + "\t" + enteredDust +
                "\t" + returned + "\t" + hex(returned) + "\t" + selfOwner.getName() +
                "\t" + dustOwner.getName() + "\t" + dust.getClass().getName() + "\t55\t1\t1");
        }
        need(index == 160 && dust.bm == initialDust && slots[75].bm == initial75 &&
             slots[76].bm == initial76 && uu.aw == dust && Arrays.equals(slots, uu.m), "final actual aliases and bm");
        need(network == 0 && natives == 0 && execs == 0 && writes == 0, "source external-effect guard");
        System.out.println("DB_GUARD\tsource\tnetwork=0\tnative=0\texecs=0\twrites=0");
        deny(() -> System.getSecurityManager().checkConnect("127.0.0.1", 25565));
        deny(() -> System.getSecurityManager().checkListen(25565));
        deny(() -> System.getSecurityManager().checkLink("db-texture-denial"));
        deny(() -> System.getSecurityManager().checkExec("db-texture-denial"));
        deny(() -> System.getSecurityManager().checkWrite("db-texture-denial"));
        need(network == 2 && natives == 1 && execs == 1 && writes == 1, "guard denial controls");
        System.out.println("DB_GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\twrites=1");
        System.out.println("DB_COMPLETE\t160");
    }
}
