import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.client.Minecraft;

/** Authored finite inputs and passive observations; every game body is unchanged. */
public final class RedstoneUseObservation {
  static int network, natives, execs, deniedWrites, worldWrites;
  static int active = -1;
  static fd observedWorld;
  static iz observedStack;
  static final List<Object> held = new ArrayList<>();
  static final int MAX_HELD = 4096;
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
  static int id(Object object) {
    if (object == null) return 0;
    for (int i = 0; i < held.size(); ++i) if (held.get(i) == object) return i + 1;
    need(held.size() < MAX_HELD, "bounded passive identity inventory");
    held.add(object); return held.size();
  }
  static List<Integer> stack(iz value) throws Exception {
    return value == null ? null : List.of(value.a, value.b, value.c, field(iz.class, "d").getInt(value));
  }
  static Map<String,Object> setState(sm block) throws Exception {
    Object set = field(sm.class, "b").get(block);
    need(set != null && set.getClass() == HashSet.class, "actual SM HashSet");
    Object map = field(HashSet.class, "map").get(set);
    need(map != null && map.getClass() == HashMap.class, "actual SM HashMap");
    Object[] table = (Object[])field(HashMap.class, "table").get(map);
    need(table == null || table.length <= 1024, "bounded actual Set table");
    Class<?> nodeClass = Class.forName("java.util.HashMap$Node", false, null);
    Field next = field(nodeClass, "next"), key = field(nodeClass, "key"), hash = field(nodeClass, "hash");
    List<Object> nodes = new ArrayList<>(), visited = new ArrayList<>(), pending = new ArrayList<>();
    List<Integer> roots = new ArrayList<>();
    if (table != null) for (int bucket = 0; bucket < table.length; ++bucket) {
      roots.add(id(table[bucket]));
      for (Object node = table[bucket]; node != null; node = next.get(node)) {
        need(visited.size() < 256, "bounded complete Set node inventory");
        for (Object earlier : visited) need(node != earlier, "acyclic actual bucket graph");
        visited.add(node); Object position = key.get(node);
        need(position != null && position.getClass() == wf.class, "actual original WF key");
        List<Integer> xyz = List.of(field(wf.class, "a").getInt(position),
            field(wf.class, "b").getInt(position), field(wf.class, "c").getInt(position));
        boolean tree = node.getClass().getName().equals("java.util.HashMap$TreeNode");
        need(tree || node.getClass() == nodeClass, "actual JDK node kind");
        Map<String,Object> row = map("id", id(node), "bucket", bucket, "hash", hash.getInt(node),
            "key_id", id(position), "xyz", xyz, "next", id(next.get(node)), "tree", tree);
        if (tree) for (String name : new String[]{"parent", "left", "right", "prev"})
          row.put(name, id(field(node.getClass(), name).get(node)));
        if (tree) row.put("red", field(node.getClass(), "red").getBoolean(node));
        nodes.add(row); pending.add(xyz);
      }
    }
    int size = field(HashMap.class, "size").getInt(map);
    need(size == nodes.size(), "complete actual Set inventory");
    return map("set_id", id(set), "map_id", id(map), "table_id", id(table), "size", size,
        "capacity", table == null ? 0 : table.length, "threshold", field(HashMap.class, "threshold").getInt(map),
        "mod_count", field(HashMap.class, "modCount").getInt(map),
        "load_factor_bits", f(field(HashMap.class, "loadFactor").getFloat(map)),
        "output_flag", field(sm.class, "a").getBoolean(block), "roots", roots,
        "nodes", nodes, "pending", pending);
  }
  static void prepare(boolean use, int[] p) {
    for (lm chunk : chunks) {
      Arrays.fill(chunk.b, (byte)0); Arrays.fill(chunk.e.a, (byte)0);
      Arrays.fill(chunk.f.a, (byte)15); Arrays.fill(chunk.g.a, (byte)0);
      Arrays.fill(chunk.h, (byte)0); chunk.i = 0; chunk.n = true;
    }
    if (use) {
      put(p[7], p[8] - 1, p[9], p[11], 0);
      put(p[4], p[5], p[6], p[10], 0);
      put(p[7], p[8], p[9], p[12], 0);
    } else {
      put(p[0], p[1] - 1, p[2], p[4], 0);
      put(p[0], p[1], p[2], p[3], 0);
    }
  }
  static List<Object> queues(fd world) throws Exception {
    List<Object> result = new ArrayList<>();
    for (Object value : (List<?>)field(fd.class, "C").get(world)) {
      st region = (st)value;
      need(region.a == eb.a || region.a == eb.b, "actual light kind");
      result.add(List.of(region.a == eb.a ? 0 : 1, region.b, region.c, region.d,
          region.e, region.f, region.g));
    }
    return result;
  }
  static List<Object> changes(byte[][] before) {
    byte[][] after = arrays(); List<Object> result = new ArrayList<>();
    for (int i = 0; i < before.length; ++i) {
      need(before[i].length == after[i].length, "same actual array lengths");
      for (int j = 0; j < before[i].length; ++j) if (before[i][j] != after[i][j])
        result.add(List.of(i / 5, i % 5, j, before[i][j] & 255, after[i][j] & 255));
    }
    return result;
  }
  static final class Observer implements pm {
    final List<Object> events = new ArrayList<>();
    int mode, cells;
    void reset(int nextMode) { events.clear(); cells = 0; mode = nextMode; }
    public void a(int x, int y, int z) {
      try {
        ++cells; events.add(map("event", "cell", "xyz", List.of(x,y,z), "cell", cell(x,y,z),
            "stack", stack(observedStack)));
        if (cells == 1 && mode == 1) throw new IllegalStateException("declared Redstone cell refusal");
        if (cells == 1 && mode == 2) { observedStack.a = 99; events.add(map("event", "count-store", "value", 99)); }
        if (cells == 1 && mode == 3) { observedStack.c = 332; events.add(map("event", "id-store", "value", 332)); }
      } catch (ReflectiveOperationException failure) { throw new AssertionError(failure); }
      catch (Exception failure) {
        if (failure instanceof IllegalStateException) throw (IllegalStateException)failure;
        throw new AssertionError(failure);
      }
    }
    public void b(int a,int b,int c,int d,int e,int f) { events.add(List.of("range",a,b,c,d,e,f)); }
    public void a(String n,double x,double y,double z,float v,float p) { events.add(List.of("sound",n,d(x),d(y),d(z),f(v),f(p))); }
    public void a(String n,double x,double y,double z,double a,double b,double c) { events.add(List.of("particle",n,d(x),d(y),d(z),d(a),d(b),d(c))); }
    public void a(sn entity) {
      try {
        Map<String,Object> event = map("event", "unexpected-add", "class", entity.getClass().getName(),
            "position", List.of(d(entity.aM),d(entity.aN),d(entity.aO)),
            "motion", List.of(d(entity.aP),d(entity.aQ),d(entity.aR)), "yaw", f(entity.aS),
            "entity_random", random((Random)field(sn.class,"bs").get(entity)));
        if (entity instanceof hl) {
          hl item = (hl)entity; event.put("stack", stack(item.a)); event.put("bob", f(item.d));
          Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
          event.put("math_post", random((Random)field(holder,"randomNumberGenerator").get(null)));
        }
        events.add(event);
      } catch (Exception failure) { throw new AssertionError(failure); }
    }
    public void b(sn entity) { events.add(List.of("remove",entity.getClass().getName())); }
    public void e() { events.add(List.of("refresh")); }
    public void a(String n,int x,int y,int z) { events.add(List.of("record",n,x,y,z)); }
    public void a(int x,int y,int z,ow tile) { events.add(List.of("tile",x,y,z,tile==null?"NULL":tile.getClass().getName())); }
    public void a(gs player,int a,int b,int c,int d,int e) { events.add(List.of("level",a,b,c,d,e)); }
  }
  static void deny(Runnable call) {
    try { call.run(); throw new AssertionError("guard denial expected"); }
    catch (SecurityException expected) { }
  }
  @SuppressWarnings("removal") public static void main(String[] args) throws Exception {
    Locale.setDefault(Locale.ROOT); need(args.length == 4, "jar, privateworld, profiles, use|support");
    final boolean use = args[3].equals("use"); need(use || args[3].equals("support"), "declared cohort");
    Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
    List<String> profiles = Files.readAllLines(Path.of(args[2]));
    System.setSecurityManager(new SecurityManager() {
      public void checkPermission(Permission permission) { }
      public void checkConnect(String h,int p) { ++network; throw new SecurityException("network"); }
      public void checkListen(int p) { ++network; throw new SecurityException("listen"); }
      public void checkLink(String n) { ++natives; throw new SecurityException("native"); }
      public void checkExec(String n) { ++execs; throw new SecurityException("exec"); }
      public void checkWrite(String n) {
        if (Path.of(n).toAbsolutePath().normalize().startsWith(directory)) { ++worldWrites; return; }
        ++deniedWrites; throw new SecurityException("outside private world");
      }
      public void checkDelete(String n) { checkWrite(n); }
    });
    for (Class<?> type : new Class<?>[]{ie.class,sm.class,iz.class,fd.class,lm.class,mx.class,
        uu.class,wf.class,pm.class,st.class,eb.class,ln.class}) origin(type,jar);
    kq app = null; xi stats = null; dc player = null;
    if (use) {
      for (Class<?> type : new Class<?>[]{kq.class,Minecraft.class,kg.class,dc.class,gs.class,xi.class,jl.class}) origin(type,jar);
      app = new kq(null,null,null,854,480,false,null);
      app.k = new gr("RedstoneUseReference","-"); field(Minecraft.class,"Z").set(app,directory.toFile());
      stats = new xi(app.k,directory.toFile()); app.I = stats;
    }
    fd world = new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
    observedWorld = world; chunks = new lm[]{world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
    if (use) { player = new dc(app,world,app.k,0); app.f = world; app.h = player; }
    uu registered = uu.m[55]; need(registered != null && registered.getClass() == sm.class && registered.bn == 55, "actual registered SM55");
    sm block = (sm)registered;
    gm item = use ? gm.c[331] : null; need(!use || item != null && item.getClass() == ie.class, "actual registered IE331");
    Object originalSet = field(sm.class,"b").get(block);
    Observer observer = new Observer(); world.a(observer); int index = 0;
    for (String profile : profiles) {
      String[] columns = profile.split("\t"); need(columns.length == (use ? 17 : 7), "exact frozen profile width");
      int[] p = new int[columns.length-1]; List<Integer> values = new ArrayList<>();
      for (int i = 0; i < p.length; ++i) { p[i] = Integer.parseInt(columns[i+1]); values.add(p[i]); }
      prepare(use,p); if (use) world.B = p[13] != 0;
      iz stack = use ? new iz(p[0],p[1],p[2]) : null; observedStack = stack;
      observer.reset(use ? p[15] : 0);
      byte[][] current = arrays(), prior = new byte[current.length][];
      for (int i = 0; i < current.length; ++i) prior[i] = current[i].clone();
      Random randomOwner = world.r;
      Object queueBefore = queues(world), setBefore = setState(block), rngBefore = random(randomOwner);
      List<Integer> stackBefore = stack(stack);
      int used331 = use ? stats.a(jl.E[331]) : 0, used332 = use ? stats.a(jl.E[332]) : 0;
      Integer healthBefore = use ? Integer.valueOf(field(ls.class,"Y").getInt(player)) : null;
      Object clickedBefore = use ? cell(p[4],p[5],p[6]) : null;
      Object targetBefore = use ? cell(p[7],p[8],p[9]) : cell(p[0],p[1],p[2]);
      String result = "UNRETURNED", exception = "NONE"; List<Object> exceptionFrames = new ArrayList<>(); active = index;
      try {
        boolean returned = use ? ((p[14]&2) != 0 ? null : stack).a((p[14]&4) != 0 ? null : player,
            (p[14]&1) != 0 ? null : world,p[4],p[5],p[6],p[3])
            : block.a(p[5] != 0 ? null : world,p[0],p[1],p[2]);
        result = Boolean.toString(returned);
      } catch (Throwable failure) {
        exception = failure.getClass().getName();
        for (StackTraceElement frame : failure.getStackTrace())
          exceptionFrames.add(List.of(frame.getClassName(),frame.getMethodName(),frame.getLineNumber()));
      }
      finally { active = -1; }
      Object rngAfter = random(world.r);
      List<Object> logicalEvents = new ArrayList<>(), unexpected = new ArrayList<>();
      for (Object event : observer.events) {
        if (event instanceof Map && "unexpected-add".equals(((Map<?,?>)event).get("event"))) unexpected.add(event);
        else logicalEvents.add(event);
      }
      System.out.println((use ? "USE\t" : "SUPPORT\t") + json(map("index",index,"name",columns[0],"values",values,
          "result",result,"exception",exception,"stack_before",stackBefore,"stack_after",stack(stack),
          "clicked_before",clickedBefore,"clicked_after",use?cell(p[4],p[5],p[6]):null,
          "target_before",targetBefore,"target_after",use?cell(p[7],p[8],p[9]):cell(p[0],p[1],p[2]),
          "changes",changes(prior),"queues_before",queueBefore,"queues_after",queues(world),
          "set_before",setBefore,"set_after",setState(block),"events",logicalEvents,
          "unexpected_adds",unexpected.size(),"remote",world.B,"world_o",field(fd.class,"o").getBoolean(world),
          "registered_block_class",registered.getClass().getName(),"registered_block_id",registered.bn,
          "registered_item_class",use?item.getClass().getName():null,"registered_item_slot",use?Integer.valueOf(331):null,
          "registered_owners_same",uu.m[55] == registered && (!use || gm.c[331] == item),
          "used331_delta",use?Integer.valueOf(stats.a(jl.E[331])-used331):null,"used332_delta",use?Integer.valueOf(stats.a(jl.E[332])-used332):null,
          "health_before",healthBefore,"health_after",use?Integer.valueOf(field(ls.class,"Y").getInt(player)):null,
          "world_random_same",world.r == randomOwner,"world_rng_unchanged",world.r == randomOwner && rngBefore.equals(rngAfter))));
      System.out.println("OWN_ENTROPY\t"+json(map("index",index,"name",columns[0],"world_before",rngBefore,
          "world_after",rngAfter,"world_random_same",world.r == randomOwner,"unexpected_entities",unexpected)));
      if (!exception.equals("NONE")) System.out.println("SOURCE_EXCEPTION\t"+json(map("index",index,
          "name",columns[0],"exception",exception,"frames",exceptionFrames)));
      need(field(sm.class,"b").get(block) == originalSet && uu.m[55] == registered && (!use || gm.c[331] == item), "same actual registered owners");
      ++index;
    }
    need(index == (use ? 42 : 12), "complete frozen roster");
    for (int i = 0; i < chunks.length; ++i) need(world.c(i/2,i%2) == chunks[i], "same canonical Chunks");
    need(network == 0 && natives == 0 && execs == 0 && deniedWrites == 0, "no source external attempts");
    System.out.println("GUARD\tsource\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes="+worldWrites);
    deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));
    deny(()->System.getSecurityManager().checkListen(25565)); deny(()->System.getSecurityManager().checkLink("redstone-guard"));
    deny(()->System.getSecurityManager().checkExec("redstone-guard"));
    deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("redstone-guard").toString()));
    need(network == 2 && natives == 1 && execs == 1 && deniedWrites == 1, "five denial controls");
    System.out.println("GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\tdenied-writes=1");
    System.out.println("COMPLETE\t"+args[3]+"\t"+index);
    // KG's original constructor sets daemon=true. Do not write Minecraft.J,
    // interrupt it, or repair a Source phase/clock to make this process exit.
  }
}
