import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored finite inputs and passive observations; every game body is unchanged. */
public final class LeverPlacementObservation {
  static int network, natives, execs, deniedWrites, worldWrites;
  static boolean mathSeen;
  static lm[] chunks;
  static Field field(Class<?> owner, String name) throws Exception {
    Field result = owner.getDeclaredField(name); result.setAccessible(true); return result;
  }
  static void need(boolean condition, String text) { if (!condition) throw new AssertionError(text); }
  static Map<String,Object> map(Object... pairs) {
    Map<String,Object> result = new LinkedHashMap<>();
    for (int i = 0; i < pairs.length; i += 2) result.put((String)pairs[i], pairs[i + 1]);
    return result;
  }
  static String json(Object value) {
    if (value == null) return "null";
    if (value instanceof String) return "\"" + ((String)value).replace("\\", "\\\\")
        .replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
    if (value instanceof Map) {
      StringJoiner result = new StringJoiner(",", "{", "}");
      for (Map.Entry<?,?> entry : ((Map<?,?>)value).entrySet()) result.add(json(entry.getKey()) + ":" + json(entry.getValue()));
      return result.toString();
    }
    if (value instanceof Iterable) {
      StringJoiner result = new StringJoiner(",", "[", "]");
      for (Object entry : (Iterable<?>)value) result.add(json(entry));
      return result.toString();
    }
    return value.toString();
  }
  static String d(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
  static String f(float value) { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value)); }
  static Map<String,Object> random(Random random) throws Exception {
    return map("seed", String.format(Locale.ROOT, "%012x", ((AtomicLong)field(Random.class, "seed").get(random)).get()),
        "gaussian", d(field(Random.class, "nextNextGaussian").getDouble(random)),
        "have", field(Random.class, "haveNextNextGaussian").getBoolean(random));
  }
  static Map<String,Object> mathRandom() throws Exception {
    // Called only after an observed original HL constructor has used Math.random.
    Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
    return random((Random)field(holder, "randomNumberGenerator").get(null));
  }
  static void origin(Class<?> type, Path jar) throws Exception {
    need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "original class origin");
  }
  static byte[][] arrays() {
    byte[][] result = new byte[chunks.length * 5][];
    for (int i = 0; i < chunks.length; ++i) {
      result[i * 5] = chunks[i].b; result[i * 5 + 1] = chunks[i].e.a;
      result[i * 5 + 2] = chunks[i].f.a; result[i * 5 + 3] = chunks[i].g.a; result[i * 5 + 4] = chunks[i].h;
    }
    return result;
  }
  static void put(int x, int y, int z, int id, int metadata) {
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127) return;
    lm chunk = chunks[(x >> 4) * 2 + (z >> 4)];
    int index = ((x & 15) << 11) | ((z & 15) << 7) | y;
    chunk.b[index] = (byte)id;
    int shift = (index & 1) * 4;
    chunk.e.a[index >> 1] = (byte)((chunk.e.a[index >> 1] & ~(15 << shift)) | ((metadata & 15) << shift));
  }
  static List<Integer> cell(int x, int y, int z) {
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127) return List.of(0, 0);
    lm chunk = chunks[(x >> 4) * 2 + (z >> 4)]; int index = ((x & 15) << 11) | ((z & 15) << 7) | y;
    return List.of(chunk.b[index] & 255, (chunk.e.a[index >> 1] >>> ((index & 1) * 4)) & 15);
  }
  static void prepare(int[] p) {
    for (lm chunk : chunks) {
      Arrays.fill(chunk.b, (byte)0); Arrays.fill(chunk.e.a, (byte)0); Arrays.fill(chunk.f.a, (byte)15);
      Arrays.fill(chunk.g.a, (byte)0); Arrays.fill(chunk.h, (byte)0); chunk.i = 0; chunk.n = true;
    }
    int x = p[1], y = p[2], z = p[3], mask = p[7];
    if ((mask & 1) != 0) put(x - 1, y, z, 1, 0);
    if ((mask & 2) != 0) put(x + 1, y, z, 1, 0);
    if ((mask & 4) != 0) put(x, y, z - 1, 1, 0);
    if ((mask & 8) != 0) put(x, y, z + 1, 1, 0);
    put(x, y - 1, z, p[8], 0); put(x, y, z, p[9], p[4]);
  }
  static List<Object> queues(fd world) throws Exception {
    List<Object> result = new ArrayList<>();
    for (Object value : (List<?>)field(fd.class, "C").get(world)) {
      st region = (st)value;
      need(region.a == eb.a || region.a == eb.b, "actual light kind");
      result.add(List.of(region.a == eb.a ? 0 : 1, region.b, region.c, region.d, region.e, region.f, region.g));
    }
    return result;
  }
  static final class Observer implements pm {
    final List<Object> events = new ArrayList<>(), drops = new ArrayList<>(), entropy = new ArrayList<>();
    void clear() { events.clear(); drops.clear(); entropy.clear(); }
    public void a(int x, int y, int z) { List<Integer> c = cell(x, y, z); events.add(List.of("cell", x, y, z, c.get(0), c.get(1))); }
    public void b(int x0, int y0, int z0, int x1, int y1, int z1) { events.add(List.of("range", x0, y0, z0, x1, y1, z1)); }
    public void a(sn entity) {
      try {
        need(entity instanceof hl, "actual published Item entity"); hl item = (hl)entity; int ordinal = drops.size();
        events.add(List.of("add", ordinal, item.getClass().getName()));
        drops.add(List.of(ordinal, item.getClass().getName(), item.a.c, item.a.a, item.a.i(), item.c, item.b, field(hl.class, "f").getInt(item)));
        mathSeen = true;
        entropy.add(map("ordinal", ordinal, "position", List.of(d(item.aM), d(item.aN), d(item.aO)),
            "motion", List.of(d(item.aP), d(item.aQ), d(item.aR)), "yaw", f(item.aS), "bob", f(item.d),
            "entity_random", random((Random)field(sn.class, "bs").get(item)), "math_post", mathRandom()));
      } catch (Exception failure) { throw new AssertionError(failure); }
    }
    public void b(sn entity) { events.add(List.of("remove", entity.getClass().getName())); }
    public void e() { events.add(List.of("refresh")); }
    public void a(String name, double x, double y, double z, float volume, float pitch) {
      events.add(List.of("sound", name, d(x), d(y), d(z), f(volume), f(pitch)));
    }
    public void a(String name, double x, double y, double z, double vx, double vy, double vz) {
      events.add(List.of("particle", name, d(x), d(y), d(z), d(vx), d(vy), d(vz)));
    }
    public void a(String name, int x, int y, int z) { events.add(List.of("record", name, x, y, z)); }
    public void a(int x, int y, int z, ow tile) { events.add(List.of("tile", x, y, z, tile == null ? "NULL" : tile.getClass().getName())); }
    public void a(gs player, int a, int b, int c, int d, int e) { events.add(List.of("level", a, b, c, d, e)); }
  }
  @SuppressWarnings("removal")
  public static void main(String[] args) throws Exception {
    Locale.setDefault(Locale.ROOT); need(args.length == 3, "jar, privateworld, profiles");
    Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
    List<String> profiles = Files.readAllLines(Path.of(args[2]));
    System.setSecurityManager(new SecurityManager() {
      public void checkPermission(Permission permission) {}
      public void checkConnect(String h, int p) { network++; throw new SecurityException("network"); }
      public void checkListen(int p) { network++; throw new SecurityException("listen"); }
      public void checkLink(String n) { natives++; throw new SecurityException("native"); }
      public void checkExec(String n) { execs++; throw new SecurityException("exec"); }
      public void checkWrite(String n) {
        if (Path.of(n).toAbsolutePath().normalize().startsWith(directory)) { worldWrites++; return; }
        deniedWrites++; throw new SecurityException("outside privateworld");
      }
      public void checkDelete(String n) { checkWrite(n); }
    });
    for (Class<?> type : new Class<?>[] {fd.class, lm.class, mx.class, uu.class, vm.class, hl.class, sn.class, iz.class, xr.class, st.class, eb.class, pm.class}) origin(type, jar);
    fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
    chunks = new lm[] {world.c(0, 0), world.c(0, 1), world.c(1, 0), world.c(1, 1)};
    uu block = uu.m[69]; need(block != null && block.getClass() == xr.class && block.bn == 69, "actual Lever");
    Method validate = xr.class.getDeclaredMethod("h", fd.class, int.class, int.class, int.class);
    validate.setAccessible(true);
    Observer observer = new Observer(); world.a(observer); int index = 0;
    for (String profile : profiles) {
      String[] columns = profile.split("\t"); need(columns.length == 13, "name and twelve signed inputs");
      int[] p = new int[12]; List<Integer> values = new ArrayList<>();
      for (int i = 0; i < p.length; ++i) { p[i] = Integer.parseInt(columns[i + 1]); values.add(p[i]); }
      world.B = p[11] != 0; prepare(p); observer.clear(); List<Integer> centerBefore = cell(p[1], p[2], p[3]);
      byte[][] current = arrays(), before = new byte[current.length][];
      for (int i = 0; i < current.length; ++i) before[i] = current[i].clone();
      List<Object> queueBefore = queues(world); Map<String,Object> worldBefore = random(world.r), mathBefore = mathSeen ? mathRandom() : null;
      fd actual = p[10] == 0 ? world : null; String result = "UNRETURNED", exception = "NONE";
      try {
        switch (p[0]) {
          case 0: result = Boolean.toString(block.a(actual, p[1], p[2], p[3])); break;
          case 1: result = validate.invoke(block, actual, p[1], p[2], p[3]).toString(); break;
          case 2: result = Boolean.toString(block.a(actual, p[1], p[2], p[3], p[5])); break;
          case 3: result = Boolean.toString(actual.a(69, p[1], p[2], p[3], false, p[5])); break;
          case 4: block.e(actual, p[1], p[2], p[3], p[5]); result = "VOID"; break;
          case 5: result = Boolean.toString(block.a(actual, p[1], p[2], p[3], (gs)null)); break;
          case 6: block.b(actual, p[1], p[2], p[3], (gs)null); result = "VOID"; break;
          case 7: block.b(actual, p[1], p[2], p[3], p[6]); result = "VOID"; break;
          case 8: block.b(actual, p[1], p[2], p[3]); result = "VOID"; break;
          default: throw new AssertionError("declared entry");
        }
      } catch (Throwable failure) {
        Throwable reached = failure instanceof InvocationTargetException ? failure.getCause() : failure;
        exception = reached.getClass().getName();
      }
      List<Object> changes = new ArrayList<>(); byte[][] after = arrays();
      for (int i = 0; i < before.length; ++i) {
        need(before[i].length == after[i].length, "actual array lengths");
        for (int j = 0; j < before[i].length; ++j) if (before[i][j] != after[i][j])
          changes.add(List.of(i / 5, i % 5, j, before[i][j] & 255, after[i][j] & 255));
      }
      Map<String,Object> worldAfter = random(world.r), mathAfter = mathSeen ? mathRandom() : null;
      System.out.println("LEVER\t" + json(map("index", index, "name", columns[0], "values", values,
          "result", result, "exception", exception, "center_before", centerBefore, "center_after", cell(p[1], p[2], p[3]),
          "changes", changes, "queues_before", queueBefore, "queues_after", queues(world), "events", observer.events,
          "drops", observer.drops, "world_rng_unchanged", worldBefore.equals(worldAfter))));
      System.out.println("ENTROPY\t" + json(map("index", index, "name", columns[0], "world_before", worldBefore,
          "world_after", worldAfter, "math_before", mathBefore, "math_after", mathAfter, "items", observer.entropy)));
      index++;
    }
    for (int i = 0; i < chunks.length; ++i) need(world.c(i / 2, i % 2) == chunks[i], "canonical original Chunks");
    need(network == 0 && natives == 0 && execs == 0 && deniedWrites == 0, "no external attempts");
    System.out.println("COMPLETE\t" + index);
    world.B = false;
    System.out.println("LEVER_GUARD\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes=" + worldWrites
        + "\tcanonical-chunks=4\tgame-code-replacements=0\tremote=" + (world.B ? 1 : 0));
  }
}
