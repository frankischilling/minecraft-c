import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Prepared component operands; every game constructor and invoked body is unchanged. */
public final class PlayerDamageWakeObservation {
    static kq app;
    static xi writer;
    static fd world;
    static lm chunk;
    static dc actor;
    static eq originalBounds;
    static br originalBed, originalSpawn, originalPrivate;
    static Random shared;
    static Path archive, directory;
    static int network, nativeLinks, rejectedWrites, execs;
    static final int[][] FACING = {{0,1},{-1,0},{0,-1},{1,0}};
    static final List<Object> sounds = new ArrayList<>();

    static void need(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object get(Object value, Class<?> owner, String name) throws Exception {
        return field(owner,name).get(value);
    }
    static void put(Object value, Class<?> owner, String name, Object replacement) throws Exception {
        field(owner,name).set(value,replacement);
    }
    static Map<String,Object> map(Object... values) { return ConstructorJson.map(values); }
    static String f(float x) { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x)); }
    static String d(double x) { return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x)); }
    static String hash(byte[] value) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));
    }
    static Object xyz(br value) { return value == null ? null : List.of(value.a,value.b,value.c); }
    static Object bounds(eq value) {
        return value == null ? null : List.of(d(value.a),d(value.b),d(value.c),d(value.d),d(value.e),d(value.f));
    }
    static Object random(Random value) throws Exception {
        return value == null ? null : List.of(String.format(Locale.ROOT,"%016x",((AtomicLong)get(value,Random.class,"seed")).get()),
            get(value,Random.class,"haveNextNextGaussian"),d((Double)get(value,Random.class,"nextNextGaussian")));
    }
    static Object statMap(String name) throws Exception {
        Map<?,?> source = (Map<?,?>)get(writer,xi.class,name);
        TreeMap<Integer,Integer> ordered = new TreeMap<>();
        for (Map.Entry<?,?> item : source.entrySet())
            need(ordered.put((Integer)get(item.getKey(),vr.class,"e"),((Number)item.getValue()).intValue()) == null,"unique statistic IDs");
        List<Object> result = new ArrayList<>();
        for (Map.Entry<Integer,Integer> item : ordered.entrySet()) result.add(List.of(item.getKey(),item.getValue()));
        return result;
    }
    static int metadata() { return chunk.e.a[((8*16+8)*128+64)>>1] & 15; }
    static Object planes() throws Exception {
        return map("blocks",hash(chunk.b),"metadata",hash(chunk.e.a),"sky",hash(chunk.f.a),"block_light",hash(chunk.g.a),"height",hash(chunk.h));
    }
    static Object state() throws Exception {
        br spawn = actor.Q(), privateCoordinate = (br)get(actor,gs.class,"bN");
        return map("position",List.of(d(actor.aM),d(actor.aN),d(actor.aO)),
            "motion",List.of(d(actor.aP),d(actor.aQ),d(actor.aR)),
            "dimensions",List.of(f(actor.bg),f(actor.bh),f(actor.bf)),
            "sleep_offsets",List.of(f(actor.w),f(actor.y)),"sleeping",actor.u,"timer",get(actor,gs.class,"a"),
            "bed",xyz(actor.v),"spawn",xyz(spawn),"private_coordinate",xyz(privateCoordinate),
            "bounds",bounds(actor.aW),"original_bounds",bounds(originalBounds),
            "world_same",actor.aI==world,"bounds_same",actor.aW==originalBounds,
            "bed_same",actor.v==originalBed,"spawn_same",spawn==originalSpawn,"private_same",privateCoordinate==originalPrivate,
            "health",actor.Y,"previous_health",actor.Z,"hurt_resistance",actor.by,"hurt_time",actor.aa,"hurt_maximum",actor.ab,
            "previous_damage",get(actor,ls.class,"au"),"idle",get(actor,ls.class,"av"),"attacked",actor.bb,
            "removed",actor.be,"armor_remainder",get(actor,gs.class,"bO"),
            "world_sleeping",get(world,fd.class,"J"),"remote",world.B,"world_players",world.d.size(),"world_entities",world.b.size(),
            "head",List.of(chunk.b[(8*16+8)*128+64]&255,metadata()),"planes",planes(),
            "random",List.of(random(actor.bs),random(world.r),random(shared)),
            "statistics",map("total",statMap("a"),"unsent",statMap("b"),"dirty",get(writer,xi.class,"c")),
            "sounds",new ArrayList<>(sounds));
    }
    static void cell(int x,int y,int z,int id,int data) {
        int index=((x&15)*16+(z&15))*128+y; chunk.b[index]=(byte)id;
        int shift=(index&1)*4;
        chunk.e.a[index>>1]=(byte)((chunk.e.a[index>>1]&~(15<<shift))|((data&15)<<shift));
    }
    static void setup(int[] input) throws Exception {
        world=new fd(new wy(),"PlayerDamageWakeReference",xa.a(0),5143L);
        world.a(new br(8,64,8)); chunk=world.c(0,0);
        need(chunk.getClass()==lm.class && world.w().getClass()==ok.class,"canonical Chunk and provider");
        Arrays.fill(chunk.b,(byte)0); Arrays.fill(chunk.e.a,(byte)0); Arrays.fill(chunk.f.a,(byte)0);
        Arrays.fill(chunk.g.a,(byte)0); Arrays.fill(chunk.h,(byte)64);
        for(int x=0;x<16;++x) for(int z=0;z<16;++z) {
            cell(x,63,z,2,0);
            for(int y=64;y<128;++y) { int index=(x*16+z)*128+y; chunk.f.a[index>>1]|=(byte)(15<<((index&1)*4)); }
        }
        actor=new dc(app,world,app.k,0); app.f=world; app.h=actor; app.i=actor;
        actor.e(8.5,64.0+(double)1.62f,8.5);
        need(actor.Y==20 && !actor.u && world.b(actor),"healthy actually published Player");
        actor.a(new br(42,66,44)); put(actor,gs.class,"bN",new br(-11,65,13));
        actor.w=.375f; actor.y=-.625f; actor.aP=.25; actor.aQ=-.375; actor.aR=.125;
        world.f=5; int direction=input[1], dx=FACING[direction][0], dz=FACING[direction][1];
        cell(8,64,8,26,direction|8); cell(8-dx,64,8-dz,26,direction);
        need(actor.b(8,64,8)==cw.a && actor.u && (Boolean)get(world,fd.class,"J"),"actual successful sleep preparation");
        put(actor,gs.class,"a",37); cell(8,64,8,26,direction|12);
        actor.E=20; actor.Y=input[2]; actor.Z=17; actor.by=0; actor.aa=3; actor.ab=4; actor.ad=0; actor.ac=99;
        actor.al=.25f; actor.bb=false; actor.be=false; actor.aS=30;
        put(actor,ls.class,"au",0); put(actor,ls.class,"av",17); put(actor,ls.class,"ah",false); put(actor,gs.class,"bO",0);
        actor.bs.setSeed(38); world.r.setSeed(2468); shared.setSeed(0);
        ((Map<?,?>)get(writer,xi.class,"a")).clear(); ((Map<?,?>)get(writer,xi.class,"b")).clear(); put(writer,xi.class,"c",false);
        sounds.clear(); world.a(new Observer());
        originalBounds=actor.aW;
        int mode=input[4];
        if(mode==1 || mode==10) world.B=true;
        if(mode==2 || mode==7 || mode==12) actor.aI=null;
        if(mode==3 || mode==8 || mode==9 || mode==10) put(actor,sn.class,"aW",null);
        if(mode==4 || mode==9 || mode==12) actor.v=null;
        if(mode==5 || mode==8) { cell(8,64,8,0,0); cell(8-dx,64,8-dz,0,0); }
        if(mode==6) put(actor,dc.class,"b",null);
        if(mode==7 || mode==13) actor.u=false;
        if(mode==11) actor.c=null;
        originalBed=actor.v; originalSpawn=actor.Q(); originalPrivate=(br)get(actor,gs.class,"bN");
    }
    static final class Observer implements pm {
        public void a(String name,double x,double y,double z,float volume,float pitch) {
            sounds.add(map("name",name,"position",List.of(d(x),d(y),d(z)),"volume",f(volume),"pitch",f(pitch)));
        }
        public void a(sn value) { throw new AssertionError("unexpected Entity publication"); }
        public void b(sn value) { throw new AssertionError("unexpected Entity removal"); }
        public void a(String n,double x,double y,double z,double a,double b,double c) { throw new AssertionError("unexpected particle"); }
        public void a(int x,int y,int z) {}
        public void b(int x,int y,int z,int a,int b,int c) {}
        public void e() { throw new AssertionError("unexpected refresh"); }
        public void a(String n,int x,int y,int z) { throw new AssertionError("unexpected record"); }
        public void a(int x,int y,int z,ow value) { throw new AssertionError("unexpected TileEntity"); }
        public void a(gs p,int a,int b,int c,int d,int e) { throw new AssertionError("unexpected event"); }
    }
    static void workers() throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        for(;;) {
            boolean found=false;
            for(Thread t:Thread.getAllStackTraces().keySet()) if(t.getClass().getName().equals("vk")||t.getClass().getName().equals("vj")) { found=true; t.join(100); }
            if(!found && !(Boolean)get(get(writer,xi.class,"d"),cj.class,"a")) return;
            need(System.nanoTime()<deadline,"statistics workers deadline"); Thread.sleep(1);
        }
    }
    @SuppressWarnings("removal") static final class Guard extends SecurityManager {
        public void checkPermission(Permission permission) {}
        public void checkConnect(String host,int port) { ++network; throw new SecurityException("network"); }
        public void checkListen(int port) { ++network; throw new SecurityException("listen"); }
        public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native link"); }
        public void checkExec(String command) { ++execs; throw new SecurityException("exec"); }
        public void checkWrite(String path) {
            if(!Path.of(path).toAbsolutePath().normalize().startsWith(directory)) { ++rejectedWrites; throw new SecurityException("outside isolated work directory"); }
        }
        public void checkDelete(String path) { checkWrite(path); }
    }
    static void bind(Path names) throws Exception {
        for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)) {
            Class<?> type=Class.forName(name.replace('/','.'),false,PlayerDamageWakeObservation.class.getClassLoader());
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"unchanged game class origin "+name);
            byte[] bytes; try(InputStream in=type.getResourceAsStream("/"+name+".class")) { need(in!=null,"class resource"); bytes=in.readAllBytes(); }
            System.out.println("CLASS "+name+" "+bytes.length+" "+hash(bytes));
        }
    }
    @SuppressWarnings("removal") public static void main(String[] args) throws Exception {
        need(args.length==4,"archive isolated-directory profiles class-names"); archive=Path.of(args[0]).toRealPath(); directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new Guard()); bind(Path.of(args[3]));
        app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("BedNightComponent","-"); put(app,net.minecraft.client.Minecraft.class,"Z",directory.toFile());
            writer=new xi(app.k,directory.toFile()); app.I=writer; ep.f.a(new kh(app)); workers();
            shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");
            int count=0;
            for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)) {
                int[] input=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray(); need(input.length==5 && input[0]==count,"ordered profile"); setup(input);
                Object before=state(), result=null; String exception="NONE"; List<String> frames=new ArrayList<>();
                try { result=actor.a((sn)null,input[3]); }
                catch(NullPointerException failure) {
                    exception="java/lang/NullPointerException";
                    for(StackTraceElement frame:failure.getStackTrace()) {
                        if(frame.getClassName().equals("PlayerDamageWakeObservation")) break;
                        frames.add(frame.getClassName()+"."+frame.getMethodName());
                    }
                }
                Object after=state(); List<Integer> transport=new ArrayList<>(); for(int word:input) transport.add(word);
                System.out.println("PLAYER_DAMAGE_WAKE "+ConstructorJson.json(map("schema","PLAYER_DAMAGE_WAKE_ROW_V1","case",count,"input",transport,"result",result,"exception",exception,"exception_frames",frames,"before",before,"after",after)));
                ++count; actor=null; world=null; chunk=null; originalBounds=null; originalBed=null; originalSpawn=null; originalPrivate=null;
                if(count%4==0) System.gc();
            }
            need(network==0 && nativeLinks==0 && rejectedWrites==0 && execs==0,"external effects guards");
            System.out.println("PLAYER_DAMAGE_WAKE_COMPLETE cases="+count+" network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
        } finally {
            if(writer!=null) workers(); app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet()) if(thread.getName().equals("Timer hack thread")) { thread.interrupt(); thread.join(2000); need(!thread.isAlive(),"Timer worker retirement"); }
        }
    }
}
