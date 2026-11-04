import java.nio.file.*;
import java.security.Permission;
import java.util.*;

/** Authored scalar caller; all game receivers and methods are supplied original classes. */
public final class AppearanceObservation {
  static int network, natives, execs, writes;
  static void need(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }
  static int[] integers(String path) throws Exception {
    List<String> lines = Files.readAllLines(Path.of(path));
    int[] values = new int[lines.size()];
    for (int i = 0; i < values.length; ++i) values[i] = Integer.parseInt(lines.get(i));
    return values;
  }
  static void origin(Class<?> type, Path jar) throws Exception {
    need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
        .toRealPath().equals(jar), "unchanged original class origin");
  }
  static String owner(gm item, String name, Class<?>... parameters) throws Exception {
    return item.getClass().getMethod(name, parameters).getDeclaringClass().getName();
  }
  @SuppressWarnings("removal")
  public static void main(String[] args) throws Exception {
    Locale.setDefault(Locale.ROOT);
    need(args.length == 3, "jar, damage operands, bm operands");
    Path jar = Path.of(args[0]).toRealPath();
    int[] operands = integers(args[1]), bmOperands = integers(args[2]);
    System.setSecurityManager(new SecurityManager() {
      public void checkPermission(Permission permission) {}
      public void checkConnect(String host, int port) { network++; throw new SecurityException("network"); }
      public void checkListen(int port) { network++; throw new SecurityException("listen"); }
      public void checkLink(String name) { natives++; throw new SecurityException("native"); }
      public void checkExec(String name) { execs++; throw new SecurityException("exec"); }
      public void checkWrite(String name) { writes++; throw new SecurityException("write"); }
      public void checkDelete(String name) { writes++; throw new SecurityException("delete"); }
    });
    origin(gm.class, jar);
    origin(uu.class, jar);
    origin(iz.class, jar);
    Class.forName("uu"); // Declared genuine Block-before-Item initialization.
    gm[] slots = gm.c.clone();
    int[] initialBh = new int[gm.c.length];
    Set<Class<?>> classes = new HashSet<>();
    int items = 0, rows = 0, bmRows = 0;
    for (int id = 0; id < gm.c.length; ++id) {
      gm item = gm.c[id];
      if (item == null) continue;
      origin(item.getClass(), jar);
      classes.add(item.getClass());
      initialBh[id] = item.bh;
      items++;
      System.out.println("CLASS\t" + id + "\t" + item.getClass().getName() + "\t"
          + owner(item, "a", int.class) + "\t" + owner(item, "f", int.class) + "\t"
          + owner(item, "b") + "\t" + owner(item, "c") + "\t" + item.bh);
    }
    for (int id = 0; id < gm.c.length; ++id) {
      gm item = gm.c[id];
      if (item == null) continue;
      for (int damage : operands) {
        int direct = item.a(damage);
        int wrapped = new iz(id, 2, damage).b();
        int color = item.f(damage);
        boolean full3d = item.b(), rotate = item.c();
        System.out.println("APPEARANCE\t" + id + "\t" + damage + "\t" + direct + "\t" + wrapped
            + "\t" + color + "\t" + (full3d ? 1 : 0) + "\t" + (rotate ? 1 : 0));
        rows++;
      }
    }
    int initialSapling = uu.z.bm, initialLeaves = uu.L.bm;
    for (int id : new int[] {6, 18}) {
      gm item = gm.c[id];
      for (int bm : bmOperands) {
        for (int damage = 0; damage < 4; ++damage) {
          uu block = id == 6 ? uu.z : uu.L;
          int saved = block.bm, direct, wrapped;
          try {
            block.bm = bm;
            direct = item.a(damage);
            wrapped = new iz(id, 2, damage).b();
          } finally {
            block.bm = saved;
          }
          need(block.bm == saved, "actual bm restored before next row");
          System.out.println("BM\t" + id + "\t" + bm + "\t" + damage + "\t" + direct + "\t" + wrapped);
          bmRows++;
        }
      }
    }
    need(uu.z.bm == initialSapling && uu.L.bm == initialLeaves, "final actual bm restored");
    need(Arrays.equals(slots, gm.c), "registration identities unchanged");
    for (int id = 0; id < gm.c.length; ++id)
      if (gm.c[id] != null) need(gm.c[id].bh == initialBh[id], "actual Item bh unchanged");
    need(network == 0 && natives == 0 && execs == 0 && writes == 0, "no external effects");
    System.out.println("COMPLETE\t" + items + "\t" + classes.size() + "\t" + rows + "\t" + bmRows);
    System.out.println("GUARD\tnetwork=" + network + "\tnative=" + natives + "\texecs=" + execs
        + "\twrites=" + writes + "\tslots-unchanged=1\tbh-unchanged=1\tbm-restored=1");
  }
}
