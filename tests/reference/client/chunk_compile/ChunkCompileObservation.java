import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Locale;

/** Actual complete empty dk.a(), genuine World/Chunks and prepared DK inputs. */
public final class ChunkCompileObservation {
    static int network,nativeLinks;
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static Object raw(Class<?> type)throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance",Class.class).invoke(field(unsafe,"theUnsafe").get(null),type);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==2,"JAR and private authored World");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkLink(String lib){++nativeLinks;throw new SecurityException();}
            @Override public void checkWrite(String f){require(Path.of(f).toAbsolutePath().normalize().startsWith(directory),"private World writes only");}
        });
        for(Class<?> type:new Class<?>[]{dk.class,cv.class,ew.class,fd.class,lm.class,nw.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original class");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm actual=world.c(0,0);
        dk chunk=(dk)raw(dk.class);
        chunk.a=world;chunk.c=1;chunk.d=128;chunk.e=1;chunk.f=chunk.g=chunk.h=2;
        chunk.p=new boolean[2];chunk.A=true;field(dk.class,"E").setBoolean(chunk,false);chunk.u=true;
        ArrayList<Object> old=new ArrayList<>(),all=new ArrayList<>();
        field(dk.class,"B").set(chunk,old);field(dk.class,"F").set(chunk,all);
        lm.a=true;int before=dk.b;
        chunk.a(); // Whole unchanged body, including actual ew and cv constructors.
        require(world.c(0,0)==actual,"same genuine cached Chunk identity");
        System.out.printf("DK_EMPTY 1 %d %d %d %d %d %d %d %d%n",chunk.p[0]?1:0,
            chunk.p[1]?1:0,chunk.A?1:0,field(dk.class,"E").getBoolean(chunk)?1:0,chunk.u?1:0,dk.b-before,old.size(),all.size());
        require(network==0&&nativeLinks==0,"no network or native/GL link reached");
        System.out.println("Original DK: 1 empty whole-body row; network=0; native=0");
    }
}
