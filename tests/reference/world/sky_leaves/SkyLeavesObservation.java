import java.awt.Color;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Construct real XV/UG/CC and EI; only the seed-carrying FD is prepared. */
public final class SkyLeavesObservation {
    static final long[] SEEDS = {0,-1,Long.MIN_VALUE,Long.MAX_VALUE};
    static final int[][] COORDS = {{0,0},{-17,31},{Integer.MIN_VALUE,Integer.MAX_VALUE},
        {Integer.MAX_VALUE,Integer.MIN_VALUE},{30000000,-30000000}};
    static final int[] TEMPS = {0,0x80000000,1,0x80000001,0x00800000,0x80800000,
        0x3f000000,0x3f800000,0xbf800000,0x403fffff,0x40400000,0x40400001,
        0xc03fffff,0xc0400000,0xc0400001,0x7f7fffff,0xff7fffff,0x7f800000,
        0xff800000,0x7fc12345,0xffc12345,0x7fa12345,0xffa12345};
    static final int[][] HSB = {
        {0,0,0x3f800000},{0x7fc12345,0,0x3f800000},{0x7f800000,0,0x80000000},
        {0,0x3f800000,0x3f800000},{0x3e2aaaab,0x3f800000,0x3f800000},
        {0x3eaaaaab,0x3f800000,0x3f800000},{0x3f000000,0x3f800000,0x3f800000},
        {0x3f2aaaab,0x3f800000,0x3f800000},{0x3f555555,0x3f800000,0x3f800000},
        {0x3f800000,0x3f800000,0x3f800000},{0x3f000000,0x3f000000,0x3f000000},
        {0xbdcccccd,0x3f000000,0x3f800000},{0x3f8ccccd,0x3f000000,0x3f800000},
        {0x3e99999a,0xbf800000,0x40000000},{0x3e99999a,0x40000000,0xbf800000},
        {0x7fc12345,0x3f000000,0x3f800000},{0x3e99999a,0x7fc12345,0x3f800000},
        {0x3e99999a,0x3f000000,0x7fc12345},{0x7f800000,0x3f000000,0x3f800000},
        {0xff800000,0x3f000000,0x3f800000},{0x3e99999a,0x7f800000,0x3f800000},
        {0x3e99999a,0x3f000000,0x7f800000},{0x3e99999a,0x3f000000,0xff800000},
        {0,0x3f000000,1},{0x7f7fffff,0x3f000000,0x7f7fffff}};
    static int network, natives;
    static void require(boolean b) { if (!b) throw new AssertionError("sky leaves invariant"); }
    static Object prepared(Class<?> type) throws Exception {
        Constructor<?> c = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type,Object.class.getDeclaredConstructor());
        c.setAccessible(true); return c.newInstance();
    }
    static Field field(Class<?> type,String name) throws Exception {
        Field f=type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length==1); Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath();
        // Color's platform bootstrap is outside the loaded arithmetic leaf.
        require(Color.HSBtoRGB(0,0,1)==0xffffffff);
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h,int p) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int p) { ++network; throw new SecurityException(); }
            @Override public void checkLink(String l) { ++natives; throw new SecurityException(); }
            @Override public void checkWrite(String f) { throw new SecurityException(); }
        });
        for (Class<?> type:new Class<?>[]{xv.class,ug.class,cc.class,fd.class,ei.class,kd.class,hd.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar));
        for (int s=0;s<SEEDS.length;++s) for(int c=0;c<COORDS.length;++c)
            for(int mode=0;mode<3;++mode) {
                fd world=(fd)prepared(fd.class);
                field(fd.class,"x").set(world,new ei(SEEDS[s],"Sky leaves"));
                xv manager=new xv(world);
                manager.a=mode==0?null:new double[mode==1?0:5];
                if(manager.a!=null) Arrays.fill(manager.a,-7.5);
                double[] old=manager.a;
                manager.b=new double[]{9.25,10.25}; manager.c=new double[]{11.5};
                double[] b=manager.b,aux=manager.c;
                double answer=manager.b(COORDS[c][0],COORDS[c][1]);
                require(manager.b==b&&manager.c==aux&&manager.d==null&&b[0]==9.25&&aux[0]==11.5);
                System.out.printf("RAW %d %d %d %016x %d %d",s,c,mode,
                    Double.doubleToRawLongBits(answer),manager.a==old?1:0,manager.a.length);
                for(double v:manager.a)System.out.printf(" %016x",Double.doubleToRawLongBits(v));
                System.out.println();
            }
        kd[] biomes={kd.a,kd.b,kd.c,kd.d,kd.e,kd.f,kd.g,kd.h,kd.i,kd.j,kd.k,kd.l,kd.m};
        for(int b=0;b<biomes.length;++b) for(int t=0;t<TEMPS.length;++t)
            System.out.printf("SKY %d %d %08x %08x%n",b,t,TEMPS[t],
                biomes[b].a(Float.intBitsToFloat(TEMPS[t])));
        for(int i=0;i<HSB.length;++i) {
            int[] v=HSB[i];
            System.out.printf("HSB %d %08x %08x %08x %08x%n",i,v[0],v[1],v[2],
                Color.HSBtoRGB(Float.intBitsToFloat(v[0]),Float.intBitsToFloat(v[1]),Float.intBitsToFloat(v[2])));
        }
        for(int f=0;f<2;++f) {
            hd fixed=new hd(f==0?kd.l:kd.m,f==0?1:.5,0);
            fixed.a=new double[]{4,5}; double[] old=fixed.a;
            double answer=fixed.b(7,-9);
            require(fixed.a==old&&fixed.a[0]==4&&fixed.a[1]==5);
            System.out.printf("RAWFIX %d %016x %016x %016x%n",f,Double.doubleToRawLongBits(answer),
                Double.doubleToRawLongBits(fixed.a[0]),Double.doubleToRawLongBits(fixed.a[1]));
        }
        require(network==0&&natives==0);
        System.out.println("Original sky leaves: 386 rows; network=0; native=0");
    }
}
