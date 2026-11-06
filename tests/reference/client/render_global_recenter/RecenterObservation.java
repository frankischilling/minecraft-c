import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;

/** Prepared actual fields; every called game method retains its original Code. */
public final class RecenterObservation {
    static dk[] actors;
    static int network, nativeLinks;
    static Field field(Class<?> type, String name) throws Exception {
        Field f=type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> u=Class.forName("sun.misc.Unsafe");
        return u.getMethod("allocateInstance",Class.class).invoke(field(u,"theUnsafe").get(null),type);
    }
    static int id(Object value) {
        if(value==null)return 0;
        for(int i=0;i<actors.length;++i)if(value==actors[i])return i+1;
        throw new AssertionError("foreign source identity");
    }
    static String ids(Object[] values) {
        if(values==null)return "N";
        StringJoiner s=new StringJoiner(",");for(Object value:values)s.add(""+id(value));
        return s.length()==0?"-":s.toString();
    }
    static String captured(jo comparator) throws Exception {
        if(comparator==null)return "N";
        return String.format(Locale.ROOT,"%016x,%016x,%016x",
            Double.doubleToRawLongBits(field(jo.class,"a").getDouble(comparator)),
            Double.doubleToRawLongBits(field(jo.class,"b").getDouble(comparator)),
            Double.doubleToRawLongBits(field(jo.class,"c").getDouble(comparator)));
    }
    static void run(Scanner in) throws Exception {
        String kind=in.next();int index=in.nextInt();String name=in.next();boolean cameraPresent=in.nextInt()!=0;
        double[] coordinates=new double[3];for(int i=0;i<3;++i)coordinates[i]=Double.longBitsToDouble(Long.parseUnsignedLong(in.next(),16));
        int p=in.nextInt(),q=in.nextInt(),r=in.nextInt(),x=in.nextInt(),y=in.nextInt(),z=in.nextInt();
        int count=in.nextInt(),length=in.nextInt(),queued=in.nextInt();actors=new dk[count];
        for(int i=0;i<count;++i){dk d=(dk)raw(dk.class);actors[i]=d;d.c=in.nextInt();d.d=in.nextInt();d.e=in.nextInt();d.q=in.nextInt();d.r=in.nextInt();d.s=in.nextInt();d.u=in.nextInt()!=0;}
        dk[] array=length<0?null:new dk[length];for(int i=0;i<Math.max(0,length);++i){int id=in.nextInt();array[i]=id==0?null:actors[id-1];}
        List<dk> queue=new ArrayList<>();for(int i=0;i<queued;++i){int id=in.nextInt();queue.add(id==0?null:actors[id-1]);}
        if(kind.equals("JO")){
            dc camera=null;if(cameraPresent){camera=(dc)raw(dc.class);camera.aM=coordinates[0];camera.aN=coordinates[1];camera.aO=coordinates[2];}
            jo source=null;String exception="NONE";StringJoiner trace=new StringJoiner(",");
            try {
                source=new jo(camera);final jo comparator=source;
                Arrays.sort(array,new Comparator<dk>(){public int compare(dk left,dk right){
                    try{int value=comparator.a(left,right);trace.add(id(left)+":"+id(right)+":"+value);return value;}
                    catch(RuntimeException e){trace.add(id(left)+":"+id(right)+":X");throw e;}
                }});
            } catch(NullPointerException e){exception="NPE";}catch(IllegalArgumentException e){exception="IAE";}
            System.out.println("JO "+index+" "+name+" "+exception+" "+captured(source)+" "+ids(array)+" "+(trace.length()==0?"-":trace));
        } else {
            n source=(n)raw(n.class);for(String cell:new String[]{"p","q","r"})field(n.class,cell).setInt(source,cell.equals("p")?p:cell.equals("q")?q:r);
            field(n.class,"o").set(source,array);field(n.class,"m").set(source,queue);
            Method method=n.class.getDeclaredMethod("b",int.class,int.class,int.class);method.setAccessible(true);
            String exception="NONE";
            try{method.invoke(source,x,y,z);}catch(InvocationTargetException e){Throwable cause=e.getCause();if(cause instanceof NullPointerException)exception="NPE";else if(cause instanceof ArrayIndexOutOfBoundsException)exception="AIOOBE";else if(cause instanceof ArithmeticException)exception="ARITH";else throw e;}
            StringJoiner bounds=new StringJoiner(" ");for(String cell:new String[]{"B","C","D","E","F","G"})bounds.add(""+field(n.class,cell).getInt(source));
            StringJoiner state=new StringJoiner(",");for(dk d:actors)state.add(d.c+":"+d.d+":"+d.e+":"+(d.u?1:0));
            System.out.println("REC "+index+" "+name+" "+exception+" "+bounds+" "+ids(array)+" "+ids(queue.toArray())+" "+(state.length()==0?"-":state));
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[0]).toRealPath();String input=Files.readString(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}public void checkListen(int p){++network;throw new SecurityException("listen denied");}public void checkWrite(String p){throw new SecurityException("writes denied");}public void checkLink(String p){++nativeLinks;throw new SecurityException("native denied");}});
        for(Class<?> c:new Class<?>[]{n.class,dk.class,jo.class})if(!Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new AssertionError("foreign game class");
        Scanner in=new Scanner(input);in.useLocale(Locale.ROOT);int count=0;while(in.hasNext()){run(in);++count;}
        if(count!=31||network!=0||nativeLinks!=0)throw new AssertionError("capture scope/guard differs");
        System.out.println("Original recenter/JO: 31 rows; untouched game methods; network=0; native=0");
    }
}
