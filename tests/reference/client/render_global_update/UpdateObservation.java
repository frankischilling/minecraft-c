import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Prepared genuine receivers; every invoked game method is unchanged. */
@SuppressWarnings({"unchecked","removal"})
public final class UpdateObservation {
    static String kind, name, row;
    static int index;
    static dk[] actors;
    static int network, nativeLinks;

    static void require(boolean value) {
        if (!value) throw new AssertionError("original update observer invariant");
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field f=type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> u=Class.forName("sun.misc.Unsafe");
        return u.getMethod("allocateInstance",Class.class).invoke(field(u,"theUnsafe").get(null),type);
    }
    static void beginCase() {}
    static void endCase() {}
    static int id(Object value) {
        if (value==null) return 0;
        for (int i=0;i<actors.length;++i) if (value==actors[i]) return i+1;
        throw new AssertionError("unexpected actor identity");
    }
    static String ids(List<?> values) {
        if (values==null) return "N";
        if (values.isEmpty()) return "-";
        StringBuilder out=new StringBuilder();
        for (Object value:values) {if(out.length()!=0)out.append(',');out.append(id(value));}
        return out.toString();
    }
    static String fields(int which) throws Exception {
        if (actors.length==0) return "-";
        StringBuilder out=new StringBuilder();
        for(dk d:actors) {
            if(out.length()!=0)out.append(',');
            int value=which==0?(d.u?1:0):which==1?((d.p[0]?1:0)|(d.p[1]?2:0)):
                which==2?(field(dk.class,"E").getBoolean(d)?1:0):which==3?(d.A?1:0):d.B.size();
            out.append(value);
        }
        return out.toString();
    }
    static ls camera(String text) throws Exception {
        if(text.equals("N"))return null;
        String[] v=text.split(",");dc value=(dc)raw(dc.class);
        value.aM=Double.longBitsToDouble(Long.parseUnsignedLong(v[0],16));
        value.aN=Double.longBitsToDouble(Long.parseUnsignedLong(v[1],16));
        value.aO=Double.longBitsToDouble(Long.parseUnsignedLong(v[2],16));return value;
    }
    static void run(String line) throws Exception {
        String[] cells=line.split("\\t",-1);kind=cells[0];index=Integer.parseInt(cells[1]);name=cells[2];
        ls camera=camera(cells[5]);String[] specs=cells[7].isEmpty()?new String[0]:cells[7].split(",");
        actors=new dk[specs.length];
        for(int i=0;i<actors.length;++i) {
            int[] s=Arrays.stream(specs[i].split(":")).mapToInt(Integer::parseInt).toArray();
            dk d=(dk)raw(dk.class);d.q=s[0];d.r=s[1];d.s=s[2];d.w=s[3];d.o=s[4]!=0;d.u=s[5]!=0;
            int mask=(i+1)%4;d.p=new boolean[]{(mask&1)!=0,(mask&2)!=0};
            field(dk.class,"E").setBoolean(d,(i+1)%2!=0);d.A=true;d.f=d.g=d.h=16;
            d.B=new ArrayList<Object>();d.B.add(null);actors[i]=d;
        }
        List<dk> queue=new ArrayList<>();
        if(!cells[6].isEmpty())for(String item:cells[6].split(",")) {
            int id=Integer.parseInt(item);queue.add(id==0?null:actors[id-1]);
        }
        n renderer=(n)raw(n.class);field(n.class,"m").set(renderer,cells[4].equals("1")?null:queue);
        dk.b=0;lm.a=true;int result=-1;String exception="NONE";
        dk[] sorted=queue.toArray(new dk[0]);
        beginCase();
        try {
            if(kind.equals("UPDATE"))result=renderer.a(camera,cells[3].equals("1"))?1:0;
            else if(kind.equals("SORT")){Arrays.sort(sorted,new md(camera));result=1;}
            else result=new md(camera).a(queue.get(0),queue.get(1));
        } catch(NullPointerException expected) {exception="NPE";}
          catch(IllegalArgumentException expected) {exception="IAE";}
        if(kind.equals("UPDATE")) {
            require(field(n.class,"m").get(renderer)==(cells[4].equals("1")?null:queue));
            row="UPDATE "+index+" "+name+" "+result+" "+exception+" "+
                ids(cells[4].equals("1")?null:queue)+" "+fields(0)+" "+fields(1)+" "+
                fields(2)+" "+fields(3)+" "+fields(4)+" "+dk.b+" "+(lm.a?1:0);
        } else if(kind.equals("SORT"))row="SORT "+index+" "+name+" "+result+" "+exception+" "+ids(Arrays.asList(sorted));
        else row="COMPARE "+index+" "+name+" "+result+" "+exception;
        endCase();
    }
    @SuppressWarnings({"removal","unchecked"})
    public static void main(String[] args) throws Exception {
        require(args.length==2);Path jar=Path.of(args[0]).toRealPath();
        require(System.getProperty("java.util.Arrays.useLegacyMergeSort")==null);
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission permission){}
            @Override public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            @Override public void checkWrite(String file){throw new SecurityException("writes denied");}
            @Override public void checkLink(String library){++nativeLinks;throw new SecurityException("native links denied");}
        });
        for(Class<?> type:new Class<?>[]{n.class,md.class,dk.class,ew.class,dc.class,lm.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar));
        for(String line:Files.readAllLines(Path.of(args[1])))if(!line.isEmpty())run(line);
        require(network==0&&nativeLinks==0);
        System.out.println("Original update observer: 42 updates; 14 sorts; 8 compares; network=0; native=0");
    }
}
