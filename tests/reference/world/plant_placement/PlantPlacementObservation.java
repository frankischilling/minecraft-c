import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored inputs and passive observers around unchanged genuine World/Block methods. */
public final class PlantPlacementObservation {
  static int network, natives, execs, deniedWrites, worldWrites;
  static final int[] IDS = {6, 31, 32, 37, 38, 39, 40, 59};
  static boolean worldSame = true, chunkSame = true, rngSame = true;
  static void need(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }
  static Field field(Class<?> type, String name) throws Exception {
    Field result = type.getDeclaredField(name);
    result.setAccessible(true);
    return result;
  }
  static void origin(Class<?> type, Path jar) throws Exception {
    need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
        .toRealPath().equals(jar), "unchanged original class origin");
  }
  static String substrateOwner(Class<?> type) throws Exception {
    for (Class<?> current = type; current != null; current = current.getSuperclass()) {
      try { return current.getDeclaredMethod("d", int.class).getDeclaringClass().getName(); }
      catch (NoSuchMethodException absent) {}
    }
    throw new AssertionError("actual substrate declaration");
  }
  static Object primitive(Field field, Object owner) throws Exception {
    if (field.getType() == float.class) return Float.floatToRawIntBits(field.getFloat(owner));
    if (field.getType() == double.class) return Double.doubleToRawLongBits(field.getDouble(owner));
    return field.get(owner);
  }
  static final class Snapshot {
    final Object owner;
    final Field[] fields;
    final Object[] values;
    Snapshot(Object owner) throws Exception {
      this.owner = owner;
      fields = Arrays.stream(owner.getClass().getDeclaredFields())
          .filter(f -> !Modifier.isStatic(f.getModifiers()))
          .sorted(Comparator.comparing(Field::getName)).toArray(Field[]::new);
      values = new Object[fields.length];
      for (int i = 0; i < fields.length; ++i) {
        fields[i].setAccessible(true);
        values[i] = primitive(fields[i], owner);
      }
    }
    boolean same() throws Exception {
      boolean same = true;
      for (int i = 0; i < fields.length; ++i) {
        Object current = primitive(fields[i], owner);
        same &= fields[i].getType().isPrimitive() ? values[i].equals(current) : values[i] == current;
      }
      return same;
    }
  }
  static long[] random(Random random) throws Exception {
    return new long[] {((AtomicLong)field(Random.class, "seed").get(random)).get(),
        field(Random.class, "haveNextNextGaussian").getBoolean(random) ? 1 : 0,
        Double.doubleToRawLongBits(field(Random.class, "nextNextGaussian").getDouble(random))};
  }
  static byte[][] arrays(lm[] chunks) {
    byte[][] result = new byte[chunks.length * 5][];
    for (int i = 0; i < chunks.length; ++i) {
      result[i * 5] = chunks[i].b;
      result[i * 5 + 1] = chunks[i].e.a;
      result[i * 5 + 2] = chunks[i].f.a;
      result[i * 5 + 3] = chunks[i].g.a;
      result[i * 5 + 4] = chunks[i].h;
    }
    return result;
  }
  static void put(lm[] chunks, int x, int y, int z, int id) {
    if (y < 0 || y > 127) return;
    lm chunk = chunks[(x >> 4) * 2 + (z >> 4)];
    chunk.b[((x & 15) << 11) | ((z & 15) << 7) | y] = (byte)id;
  }
  static void prepare(lm[] chunks, int[] p) {
    for (lm chunk : chunks) {
      Arrays.fill(chunk.b, (byte)0);
      Arrays.fill(chunk.e.a, (byte)0);
      Arrays.fill(chunk.f.a, (byte)15);
      Arrays.fill(chunk.g.a, (byte)0);
      Arrays.fill(chunk.h, (byte)0);
      chunk.i = 0;
      chunk.n = true;
    }
    put(chunks, p[1], p[2] - 1, p[3], p[6]);
    put(chunks, p[1], p[2], p[3], p[5]);
  }
  @SuppressWarnings("removal")
  public static void main(String[] args) throws Exception {
    Locale.setDefault(Locale.ROOT);
    need(args.length == 3, "jar, private world, profiles");
    Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
    List<String> profiles = Files.readAllLines(Path.of(args[2]));
    System.setSecurityManager(new SecurityManager() {
      public void checkPermission(Permission permission) {}
      public void checkConnect(String host, int port) { network++; throw new SecurityException("network"); }
      public void checkListen(int port) { network++; throw new SecurityException("listen"); }
      public void checkLink(String name) { natives++; throw new SecurityException("native"); }
      public void checkExec(String name) { execs++; throw new SecurityException("exec"); }
      public void checkWrite(String name) {
        if (Path.of(name).toAbsolutePath().normalize().startsWith(directory)) { worldWrites++; return; }
        deniedWrites++;
        throw new SecurityException("outside private World");
      }
      public void checkDelete(String name) { checkWrite(name); }
    });
    for (Class<?> type : new Class<?>[] {fd.class, lm.class, mx.class, uu.class, wb.class,
                                        he.class, ru.class, jb.class, tl.class, ni.class, ln.class})
      origin(type, jar);
    fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
    lm[] chunks = {world.c(0, 0), world.c(0, 1), world.c(1, 0), world.c(1, 1)};
    uu[] slots = uu.m.clone();
    for (int id : IDS) {
      uu block = uu.m[id];
      need(block != null && block.bn == id, "genuine registered plant");
      origin(block.getClass(), jar);
      System.out.println("CLASS\t" + id + "\t" + block.getClass().getName() + "\t"
          + block.getClass().getMethod("a", fd.class, int.class, int.class, int.class, int.class)
              .getDeclaringClass().getName() + "\t"
          + block.getClass().getMethod("a", fd.class, int.class, int.class, int.class)
              .getDeclaringClass().getName() + "\t" + substrateOwner(block.getClass()));
    }
    int index = 0, calls = 0;
    for (String profile : profiles) {
      String[] columns = profile.split("\t");
      need(columns.length == 9, "name and eight signed inputs");
      int[] p = new int[8];
      for (int i = 0; i < p.length; ++i) p[i] = Integer.parseInt(columns[i + 1]);
      uu block = uu.m[p[0]];
      for (int channel = 0; channel < 3; ++channel) {
        prepare(chunks, p);
        byte[][] current = arrays(chunks), before = new byte[current.length][];
        for (int i = 0; i < current.length; ++i) before[i] = current[i].clone();
        Snapshot worldBefore = new Snapshot(world);
        Snapshot[] chunkBefore = new Snapshot[chunks.length];
        for (int i = 0; i < chunks.length; ++i) chunkBefore[i] = new Snapshot(chunks[i]);
        long[] randomBefore = random(world.r);
        String result = "UNRETURNED", exception = "NONE";
        fd actual = p[7] == 0 ? world : null;
        try {
          boolean returned = channel == 0 ? block.a(actual, p[1], p[2], p[3])
              : channel == 1 ? block.a(actual, p[1], p[2], p[3], p[4])
              : actual.a(p[0], p[1], p[2], p[3], false, p[4]);
          result = Boolean.toString(returned);
        } catch (NullPointerException reached) {
          exception = reached.getClass().getName();
        }
        int changed = 0;
        byte[][] after = arrays(chunks);
        for (int i = 0; i < before.length; ++i) {
          need(before[i].length == after[i].length, "actual array lengths unchanged");
          for (int j = 0; j < before[i].length; ++j) if (before[i][j] != after[i][j]) changed++;
        }
        worldSame &= worldBefore.same();
        for (Snapshot snapshot : chunkBefore) chunkSame &= snapshot.same();
        rngSame &= Arrays.equals(randomBefore, random(world.r));
        System.out.println("PLANT\t" + index + "\t" + columns[0] + "\t" + channel + "\t" + p[0]
            + "\t" + p[1] + "\t" + p[2] + "\t" + p[3] + "\t" + p[4] + "\t" + p[5]
            + "\t" + p[6] + "\t" + result + "\t" + exception + "\t" + changed);
        calls++;
      }
      index++;
    }
    need(Arrays.equals(slots, uu.m), "original registry identities unchanged");
    for (int i = 0; i < chunks.length; ++i)
      need(world.c(i / 2, i % 2) == chunks[i], "original canonical Chunks unchanged");
    need(network == 0 && natives == 0 && execs == 0 && deniedWrites == 0, "no external attempts");
    System.out.println("COMPLETE\t8\t" + index + "\t" + calls);
    System.out.println("PLANT_GUARD\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes="
        + worldWrites + "\tworld-fields-unchanged=" + (worldSame ? 1 : 0)
        + "\tchunk-fields-unchanged=" + (chunkSame ? 1 : 0) + "\trng-unchanged=" + (rngSame ? 1 : 0)
        + "\tregistry-unchanged=1\tcanonical-chunks=4\tremote=" + (world.B ? 1 : 0));
  }
}
