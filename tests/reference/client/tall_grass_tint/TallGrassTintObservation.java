import java.lang.reflect.Field;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/* Real constructors and unchanged RU/QM/FD/XV/IA bodies. Prepared Chunk
 * metadata, climate arrays and actual palette setters are declared inputs. */
public final class TallGrassTintObservation {
    static int connects, listens, links;
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Object staticRaw(Class<?> owner, String name) throws Exception {
        Class<?> uc = Class.forName("sun.misc.Unsafe");
        Object u = field(uc, "theUnsafe").get(null);
        Field f = field(owner, name);
        Object base = uc.getMethod("staticFieldBase", Field.class).invoke(u, f);
        long offset = (Long)uc.getMethod("staticFieldOffset", Field.class).invoke(u, f);
        return uc.getMethod("getObject", Object.class, long.class).invoke(u, base, offset);
    }
    static long state(Random random) throws Exception {
        return ((AtomicLong)field(Random.class, "seed").get(random)).get();
    }
    static String f(float value) { return String.format("%08x", Float.floatToRawIntBits(value)); }
    static String values(double[] array) {
        StringBuilder b = new StringBuilder();
        for (double value : array) {
            if (b.length() != 0) b.append(',');
            b.append(String.format("%016x", Double.doubleToRawLongBits(value)));
        }
        return b.toString();
    }
    static void observe(fd world, xv manager, String line) throws Exception {
        String[] cells = line.split("\t", -1);
        require(cells.length == 9 && cells[0].equals("INPUT"), "profile shape");
        int index = Integer.parseInt(cells[1]), mode = Integer.parseInt(cells[2]);
        int metadata = Integer.parseInt(cells[3]), x = Integer.parseInt(cells[4]);
        int y = Integer.parseInt(cells[5]), z = Integer.parseInt(cells[6]);
        int fault = Integer.parseInt(cells[7]), palette = Integer.parseInt(cells[8]);
        world.e(x, y, z, metadata); // Actual quiet setter before the measured body.
        manager.a = new double[]{9,8,7}; manager.b = new double[]{-1,-2,-3};
        field(xv.class, "c").set(manager, new double[]{6,5,4});
        if (palette == 1) {
            int[] grass = new int[65536], foliage = new int[65536];
            for (int i=0; i<65536; ++i) { grass[i]=0xa5000000|i; foliage[i]=0x5a000000|i; }
            ia.a(grass); jh.a(foliage);
        } else if (palette == 2) ia.a((int[])null);
        else if (palette == 3) ia.a(new int[0]);
        qm particle = new qm(fault == 1 ? null : world, 8,64,8,0,0,0,uu.Y,0,7);
        Object beforeA = manager.a, beforeB = manager.b, beforeD = field(xv.class,"d").get(manager);
        long beforeRandom = state(world.r);
        xa dimension = world.t;
        if (fault == 2) field(fd.class,"t").set(world,null);
        if (fault == 3) field(fd.class,"t").set(world,new rh());
        String status = "OK"; int color = 0x12345678; boolean same = false;
        try {
            if (mode == 0) color = uu.Y.b((xp)(fault == 1 ? null : world),x,y,z);
            else same = particle.a(x,y,z) == particle;
        } catch (NullPointerException e) { status = "NULL"; }
        catch (ArrayIndexOutOfBoundsException e) { status = "INDEX"; }
        finally { field(fd.class,"t").set(world,dimension); }
        System.out.println("TINT\t"+index+"\t"+mode+"\t"+metadata+"\t"+x+"\t"+y+"\t"+z+
            "\t"+fault+"\t"+palette+"\t"+status+"\t"+String.format("%08x",color)+
            "\t"+f(particle.i)+"\t"+f(particle.j)+"\t"+f(particle.k)+"\t"+(same?1:0)+
            "\t"+(staticRaw(ia.class,"a")==null?0:1)+"\t"+(staticRaw(jh.class,"a")==null?0:1)+
            "\t"+values(manager.a)+"\t"+values(manager.b)+
            "\t"+(manager.a==beforeA?1:0)+"\t"+(manager.b==beforeB?1:0)+
            "\t"+(field(xv.class,"d").get(manager)==beforeD?1:0)+
            "\t"+(state(world.r)==beforeRandom?1:0));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length==3,"JAR, saved World and profiles"); Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(), directory=Path.of(args[1]).toRealPath();
        Socket connect=new Socket(); ServerSocket listen=new ServerSocket();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h,int p) { ++connects;throw new SecurityException(); }
            @Override public void checkListen(int p) { ++listens;throw new SecurityException(); }
            @Override public void checkLink(String lib) { ++links;throw new SecurityException(); }
            @Override public void checkWrite(String file) {
                require(Path.of(file).toAbsolutePath().normalize().startsWith(directory),"private World writes");
            }
        });
        for (Class<?> c:new Class<?>[]{ru.class,wb.class,uu.class,qm.class,xw.class,sn.class,fd.class,
                lm.class,xv.class,xa.class,ia.class,jh.class,rh.class})
            require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original class origin");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        xv manager=world.a();manager.a(0,0,1,1); // Declared preparation, not an expected-color substitute.
        int rows=0;for(String line:Files.readAllLines(Path.of(args[2]))) {observe(world,manager,line);++rows;}
        require(rows==28 && connects==0 && listens==0 && links==0,"complete original rows and zero source attempts");
        System.out.println("GUARD\tsource_connect\t0\nGUARD\tsource_listen\t0\nGUARD\tsource_link\t0");
        try {connect.connect(new InetSocketAddress("127.0.0.1",9));throw new AssertionError("connect allowed");} catch(SecurityException expected) {}
        try {listen.bind(new InetSocketAddress("127.0.0.1",0));throw new AssertionError("listen allowed");} catch(SecurityException expected) {}
        try {System.loadLibrary("tall_grass_tint_denied");throw new AssertionError("link allowed");} catch(SecurityException expected) {}
        require(connects==1 && listens==1 && links==1,"three positive denial controls");
        System.out.println("GUARD\tconnect_denied\t1\nGUARD\tlisten_denied\t1\nGUARD\tlink_denied\t1");
        connect.close();listen.close();System.out.println("Original TallGrass tint: 28 rows");
    }
}
