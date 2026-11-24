import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import net.minecraft.client.Minecraft;

/** Prepared real identities; the invoked whole N method is unchanged. */
public final class SubmitObservation {
    static int index=-1, network, nativeLinks;
    static dk[] actors;
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object raw(Class<?> type)throws Exception{
        Class<?> u=Class.forName("sun.misc.Unsafe");
        return u.getMethod("allocateInstance",Class.class).invoke(field(u,"theUnsafe").get(null),type);
    }
    static int id(Object value){if(value==null)return 0;for(int i=0;i<actors.length;++i)if(value==actors[i])return i+1;throw new AssertionError("foreign DK");}
    static String ids(List<dk> list){if(list==null)return "N";StringJoiner s=new StringJoiner(",");for(dk d:list)s.add(""+id(d));return s.length()==0?"-":s.toString();}
    static void run(Scanner input)throws Exception{
        index=input.nextInt();String name=input.next();int cursor=input.nextInt();boolean appPresent=input.nextInt()!=0,settingsPresent=input.nextInt()!=0;
        int distance=input.nextInt(),oldDistance=input.nextInt(),pass=input.nextInt(),count=input.nextInt(),length=input.nextInt(),queued=input.nextInt();
        actors=new dk[count];for(int i=0;i<count;++i){actors[i]=(dk)raw(dk.class);actors[i].u=input.nextInt()!=0;}
        dk[] array=length<0?null:new dk[length];for(int i=0;i<Math.max(length,0);++i){int id=input.nextInt();array[i]=id==0?null:actors[id-1];}
        List<dk> queue=queued<0?null:new ArrayList<>();for(int i=0;i<Math.max(queued,0);++i){int id=input.nextInt();queue.add(id==0?null:actors[id-1]);}
        n source=(n)raw(n.class);field(n.class,"o").set(source,array);field(n.class,"m").set(source,queue);
        field(n.class,"R").setInt(source,cursor);field(n.class,"H").setInt(source,oldDistance);
        String[] counters={"M","N","O","P","Q"};for(int i=0;i<5;++i)field(n.class,counters[i]).setInt(source,(i+1)*11);
        if(appPresent){Minecraft app=(Minecraft)raw(kq.class);field(n.class,"t").set(source,app);if(settingsPresent){kv settings=(kv)raw(kv.class);settings.e=distance;settings.j=true;app.z=settings;}}
        String exception="NONE";try{source.a((ls)null,pass,.5);}catch(NullPointerException e){exception="NPE";}catch(ArrayIndexOutOfBoundsException e){exception="AIOOBE";}catch(ArithmeticException e){exception="ARITH";}
        if(exception.equals("NONE"))throw new AssertionError("pre-U failure did not happen");
        StringJoiner values=new StringJoiner(" ");for(String cell:counters)values.add(""+field(n.class,cell).getInt(source));
        if(field(n.class,"m").get(source)!=queue||field(n.class,"o").get(source)!=array)throw new AssertionError("source identities replaced");
        System.out.println("OBS "+index+" "+name+" "+exception+" "+field(n.class,"R").getInt(source)+" "+ids(queue)+" "+values+" "+field(n.class,"H").getInt(source));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        Path jar=Path.of(args[0]).toRealPath();String text=Files.readString(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}public void checkListen(int p){++network;throw new SecurityException("listener denied");}public void checkWrite(String p){throw new SecurityException("write denied");}public void checkLink(String p){++nativeLinks;throw new SecurityException("native denied");}});
        for(Class<?> c:new Class<?>[]{n.class,dk.class,Minecraft.class,kq.class,kv.class})if(!Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new AssertionError("foreign original class");
        Scanner input=new Scanner(text);input.useLocale(Locale.ROOT);int count=0;while(input.hasNext()){run(input);++count;}
        if(count!=12||network!=0||nativeLinks!=0)throw new AssertionError("capture guard/count");
        System.out.println("Original submission CPU: 12 rows; network=0; native=0");
    }
}
