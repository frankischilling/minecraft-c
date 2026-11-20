import java.lang.reflect.*;
import java.nio.file.Path;
import java.util.*;
import org.lwjgl.opengl.*;

/** Original sr/W constructors and n/dk culling on explicitly prepared N/DK fields. */
public final class CullingObservation {
    static void require(boolean b, String message) { if (!b) throw new AssertionError(message); }
    static Field field(Class<?> c, String n) throws Exception { Field f=c.getDeclaredField(n); f.setAccessible(true); return f; }
    static Object raw(Class<?> c) throws Exception {
        Class<?> rf=Class.forName("sun.reflect.ReflectionFactory");
        Object factory=rf.getMethod("getReflectionFactory").invoke(null);
        Constructor<?> ctor=(Constructor<?>)rf.getMethod("newConstructorForSerialization",Class.class,Constructor.class)
            .invoke(factory,c,Object.class.getDeclaredConstructor());
        return ctor.newInstance();
    }
    static void matrices() { GL11.glMatrixMode(5889); GL11.glLoadIdentity(); GL11.glMatrixMode(5888); GL11.glLoadIdentity(); }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[0]).toRealPath(), natives=Path.of(args[1]).toRealPath();
        final int[] network={0}; final List<String> links=new ArrayList<>();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(java.security.Permission p) {}
            public void checkConnect(String h,int p) { ++network[0]; throw new SecurityException("network denied"); }
            public void checkListen(int p) { ++network[0]; throw new SecurityException("listener denied"); }
            public void checkWrite(String p) { throw new SecurityException("write denied"); }
            public void checkLink(String p) { links.add(p); if (!Path.of(p).isAbsolute() || !Path.of(p).normalize().equals(natives.resolve("lwjgl64.dll"))) throw new SecurityException("native denied"); }
        });
        for (Class<?> c:new Class<?>[]{sr.class,n.class,dk.class,w.class,dr.class,eq.class})
            require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign class");
        Display.setDisplayMode(new DisplayMode(64,64));
        try {
            Display.create(); matrices();
            double[][] centers={{0,0,0},{10,-3,5},{-0.0,Double.longBitsToDouble(1),0},{-16,2,17}};
            for (int mode=0; mode<centers.length; ++mode) {
                sr clip=new sr(); double[] v=centers[mode]; clip.a(v[0],v[1],v[2]);
                require(field(sr.class,"a").get(clip)==field(w.class,"e").get(null),"canonical W");
                int mask=0;
                double[][] boxes={{v[0]-.5,v[1]-.5,v[2]-.5,v[0]+.5,v[1]+.5,v[2]+.5},{v[0]+2,v[1],v[2],v[0]+3,v[1]+.5,v[2]+.5},{v[0]-1,v[1]-1,v[2]-1,v[0]+1,v[1]+1,v[2]+1}};
                for(int i=0;i<boxes.length;++i) { double[] b=boxes[i]; boolean direct=clip.a(b[0],b[1],b[2],b[3],b[4],b[5]); require(direct==clip.a(eq.a(b[0],b[1],b[2],b[3],b[4],b[5])),"bounds overload"); if(direct)mask|=1<<i; }
                System.out.printf("WRAP %d %016x %016x %016x %d%n",mode,Double.doubleToRawLongBits(field(sr.class,"b").getDouble(clip)),Double.doubleToRawLongBits(field(sr.class,"c").getDouble(clip)),Double.doubleToRawLongBits(field(sr.class,"d").getDouble(clip)),mask);
            }
            sr clip=new sr(); clip.a(0,0,0);
            for(int mode=0;mode<4;++mode) {
                n renderer=(n)raw(n.class); dk[] cells=new dk[20];
                for(int i=0;i<cells.length;++i) {
                    cells[i]=(dk)raw(dk.class); boolean empty=i%5==0;
                    field(dk.class,"E").setBoolean(cells[i],empty);
                    cells[i].p=empty?new boolean[]{true,true}:null;
                    cells[i].o=i%3==0;
                    double x=i%2==0?3:0; cells[i].v=eq.a(x-.25,-.25,-.25,x+.25,.25,.25);
                }
                field(n.class,"o").set(renderer,cells);
                int j=new int[]{0,4,10,Integer.MAX_VALUE}[mode]; field(n.class,"j").setInt(renderer,j);
                renderer.a(clip,Float.intBitsToFloat(0x7fc12345));
                int mask=0; for(int i=0;i<cells.length;++i)if(cells[i].o)mask|=1<<i;
                System.out.printf("CULL %d %d %d %d%n",mode,j,field(n.class,"j").getInt(renderer),mask);
            }
            require(GL11.glGetError()==0 && network[0]==0,"external/GL failure");
            System.out.println("Original culling: 8 rows; network=0; display=1; prepared-N-DK=1");
            for(String link:links)System.out.println("NATIVE_LINK "+link);
        } finally { Display.destroy(); }
    }
}
