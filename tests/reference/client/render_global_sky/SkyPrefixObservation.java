import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import sun.reflect.ReflectionFactory;

/** Four actual n.a(F) early paths, before any GL operation. */
public final class SkyPrefixObservation {
    static int network,natives;
    static void require(boolean b){if(!b)throw new AssertionError("sky prefix invariant");}
    static Object raw(Class<?> type)throws Exception {
        Constructor<?> c=ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type,Object.class.getDeclaredConstructor());
        c.setAccessible(true);return c.newInstance();
    }
    static void set(Object object,Class<?> type,String name,Object value)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);f.set(object,value);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==1);Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkLink(String l){++natives;throw new SecurityException();}
            @Override public void checkWrite(String f){throw new SecurityException();}
        });
        for(Class<?> type:new Class<?>[]{n.class,kq.class,fd.class,xa.class,wd.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar));
        for(int mode=0;mode<4;++mode) {
            n renderer=(n)raw(n.class);kq app=(kq)raw(kq.class);fd world=(fd)raw(fd.class);
            xa dimension=new wd();
            dimension.c=true; // Explicit prepared source flag; no bind/biome claim.
            set(renderer,n.class,"t",mode==0?null:app);
            app.f=mode>=2?world:null;
            set(world,fd.class,"t",mode==3?dimension:null);
            boolean returned=false;
            try {renderer.a(.375f);returned=true;}
            catch(NullPointerException expected){
                require(mode<3&&expected.getStackTrace()[0].getClassName().equals("n")&&
                    expected.getStackTrace()[0].getMethodName().equals("a"));
            }
            require(returned==(mode==3));
            System.out.printf("SKY_PREFIX %d %d%n",mode,returned?1:0);
        }
        require(network==0&&natives==0);
        System.out.println("Original sky prefix: 4 rows; network=0; native=0");
    }
}
