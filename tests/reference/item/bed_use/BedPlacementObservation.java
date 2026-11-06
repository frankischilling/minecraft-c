import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Declared finite operands around unchanged LP/World/Chunk/Bed bodies. */
public final class BedPlacementObservation {
    static int active = -1, network, natives, execs, deniedWrites, worldWrites;
    static fd observedWorld;
    static dc observedPlayer;
    static iz observedStack;
    static gm observedItem;
    static uu observedBed;
    static lm[] chunks;
    static int x, y, z, hx, hz, direction, callbackMode;
    static boolean footSeen, headSeen;
    static final List<Object> actions = new ArrayList<>();

    static void need(boolean v, String m) { if (!v) throw new AssertionError(m); }
    static Field field(Class<?> c, String n) throws Exception {
        Field f = c.getDeclaredField(n); f.setAccessible(true); return f;
    }
    static String bits(float f) { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(f)); }
    static Map<String,Object> map(Object... p) {
        Map<String,Object> r = new LinkedHashMap<>();
        for (int i = 0; i < p.length; i += 2) r.put((String)p[i], p[i+1]);
        return r;
    }
    static String json(Object v) {
        if (v == null) return "null";
        if (v instanceof String) return "\"" + ((String)v).replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
        if (v instanceof Map) {
            StringJoiner j = new StringJoiner(",", "{", "}");
            for (Object o : ((Map<?,?>)v).entrySet()) {
                Map.Entry<?,?> e = (Map.Entry<?,?>)o; j.add(json(e.getKey()) + ":" + json(e.getValue()));
            }
            return j.toString();
        }
        if (v instanceof Iterable) {
            StringJoiner j = new StringJoiner(",", "[", "]");
            for (Object o : (Iterable<?>)v) j.add(json(o)); return j.toString();
        }
        return v.toString();
    }
    static long[] random(Random r) throws Exception {
        return new long[]{((AtomicLong)field(Random.class,"seed").get(r)).get(),
            field(Random.class,"haveNextNextGaussian").getBoolean(r) ? 1 : 0,
            Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(r))};
    }
    static void put(int px, int py, int pz, int id, int metadata) {
        if (px < 0 || px > 31 || pz < 0 || pz > 31 || py < 0 || py > 127) return;
        lm c = chunks[(px >> 4) * 2 + (pz >> 4)];
        int i = ((px & 15) << 11) | ((pz & 15) << 7) | py;
        c.b[i] = (byte)id; int shift = (i & 1) * 4;
        c.e.a[i >> 1] = (byte)((c.e.a[i >> 1] & ~(15 << shift)) | ((metadata & 15) << shift));
    }
    static List<Integer> cell(int px, int py, int pz) {
        if (px < 0 || px > 31 || pz < 0 || pz > 31 || py < 0 || py > 127) return List.of(0,0);
        lm c = chunks[(px >> 4) * 2 + (pz >> 4)];
        int i = ((px & 15) << 11) | ((pz & 15) << 7) | py;
        return List.of(c.b[i] & 255, (c.e.a[i >> 1] >>> ((i & 1) * 4)) & 15);
    }
    static byte[][] arrays() {
        byte[][] a = new byte[chunks.length * 5][];
        for (int i = 0; i < chunks.length; ++i) {
            a[i*5] = chunks[i].b.clone(); a[i*5+1] = chunks[i].e.a.clone();
            a[i*5+2] = chunks[i].f.a.clone(); a[i*5+3] = chunks[i].g.a.clone();
            a[i*5+4] = chunks[i].h.clone();
        }
        return a;
    }
    static List<Object> changes(byte[][] before) {
        byte[][] after = arrays(); List<Object> r = new ArrayList<>();
        for (int a = 0; a < before.length; ++a) {
            need(before[a].length == after[a].length, "complete stable Chunk arrays");
            for (int i = 0; i < before[a].length; ++i)
                if (before[a][i] != after[a][i]) r.add(List.of(a/5,a%5,i,before[a][i]&255,after[a][i]&255));
        }
        return r;
    }
    static void entered() {}
    static void finished() {}
    static final class DeclaredCallbackFailure extends RuntimeException {
        private static final long serialVersionUID = 1L;
        DeclaredCallbackFailure(String m) { super(m); }
    }
    static final class Observer implements pm {
        final List<Object> notices = new ArrayList<>();
        public void a(int px,int py,int pz) {
            notices.add(List.of("cell",px,py,pz));
            if (!footSeen && px == x && py == y && pz == z) {
                footSeen = true;
                if ((callbackMode & 1) != 0) {
                    put(hx,y,hz,observedBed.bn,direction+8);
                    actions.add(List.of("prepare-head",hx,y,hz,observedBed.bn,direction+8));
                }
                if ((callbackMode & 8) != 0) {
                    observedStack.a = 100; actions.add(List.of("stack-count",100));
                }
                if ((callbackMode & 16) != 0) {
                    try { field(uu.class,"bn").setInt(observedBed,1); }
                    catch (Exception e) { throw new AssertionError(e); }
                    actions.add(List.of("bed-id",1));
                }
                if ((callbackMode & 2) != 0) throw new DeclaredCallbackFailure("foot");
            }
            if (!headSeen && px == hx && py == y && pz == hz) {
                headSeen = true;
                if ((callbackMode & 4) != 0) throw new DeclaredCallbackFailure("head");
            }
        }
        public void b(int a,int b,int c,int d,int e,int f) { notices.add(List.of("range",a,b,c,d,e,f)); }
        public void a(String n,double a,double b,double c,float d,float e) { notices.add(List.of("sound",n)); }
        public void a(String n,double a,double b,double c,double d,double e,double f) { notices.add(List.of("particle",n)); }
        public void a(sn e) { notices.add(List.of("add",e.getClass().getName())); }
        public void b(sn e) { notices.add(List.of("remove",e.getClass().getName())); }
        public void e() { notices.add(List.of("refresh")); }
        public void a(String n,int a,int b,int c) { notices.add(List.of("record",n,a,b,c)); }
        public void a(int a,int b,int c,ow t) { notices.add(List.of("tile",a,b,c,t==null?"null":t.getClass().getName())); }
        public void a(gs p,int a,int b,int c,int d,int e) { notices.add(List.of("level",a,b,c,d,e)); }
    }
    static void deny(Runnable r) {
        try { r.run(); throw new AssertionError("guard must refuse"); }
        catch (SecurityException expected) {}
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT); need(args.length == 3,"jar private-world profiles");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        List<String> profiles = Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission p) {}
            public void checkConnect(String h,int p) { ++network; throw new SecurityException("connect"); }
            public void checkListen(int p) { ++network; throw new SecurityException("listen"); }
            public void checkLink(String n) { ++natives; throw new SecurityException("native"); }
            public void checkExec(String n) { ++execs; throw new SecurityException("exec"); }
            public void checkWrite(String n) {
                if (Path.of(n).toAbsolutePath().normalize().startsWith(directory)) { ++worldWrites; return; }
                ++deniedWrites; throw new SecurityException("outside private World");
            }
            public void checkDelete(String n) { checkWrite(n); }
        });
        for (Class<?> c : new Class<?>[]{lp.class,gm.class,iz.class,fd.class,lm.class,wi.class,
                mx.class,dc.class,gs.class,ls.class,sn.class,uu.class,ve.class,in.class,pm.class})
            need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "unchanged source origin");
        fd world = new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        chunks = new lm[]{world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
        dc player = new dc(null,world,new gr("BedPlacementReference","-"),0);
        observedWorld = world; observedPlayer = player;
        gm item = gm.c[355]; uu bed = uu.T;
        need(item.getClass() == lp.class && bed.getClass() == ve.class && bed.bn == 26,"actual Item355 and Bed26");
        observedItem = item; observedBed = bed;
        Observer observer = new Observer(); world.a(observer);
        uu[] originalSlots = uu.m.clone(); int index = 0;
        for (String line : profiles) {
            String[] c = line.split("\t"); need(c.length == 17,"exact profile width");
            int[] p = new int[16];
            for (int i=0;i<p.length;++i) p[i] = i == 4 ? Integer.parseUnsignedInt(c[i+1],16) : Integer.parseInt(c[i+1]);
            int face=p[0]; x=p[1]; y=p[2]+1; z=p[3]; float yaw=Float.intBitsToFloat(p[4]);
            double scaled=(double)(yaw*4.0f/360.0f)+.5d; int floor=(int)scaled;
            if (scaled < floor) --floor; direction=floor&3;
            int dx=direction==1?-1:direction==3?1:0, dz=direction==0?1:direction==2?-1:0;
            hx=x+dx; hz=z+dz; callbackMode=p[15]; footSeen=false; headSeen=false; actions.clear();
            for (lm chunk : chunks) {
                Arrays.fill(chunk.b,(byte)0); Arrays.fill(chunk.e.a,(byte)0);
                Arrays.fill(chunk.f.a,(byte)15); Arrays.fill(chunk.g.a,(byte)0);
                Arrays.fill(chunk.h,(byte)0); chunk.i=0; chunk.n=true;
            }
            ((List<?>)field(fd.class,"C").get(world)).clear();
            world.B=p[10]!=0; world.r.setSeed(1000+index); player.bs.setSeed(2000+index);
            put(x,y-1,z,p[8],0); put(hx,y-1,hz,p[9],0);
            put(x,y,z,p[6],p[13]); put(hx,y,hz,p[7],p[14]);
            field(uu.class,"bn").setInt(bed,p[12]); player.aS=yaw;
            iz stack=new iz(355,p[5],0); stack.b=7; observedStack=stack;
            observer.notices.clear(); byte[][] before=arrays();
            List<Integer> footBefore=cell(x,y,z),headBefore=cell(hx,y,hz);
            long[] worldRandom=random(world.r),playerRandom=random(player.bs);
            String result="UNRETURNED",exception="NONE",message="";
            active=index; entered();
            try {
                result=Boolean.toString(item.a((p[11]&2)!=0?null:stack,(p[11]&4)!=0?null:player,
                    (p[11]&1)!=0?null:world,x,p[2],z,face));
            } catch (NullPointerException e) { exception=e.getClass().getName(); }
              catch (DeclaredCallbackFailure e) { exception=e.getClass().getName(); message=e.getMessage(); }
            finally { finished(); active=-1; }
            boolean randomSame=Arrays.equals(worldRandom,random(world.r)) && Arrays.equals(playerRandom,random(player.bs));
            need(randomSame,"LP and these exact original children consume no Random");
            List<Object> values=new ArrayList<>(); for(int i=0;i<p.length;++i) values.add(i==4?bits(yaw):p[i]);
            System.out.println("BED_PLACEMENT\t"+json(map("index",index,"name",c[0],"values",values,
                "direction",direction,"target",List.of(x,y,z),"head",List.of(hx,y,hz),
                "result",result,"exception",exception,"message",message,
                "count_before",p[5],"count_after",stack.a,"stack_id",stack.c,"animation",stack.b,
                "foot_before",footBefore,"foot_after",cell(x,y,z),"head_before",headBefore,"head_after",cell(hx,y,hz),
                "bed_id_after",bed.bn,"yaw_after",bits(player.aS),"remote_after",world.B,
                "notifications",observer.notices,"observer_actions",actions,"changes",changes(before),
                "light_queue",((List<?>)field(fd.class,"C").get(world)).size(),"random_unchanged",randomSame)));
            field(uu.class,"bn").setInt(bed,26); ++index;
        }
        need(Arrays.equals(originalSlots,uu.m) && uu.T==bed && gm.c[355]==item,"original singleton identity inventory unchanged");
        for (int i=0;i<chunks.length;++i) need(world.c(i/2,i%2)==chunks[i],"canonical original Chunk identities");
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no external original-code effects");
        System.out.println("BED_PLACEMENT_GUARD network=0 native=0 exec=0 denied-writes=0 canonical-chunks=4 origin-classes=15 source-class-replacements=0");
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));
        deny(()->System.getSecurityManager().checkListen(25565));
        deny(()->System.getSecurityManager().checkLink("bed-placement-guard"));
        deny(()->System.getSecurityManager().checkExec("bed-placement-guard"));
        deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("bed-placement-guard").toString()));
        need(network==2&&natives==1&&execs==1&&deniedWrites==1,"all five guard counter controls");
        System.out.println("BED_PLACEMENT_COMPLETE rows="+index+" guard-controls=5 jdi-source-writes=0 jdi-target-invocations=0");
    }
}
