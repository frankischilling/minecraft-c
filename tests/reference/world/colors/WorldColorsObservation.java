import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;

/** Unchanged World sky/cloud/fog with genuine Dimension/biome/Info owners. */
public final class WorldColorsObservation {
    static final long[] TIMES = {-24001L, -1L, 0L, 6000L, 18000L, Long.MAX_VALUE};
    static final float[] PARTIAL = {0.0f, 0.625f};
    static final long[] CLOUD = {0xffffffL, 0x804020L, -1L, 0x123456789abcdef0L};
    static final float[][] WEATHER = {{0,0,0,0}, {0.2f,0.8f,0.1f,0.6f},
                                      {1,1,1,1}, {-0.25f,0.75f,1,0.1f}};
    static final double[][] POSITION = {{-1.25,2.75}, {128.75,-16.25},
                                        {-32000000.5,17.5}, {32000000.25,-2048.5}};
    static int network, nativeLinks;

    static void require(boolean value) {
        if (!value) throw new AssertionError("original World color invariant");
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }

    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
                     .invoke(field(unsafe,"theUnsafe").get(null), type);
    }

    static void row(int dimension, int index, String kind, bt value) throws Exception {
        System.out.printf("COLOR %d %d %s %016x %016x %016x %d%n", dimension,index,kind,
            Double.doubleToRawLongBits(value.a), Double.doubleToRawLongBits(value.b),
            Double.doubleToRawLongBits(value.c), field(bt.class,"e").getInt(null));
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length==1);
        Path jar=Path.of(arguments[0]).toRealPath();
        Class.forName("java.awt.Color"); // Host AWT bootstrap is outside measured calls.
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) {
                ++network; throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("observer denies listeners");
            }
            @Override public void checkLink(String library) {
                ++nativeLinks; throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
        });
        for(Class<?> type:new Class<?>[]{fd.class,rh.class,wd.class,ay.class,xv.class,kd.class,bt.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar));
        fd world=(fd)raw(fd.class);
        ei info=new ei(23L,"Colors");
        world.x=info;
        dc camera=(dc)raw(dc.class);
        bt old=null;
        for(int dim=0;dim<3;++dim) {
            xa dimension=dim==0?new rh():dim==1?new wd():new ay();
            field(fd.class,"t").set(world,dimension);
            dimension.a(world);
            for(int time=0;time<TIMES.length;++time) {
                info.a(TIMES[time]);
                for(int weather=0;weather<4;++weather) {
                    world.i=WEATHER[weather][0];world.j=WEATHER[weather][1];
                    world.k=WEATHER[weather][2];world.l=WEATHER[weather][3];
                    world.n=weather==2?2:weather==3?1:0;
                    field(fd.class,"H").setLong(world,CLOUD[weather]);
                    camera.aM=POSITION[weather][0];camera.aN=64.0;camera.aO=POSITION[weather][1];
                    for(int partial=0;partial<PARTIAL.length;++partial) {
                        int index=time*8+weather*2+partial;
                        bt.b();
                        bt sky=world.a(camera,PARTIAL[partial]);
                        if(old!=null)require(old==sky);
                        old=sky;
                        row(dim,index,"S",sky);
                        row(dim,index,"C",world.c(PARTIAL[partial]));
                        row(dim,index,"F",world.d(PARTIAL[partial]));
                    }
                }
            }
        }
        require(network==0&&nativeLinks==0);
        System.out.println("Original World colors: 432 rows; network=0; native=0");
    }
}
