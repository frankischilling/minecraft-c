import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored finite inputs and passive observations; every game body is unchanged. */
public final class RedstoneTorchObservation {
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
  static final List<Object> held = new ArrayList<>();
  static final int MAX_HELD = 4096, MAX_NODES = 256, MAX_BUCKETS = 1024;
  static final Random independent = new Random(31L);
  static fd[] worlds;
  static lm[][] allChunks;
  static long[] times;
  static Observer[] observers;
  static int active = -1, randomMode;
  static Object observationUnsafe;
  static Method shouldBeInitialized;
  static Class<?> mathHolder;
  static void initializeObservation() throws Exception {
    Class<?> unsafeClass=Class.forName("sun.misc.Unsafe");
    observationUnsafe=field(unsafeClass,"theUnsafe").get(null);
    shouldBeInitialized=unsafeClass.getDeclaredMethod("shouldBeInitialized",Class.class);
    mathHolder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",false,null);
  }
  static Object mathIfInitialized() throws Exception {
    if ((Boolean)shouldBeInitialized.invoke(observationUnsafe,mathHolder)) return null;
    return random((Random)field(mathHolder,"randomNumberGenerator").get(null));
  }
  static Object scheduleCounter() throws Exception {
    boolean initialized=!(Boolean)shouldBeInitialized.invoke(observationUnsafe,qy.class);
    return map("initialized",initialized,"counter",
        initialized?Long.valueOf(field(qy.class,"f").getLong(null)):null);
  }
  static String seedUniquifier() throws Exception {
    return String.format(Locale.ROOT,"%016x",
        ((AtomicLong)field(Random.class,"seedUniquifier").get(null)).get());
  }
  static int id(Object value) {
    if (value == null) return 0;
    for (int i = 0; i < held.size(); ++i) if (held.get(i) == value) return i + 1;
    need(held.size() < MAX_HELD, "bounded passive identity inventory");
    held.add(value); return held.size();
  }
  static Object get(Object value, Class<?> owner, String name) throws Exception {
    return field(owner, name).get(value);
  }
  static long time(fd world) throws Exception {
    return field(ei.class, "e").getLong(get(world, fd.class, "x"));
  }
  static List<Integer> cell(int role, int x, int y, int z) {
    if (x < 0 || x > 31 || y < 0 || y > 127 || z < 0 || z > 31) return null;
    lm chunk = allChunks[role][(x >> 4) * 2 + (z >> 4)];
    int index = ((x & 15) << 11) | ((z & 15) << 7) | y;
    return List.of(chunk.b[index] & 255, (chunk.e.a[index >> 1] >>> ((index & 1) * 4)) & 15);
  }
  static byte[][] allArrays() {
    byte[][] result = new byte[worlds.length * 20][];
    int at = 0;
    for (lm[] group : allChunks) for (lm chunk : group) {
      result[at++] = chunk.b; result[at++] = chunk.e.a; result[at++] = chunk.f.a;
      result[at++] = chunk.g.a; result[at++] = chunk.h;
    }
    return result;
  }
  static String hash(byte[] value) throws Exception {
    byte[] digest = java.security.MessageDigest.getInstance("SHA-256").digest(value);
    StringBuilder result = new StringBuilder();
    for (byte b : digest) result.append(String.format(Locale.ROOT, "%02x", b & 255));
    return result.toString();
  }
  static List<Object> arrayState() throws Exception {
    List<Object> result = new ArrayList<>(); int at = 0;
    for (byte[] value : allArrays()) {
      result.add(List.of(at / 20, (at % 20) / 5, at % 5, id(value), value.length, hash(value)));
      ++at;
    }
    return result;
  }
  static List<Object> changes(byte[][] before) {
    List<Object> result = new ArrayList<>(); byte[][] after = allArrays();
    need(before.length == after.length, "complete actual array inventory");
    for (int a = 0; a < after.length; ++a) {
      need(before[a].length == after[a].length, "actual array lengths");
      for (int i = 0; i < after[a].length; ++i) if (before[a][i] != after[a][i])
        result.add(List.of(a / 20, (a % 20) / 5, a % 5, i, before[a][i] & 255, after[a][i] & 255));
    }
    return result;
  }
  static void prepare(int[] p) {
    chunks = allChunks[p[0]];
    for (lm chunk : chunks) {
      Arrays.fill(chunk.b, (byte)0);
      Arrays.fill(chunk.e.a, (byte)0);
    }
    int x = p[3], y = p[4], z = p[5], metadata = p[6], layout = p[8];
    int sx = x, sy = y, sz = z;
    if (metadata == 1) --sx;
    else if (metadata == 2) ++sx;
    else if (metadata == 3) --sz;
    else if (metadata == 4) ++sz;
    else --sy;
    if (layout == 1 || layout == 2) {
      put(sx, sy, sz, 1, 0);
      if (layout == 2) {
        need(metadata >= 1 && metadata <= 4, "valid wall powered layout");
        put(sx, sy + 1, sz, 70, 1);
      }
    } else if (layout == 3) {
      put(x, y - 2, z, 1, 0); put(x, y - 1, z, 70, 1);
    } else need(layout == 0, "declared layout");
    put(x, y, z, p[7], metadata);
    worlds[p[0]].B = p[9] != 0;
  }
  static Object scalar(Object value) throws Exception {
    if (value == null) return null;
    if (value instanceof xu) return map("alias",id(value),"class","xu",
        "xyz",List.of(field(xu.class,"a").getInt(value),field(xu.class,"b").getInt(value),
                     field(xu.class,"c").getInt(value)),
        "time",field(xu.class,"d").getLong(value));
    if (value instanceof qy) return map("alias",id(value),"class","qy",
        "xyz",List.of(field(qy.class,"a").getInt(value),field(qy.class,"b").getInt(value),
                     field(qy.class,"c").getInt(value)),
        "block",field(qy.class,"d").getInt(value),"time",field(qy.class,"e").getLong(value),
        "order",field(qy.class,"g").getLong(value));
    if (value instanceof wf) return map("alias",id(value),"class","wf",
        "xyz",List.of(field(wf.class,"a").getInt(value),field(wf.class,"b").getInt(value),
                     field(wf.class,"c").getInt(value)));
    if (value instanceof st) {
      st r = (st)value;
      need(r.a == eb.a || r.a == eb.b, "original light kind");
      return map("alias",id(value),"class","st","region",
          List.of(r.a == eb.a ? 0 : 1,r.b,r.c,r.d,r.e,r.f,r.g));
    }
    if (value instanceof sn) {
      sn entity = (sn)value;
      Map<String,Object> result = map("alias",id(value),"class",value.getClass().getName());
      if (entity instanceof hl) {
        hl item = (hl)entity;
        result.put("stack",List.of(item.a.c,item.a.a,item.a.i()));
        result.put("defaults",List.of(item.c,item.b,field(hl.class,"f").getInt(item)));
      }
      return result;
    }
    return map("alias",id(value),"class",value.getClass().getName());
  }
  static Object list(Object object) throws Exception {
    need(object != null && object.getClass() == ArrayList.class, "actual ArrayList owner");
    Object[] data = (Object[])get(object, ArrayList.class, "elementData");
    int size = field(ArrayList.class, "size").getInt(object);
    need(size >= 0 && size <= data.length && data.length <= MAX_BUCKETS, "bounded full List backing");
    List<Object> slots = new ArrayList<>();
    for (Object value : data) slots.add(scalar(value));
    return map("alias",id(object),"backing",id(data),"size",size,"capacity",data.length,
        "modCount",field(AbstractList.class,"modCount").getInt(object),"slots",slots);
  }
  static Object hashSet(Object set) throws Exception {
    need(set != null && set.getClass() == HashSet.class, "actual HashSet");
    Object map = get(set, HashSet.class, "map");
    Object[] table = (Object[])get(map, HashMap.class, "table");
    need(table == null || table.length <= MAX_BUCKETS, "bounded complete HashMap table");
    Class<?> node = Class.forName("java.util.HashMap$Node", false, null);
    List<Object> nodes = new ArrayList<>(), seen = new ArrayList<>(), roots = new ArrayList<>();
    if (table != null) for (Object root : table) {
      roots.add(id(root));
      for (Object current = root; current != null; current = get(current,node,"next")) {
        need(seen.size() < MAX_NODES, "bounded complete hash nodes");
        for (Object earlier : seen) need(earlier != current, "acyclic buckets");
        seen.add(current);
        Map<String,Object> row = map("alias",id(current),"class",current.getClass().getName(),
            "hash",field(node,"hash").getInt(current),"key",scalar(get(current,node,"key")),
            "value",scalar(get(current,node,"value")),"next",id(get(current,node,"next")));
        if (current.getClass().getName().equals("java.util.HashMap$TreeNode")) {
          Class<?> tree = current.getClass();
          row.put("tree",List.of(id(get(current,tree,"parent")),id(get(current,tree,"left")),
              id(get(current,tree,"right")),id(get(current,tree,"prev")),
              field(tree,"red").getBoolean(current)));
        }
        nodes.add(row);
      }
    }
    return map("alias",id(set),"map",id(map),"table",id(table),
        "size",field(HashMap.class,"size").getInt(map),
        "threshold",field(HashMap.class,"threshold").getInt(map),
        "modCount",field(HashMap.class,"modCount").getInt(map),"roots",roots,"nodes",nodes);
  }
  static void treeNodes(Object node, List<Object> result, List<Object> seen) throws Exception {
    if (node == null) return;
    need(seen.size() < MAX_NODES, "bounded complete TreeMap");
    for (Object earlier : seen) need(earlier != node, "acyclic TreeMap");
    seen.add(node); Class<?> type = node.getClass();
    Object left = get(node,type,"left"), right = get(node,type,"right");
    result.add(map("alias",id(node),"key",scalar(get(node,type,"key")),"value",scalar(get(node,type,"value")),
        "left",id(left),"right",id(right),"parent",id(get(node,type,"parent")),
        "color",field(type,"color").getBoolean(node)));
    treeNodes(left,result,seen); treeNodes(right,result,seen);
  }
  static Object treeSet(Object set) throws Exception {
    need(set != null && set.getClass() == TreeSet.class, "actual TreeSet");
    Object map = get(set,TreeSet.class,"m");
    need(map.getClass() == TreeMap.class, "actual scheduler TreeMap");
    Object root = get(map,TreeMap.class,"root");
    List<Object> nodes = new ArrayList<>(); treeNodes(root,nodes,new ArrayList<>());
    return map("alias",id(set),"map",id(map),"root",id(root),"nodes",nodes,
        "size",field(TreeMap.class,"size").getInt(map),
        "modCount",field(TreeMap.class,"modCount").getInt(map));
  }
  static Object worldState() throws Exception {
    List<Object> result = new ArrayList<>();
    for (int role = 0; role < worlds.length; ++role) {
      fd w = worlds[role];
      result.add(map("role",role,"alias",id(w),"time",time(w),"a",w.a,"B",w.B,"o",w.o,"s",w.s,
          "M",field(fd.class,"M").getInt(w),"light",list(get(w,fd.class,"C")),
          "scheduled_tree",treeSet(get(w,fd.class,"E")),
          "scheduled_hash",hashSet(get(w,fd.class,"F")),
          "entities",list(get(w,fd.class,"b"))));
    }
    return result;
  }
  static Object history() throws Exception { return list(field(db.class,"b").get(null)); }
  static Object positionSet() throws Exception {
    return hashSet(field(sm.class,"b").get(uu.m[55]));
  }
  static final class Observer implements pm {
    final int role;
    final List<Object> events = new ArrayList<>(), raw = new ArrayList<>(), drops = new ArrayList<>();
    int mode; boolean refused;
    Observer(int role) { this.role = role; }
    void reset(int value) { events.clear(); raw.clear(); drops.clear(); mode=value; refused=false; }
    void refusal(int kind) {
      if (mode == kind && !refused) { refused=true; throw new IllegalStateException("declared DB callback refusal"); }
    }
    Map<String,Object> states() throws Exception {
      return map("world",random(worlds[role].r),"independent",random(independent));
    }
    public void a(int x,int y,int z) {
      events.add(map("event","cell","world",role,"xyz",List.of(x,y,z),"cell",cell(role,x,y,z)));
      refusal(1);
    }
    public void b(int a,int b,int c,int d,int e,int f) {
      events.add(List.of("range",role,a,b,c,d,e,f));
    }
    public void a(String name,double x,double y,double z,float volume,float pitch) {
      try {
        int ordinal=raw.size();
        raw.add(map("event","sound","ordinal",ordinal,"world",role,"name",name,
            "position",List.of(d(x),d(y),d(z)),"volume",f(volume),"pitch",f(pitch),"states",states()));
        events.add(map("event","sound","world",role,"name",name,"position",List.of(d(x),d(y),d(z)),
            "volume",f(volume),"raw_ordinal",ordinal));
      } catch (Exception failure) { throw new AssertionError(failure); }
      refusal(2);
    }
    public void a(String name,double x,double y,double z,double vx,double vy,double vz) {
      try {
        int ordinal=raw.size(); List<String> position=List.of(d(x),d(y),d(z)), motion=List.of(d(vx),d(vy),d(vz));
        raw.add(map("event","particle","ordinal",ordinal,"world",role,"name",name,
            "position",position,"motion",motion,"states",states()));
        Map<String,Object> value=map("event","particle","world",role,"name",name,"motion",motion,"raw_ordinal",ordinal);
        if (randomMode == 1) value.put("position",position);
        events.add(value);
      } catch (Exception failure) { throw new AssertionError(failure); }
      refusal(3);
    }
    public void a(sn entity) {
      try {
        need(entity instanceof hl,"genuine published drop class"); hl item=(hl)entity;
        int ordinal=raw.size();
        drops.add(List.of(item.getClass().getName(),item.a.c,item.a.a,item.a.i(),item.c,item.b,field(hl.class,"f").getInt(item)));
        events.add(map("event","add","world",role,"alias",id(entity),"raw_ordinal",ordinal));
        mathSeen=true;
        raw.add(map("event","item","ordinal",ordinal,"world",role,"alias",id(entity),
            "position",List.of(d(item.aM),d(item.aN),d(item.aO)),
            "motion",List.of(d(item.aP),d(item.aQ),d(item.aR)),"yaw",f(item.aS),"bob",f(item.d),
            "entity_random",random((Random)get(item,sn.class,"bs")),"math_post",mathRandom(),"states",states()));
      } catch (Exception failure) { throw new AssertionError(failure); }
    }
    public void b(sn value) { events.add(List.of("remove",role,id(value),value.getClass().getName())); }
    public void e() { events.add(List.of("refresh",role)); }
    public void a(String name,int x,int y,int z) { events.add(List.of("record",role,name,x,y,z)); }
    public void a(int x,int y,int z,ow tile) { events.add(List.of("tile",role,x,y,z,tile==null?0:id(tile))); }
    public void a(gs p,int a,int b,int c,int d,int e) { events.add(List.of("level",role,id(p),a,b,c,d,e)); }
  }
  static Object randomOwners() throws Exception {
    List<Object> result=new ArrayList<>();
    for (int i=0;i<worlds.length;++i) result.add(map("role",i,"alias",id(worlds[i].r),"state",random(worlds[i].r)));
    return map("worlds",result,"independent",map("alias",id(independent),"state",random(independent)));
  }
  static void deny(Runnable work) {
    try { work.run(); throw new AssertionError("guard must deny"); }
    catch (SecurityException expected) { }
  }
  @SuppressWarnings("removal")
  public static void main(String[] args) throws Exception {
    need(args.length==4,"jar, mutable World root, frozen profiles, cohort");
    Locale.setDefault(Locale.ROOT);
    Path jar=Path.of(args[0]).toRealPath(), directory=Path.of(args[1]).toRealPath(), profiles=Path.of(args[2]).toRealPath();
    String cohort=args[3]; need(Set.of("lifecycle","powered","burnout","aging").contains(cohort),"declared cohort");
    System.setSecurityManager(new SecurityManager() {
      public void checkPermission(Permission value) { }
      public void checkConnect(String h,int p) { ++network; throw new SecurityException("network"); }
      public void checkListen(int p) { ++network; throw new SecurityException("listen"); }
      public void checkLink(String n) { ++natives; throw new SecurityException("native"); }
      public void checkExec(String n) { ++execs; throw new SecurityException("exec"); }
      public void checkWrite(String n) {
        if (Path.of(n).toAbsolutePath().normalize().startsWith(directory)) { ++worldWrites; return; }
        ++deniedWrites; throw new SecurityException("outside private Worlds");
      }
      public void checkDelete(String n) { checkWrite(n); }
    });
    initializeObservation();
    for (Class<?> type:new Class<?>[]{fd.class,lm.class,mx.class,fm.class,wt.class,db.class,vm.class,
        uu.class,xu.class,qy.class,bv.class,sm.class,ei.class,nu.class,st.class,eb.class,pm.class,hl.class,sn.class,iz.class,wf.class})
      origin(type,jar);
    List<String> worldRows=Files.readAllLines(profiles.getParent().resolve("worlds.tsv"));
    int count=cohort.equals("aging")?6:1;
    worlds=new fd[count]; allChunks=new lm[count][]; times=new long[count]; observers=new Observer[count];
    for (int role=0;role<count;++role) {
      String[] row=worldRows.get(role).split("\t"); need(row.length==3 && Integer.parseInt(row[0])==role,"frozen World role");
      times[role]=Long.parseLong(row[2]); Path path=directory.resolve(row[1]).toRealPath();
      String constructorUniquifierBefore=seedUniquifier();
      Object constructorMathBefore=mathIfInitialized();
      fd world=new fd(new mx(path.getParent().toFile(),path.getFileName().toString(),false),"",23L);
      need(!world.s && world.t()==times[role],"actual saved World constructor time");
      worlds[role]=world;
      allChunks[role]=new lm[]{world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
      need(world.t()==times[role],"actual World time after canonical loads");
      for (int i=0;i<4;++i) {
        need(allChunks[role][i].j==i/2 && allChunks[role][i].k==i%2,"actual canonical LM coordinates");
        for(int j=0;j<i;++j) need(allChunks[role][j]!=allChunks[role][i],"distinct actual canonical Chunk owners");
      }
      observers[role]=new Observer(role); world.a(observers[role]);
      System.out.println("DB_WORLD\t"+json(map("role",role,"name",row[1],"time",time(world),"loaded",!world.s)));
      System.out.println("DB_WORLD_ENTROPY\t"+json(map("role",role,
          "seed_uniquifier_before",constructorUniquifierBefore,"seed_uniquifier_after",seedUniquifier(),
          "math_before",constructorMathBefore,"math_after",mathIfInitialized())));
    }
    need(uu.m[75].getClass()==db.class && uu.m[76].getClass()==db.class,"actual registered DB receivers");
    need(!field(db.class,"a").getBoolean(uu.m[75]) && field(db.class,"a").getBoolean(uu.m[76]),"actual off/on flags");
    Method powered=db.class.getDeclaredMethod("h",fd.class,int.class,int.class,int.class);
    Method burnout=db.class.getDeclaredMethod("a",fd.class,int.class,int.class,int.class,boolean.class);
    need(Modifier.isPrivate(powered.getModifiers()) && Modifier.isPrivate(burnout.getModifiers()),"exact private DB declarations");
    powered.setAccessible(true); burnout.setAccessible(true);
    Object listOwner=field(db.class,"b").get(null),offOwner=uu.m[75],onOwner=uu.m[76];
    int index=0,unplanned=0;
    for(String line:Files.readAllLines(profiles)) {
      String[] columns=line.split("\t");need(columns.length==16,"name plus15 signed inputs");
      int[] p=new int[15];List<Integer> values=new ArrayList<>();
      for(int i=0;i<15;++i){p[i]=Integer.parseInt(columns[i+1]);values.add(p[i]);}
      need(p[0]>=0 && p[0]<count && (p[2]==75 || p[2]==76),"actual World/receiver");
      prepare(p);fd observed=worlds[p[0]],actual=p[10]==0?observed:null;
      for(int w=0;w<count;++w){need(time(worlds[w])==times[w],"unmodified caller World times");observers[w].reset(p[14]);}
      Observer observer=observers[p[0]];randomMode=p[11];
      Random supplied=p[11]==0?observed.r:p[11]==1?independent:null;
      byte[][] before=allArrays();for(int i=0;i<before.length;++i)before[i]=before[i].clone();
      Object arrayBefore=arrayState(),worldBefore=worldState(),historyBefore=history(),setBefore=positionSet(),randomBefore=randomOwners();
      Object mathBefore=mathIfInitialized();
      String uniquifierBefore=seedUniquifier();
      Object counterBefore=scheduleCounter();
      List<Integer> centerBefore=cell(p[0],p[3],p[4],p[5]);
      String result="UNRETURNED",exception="NONE";List<Object> frames=new ArrayList<>();active=index;
      try {
        uu receiver=uu.m[p[2]];
        switch(p[1]) {
          case 0:receiver.c(actual,p[3],p[4],p[5]);result="VOID";break;
          case 1:receiver.b(actual,p[3],p[4],p[5]);result="VOID";break;
          case 2:receiver.b(actual,p[3],p[4],p[5],p[13]);result="VOID";break;
          case 3:receiver.a(actual,p[3],p[4],p[5],supplied);result="VOID";break;
          case 4:result=powered.invoke(receiver,actual,p[3],p[4],p[5]).toString();break;
          case 5:result=burnout.invoke(receiver,actual,p[3],p[4],p[5],p[12]!=0).toString();break;
          default:throw new AssertionError("declared entry");
        }
      } catch(Throwable failure) {
        Throwable reached=failure instanceof InvocationTargetException?failure.getCause():failure;
        exception=reached.getClass().getName();
        for(StackTraceElement frame:reached.getStackTrace())frames.add(List.of(frame.getClassName(),frame.getMethodName(),frame.getLineNumber()));
        if(!Set.of("java.lang.NullPointerException","java.lang.IllegalStateException").contains(exception))++unplanned;
      } finally {active=-1;}
      Object randomAfter=randomOwners(),mathAfter=mathIfInitialized();
      String uniquifierAfter=seedUniquifier();
      for(int w=0;w<count;++w)need(time(worlds[w])==times[w],"Source call did not alter World time");
      need(field(db.class,"b").get(null)==listOwner && uu.m[75]==offOwner && uu.m[76]==onOwner,"same actual class-shared owners");
      System.out.println("DB\t"+json(map("cohort",cohort,"index",index,"name",columns[0],"values",values,
          "world_role",p[0],"time",time(observed),"result",result,"exception",exception,
          "center_before",centerBefore,"center_after",cell(p[0],p[3],p[4],p[5]),
          "arrays_before",arrayBefore,"arrays_after",arrayState(),"changes",changes(before),
          "worlds_before",worldBefore,"worlds_after",worldState(),
          "history_before",historyBefore,"history_after",history(),
          "position_set_before",setBefore,"position_set_after",positionSet(),
          "qy_before",counterBefore,"qy_after",scheduleCounter(),
          "events",observer.events,"drops",observer.drops,"observer_refused",observer.refused)));
      System.out.println("DB_ENTROPY\t"+json(map("cohort",cohort,"index",index,"name",columns[0],"mode",p[11],
          "owners_before",randomBefore,"owners_after",randomAfter,"supplied_alias",id(supplied),
          "math_before",mathBefore,"math_after",mathAfter,
          "seed_uniquifier_before",uniquifierBefore,"seed_uniquifier_after",uniquifierAfter,"events",observer.raw)));
      if(!exception.equals("NONE"))System.out.println("DB_EXCEPTION\t"+json(map("cohort",cohort,"index",index,
          "name",columns[0],"exception",exception,"frames",frames,"declared_callback",p[14])));
      ++index;
    }
    need(unplanned==0,"unplanned Source/helper boundary retained above; interpretation stops");
    need(network==0 && natives==0 && execs==0 && deniedWrites==0,"no Source external attempts");
    System.out.println("DB_GUARD\tsource\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes="+worldWrites);
    deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));
    deny(()->System.getSecurityManager().checkListen(25565));deny(()->System.getSecurityManager().checkLink("db-guard"));
    deny(()->System.getSecurityManager().checkExec("db-guard"));
    deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("db-guard").toString()));
    need(network==2 && natives==1 && execs==1 && deniedWrites==1,"five guard denial controls");
    System.out.println("DB_GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\tdenied-writes=1");
    System.out.println("COMPLETE\t"+cohort+"\t"+index);
  }
}
