import java.lang.reflect.*;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;

/** Unchanged n.e/dk.f bodies on explicitly prepared original source fields. */
public final class SkyChangedObservation {
    static int network, nativeLinks;
    static void require(boolean b,String m) { if(!b)throw new AssertionError(m); }
    static Field field(Class<?> c,String n)throws Exception { Field f=c.getDeclaredField(n);f.setAccessible(true);return f; }
    static Object raw(Class<?> c)throws Exception {
        Class<?> rf=Class.forName("sun.reflect.ReflectionFactory");Object f=rf.getMethod("getReflectionFactory").invoke(null);
        Constructor<?> ctor=(Constructor<?>)rf.getMethod("newConstructorForSerialization",Class.class,Constructor.class).invoke(f,c,Object.class.getDeclaredConstructor());
        return ctor.newInstance();
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission p) {}
            public void checkConnect(String h,int p) { ++network;throw new SecurityException("network denied"); }
            public void checkListen(int p) { ++network;throw new SecurityException("listener denied"); }
            public void checkWrite(String p) { throw new SecurityException("write denied"); }
            public void checkLink(String p) { ++nativeLinks;throw new SecurityException("native denied"); }
        });
        for(Class<?> c:new Class<?>[]{n.class,dk.class})require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign source class");
        for(int mode=0;mode<8;++mode) {
            n renderer=(n)raw(n.class);dk[] all=new dk[12];
            for(int i=0;i<all.length;++i)all[i]=(dk)raw(dk.class);
            dk[] cells=all;List<dk> queue=new ArrayList<>();
            if(mode==0)for(int i=0;i<12;++i) { all[i].A=i%2!=0;all[i].u=i%3==0; }
            if(mode==1) { cells=new dk[]{all[1],all[1],all[3],all[2],all[1],all[3]};all[1].A=all[2].A=all[3].A=true; }
            if(mode==2) { cells=new dk[0];queue=null; }
            if(mode==3)cells=null;
            if(mode==4) { cells=new dk[]{all[0],all[1],null,all[2]};all[0].A=all[1].A=all[2].A=true; }
            if(mode==5) { cells=new dk[]{all[0],all[1]};all[1].A=true;queue=null; }
            if(mode==6) { cells=new dk[]{all[0],all[1]};queue=null; }
            if(mode==7) { cells=new dk[]{all[0]};all[0].A=all[0].u=true;queue=null; }
            field(n.class,"o").set(renderer,cells);field(n.class,"m").set(renderer,queue);
            boolean failed=false;try { renderer.e(); } catch(NullPointerException e) { failed=true;require(e.getStackTrace()[0].getClassName().equals("n")&&e.getStackTrace()[0].getMethodName().equals("e"),"foreign failure"); }
            int mask=0,code=0;for(int i=0;i<12;++i)if(all[i].u)mask|=1<<i;
            if(queue!=null)for(dk value:queue) { int id=-1;for(int i=0;i<12;++i)if(all[i]==value)id=i;require(id>=0,"foreign DK publication");code=code*17+id+1; }
            System.out.printf("SKY_CHANGED %d %d %d %d %d%n",mode,failed?1:0,queue==null?-1:queue.size(),mask,code);
        }
        require(network==0&&nativeLinks==0,"external attempt");
        System.out.println("Original sky change: 8 rows; network=0; native=0; prepared-N-DK=1");
    }
}
