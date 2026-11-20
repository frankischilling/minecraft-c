import java.lang.reflect.*;
import java.nio.FloatBuffer;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicLong;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;
public final class LightingObservation {
    static int network,nativeLinks;
    static ByteBuffer held;
    static void require(boolean b,String m){if(!b)throw new AssertionError(m);}
    static Object read(Class<?> c,String name)throws Exception {Field f=c.getDeclaredField(name);f.setAccessible(true);return f.get(null);}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Path jar=Path.of(args[0]).toRealPath();int mode=Integer.parseInt(args[1]);
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}public void checkListen(int p){++network;throw new SecurityException("listen denied");}public void checkWrite(String p){throw new SecurityException("write denied");}public void checkLink(String p){++nativeLinks;throw new SecurityException("native denied");}});
        Class<?> type=Class.forName("u",false,LightingObservation.class.getClassLoader());Class<?> allocation=Class.forName("ge",false,LightingObservation.class.getClassLoader());
        for(Class<?> c:new Class<?>[]{type,allocation})require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");
        Class<?> unsafeClass=Class.forName("sun.misc.Unsafe");Field sf=unsafeClass.getDeclaredField("theUnsafe");sf.setAccessible(true);Object unsafe=sf.get(null);Method cold=unsafeClass.getMethod("shouldBeInitialized",Class.class);
        require((boolean)cold.invoke(unsafe,type)&&(boolean)cold.invoke(unsafe,allocation),"source initialized eagerly");
        if(mode==2){Field total=Class.forName("java.nio.Bits").getDeclaredField("TOTAL_CAPACITY");Object base=unsafeClass.getMethod("staticFieldBase",Field.class).invoke(unsafe,total);long offset=(long)unsafeClass.getMethod("staticFieldOffset",Field.class).invoke(unsafe,total);long used=((AtomicLong)unsafeClass.getMethod("getObject",Object.class,long.class).invoke(unsafe,base,offset)).get();require(used<1048576-32,"direct memory prerequisite");held=ByteBuffer.allocateDirect((int)(1048576-used-32));boolean first=false,second=false;try{Class.forName("u");}catch(OutOfMemoryError e){boolean source=false;for(StackTraceElement frame:e.getStackTrace())if(frame.getClassName().equals("u")&&frame.getMethodName().equals("<clinit>"))source=true;require(source,"foreign allocation failure");first=true;}try{Class.forName("u");}catch(NoClassDefFoundError e){second=true;}require(first&&second,"source class failure not retained");require(((List<?>)read(allocation,"a")).isEmpty()&&((List<?>)read(allocation,"b")).isEmpty(),"ge prefix missing");System.out.println("LIGHTING_FAILURE 1 1 1");}
        else {FloatBuffer eager=mode==1?ge.e(16):null;Class.forName("u");FloatBuffer buffer=(FloatBuffer)read(type,"a");require(buffer!=eager&&buffer.isDirect()&&buffer.capacity()==16&&buffer.limit()==16&&buffer.position()==0,"actual u buffer");for(int i=0;i<16;++i)require(Float.floatToRawIntBits(buffer.get(i))==0,"initial direct bytes");require(read(type,"a")==buffer,"actual static identity");System.out.printf("LIGHTING %d %d %d %d %d %d 1%n",mode,buffer.capacity(),buffer.position(),buffer.limit(),((List<?>)read(allocation,"a")).size(),((List<?>)read(allocation,"b")).size());}
        require(network==0&&nativeLinks==0,"external attempt");System.out.println("Original lighting: mode="+mode+"; network=0; native=0");
    }
}
