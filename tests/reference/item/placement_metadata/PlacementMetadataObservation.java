import java.nio.file.*;
import java.security.Permission;
import java.util.*;

/** Authored scalar caller; every Item and implementation comes from the supplied JAR. */
public final class PlacementMetadataObservation {
  static int network, natives, execs, writes;
  static void need(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }
  static void origin(Class<?> type, Path jar) throws Exception {
    need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
        .toRealPath().equals(jar), "original class origin");
  }
  @SuppressWarnings("removal")
  public static void main(String[] args) throws Exception {
    Locale.setDefault(Locale.ROOT);
    need(args.length == 2, "jar, operands");
    Path jar = Path.of(args[0]).toRealPath();
    List<String> inputs = Files.readAllLines(Path.of(args[1]));
    int[] operands = new int[inputs.size()];
    for (int i = 0; i < operands.length; ++i) operands[i] = Integer.parseInt(inputs.get(i));
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
    Class.forName("uu"); // Declared genuine Block-before-Item initialization.
    gm[] slots = gm.c.clone();
    Set<Class<?>> classes = new HashSet<>();
    int items = 0, rows = 0;
    for (int id = 0; id < gm.c.length; ++id) {
      gm item = gm.c[id];
      if (item == null) continue;
      origin(item.getClass(), jar);
      Class<?> owner = item.getClass().getMethod("b", int.class).getDeclaringClass();
      origin(owner, jar);
      classes.add(item.getClass());
      items++;
      System.out.println("CLASS\t" + id + "\t" + item.getClass().getName() + "\t" + owner.getName());
    }
    for (int id = 0; id < gm.c.length; ++id) {
      gm item = gm.c[id];
      if (item == null) continue;
      for (int operand : operands) {
        int result = item.b(operand); // Actual original virtual metadata dispatch.
        System.out.println("METADATA\t" + id + "\t" + operand + "\t" + result);
        rows++;
      }
    }
    need(Arrays.equals(slots, gm.c), "registration identities unchanged");
    need(network == 0 && natives == 0 && execs == 0 && writes == 0, "no external effects");
    System.out.println("COMPLETE\t" + items + "\t" + classes.size() + "\t" + rows);
    System.out.println("GUARD\tnetwork=" + network + "\tnative=" + natives + "\texecs=" + execs
        + "\twrites=" + writes + "\tslots-unchanged=1");
  }
}
