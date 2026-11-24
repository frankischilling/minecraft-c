import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Prepared genuine original DC/FD receivers. No game class/body is authored. */
public final class PlayerSleepObservation {
    static volatile int active=-1;
    static Object currentActor,currentWorld,currentProvider;
    static Class<?> currentCW;
    static Path directory,archive;
    static int network,nativeLinks,rejectedWrites;
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static void need(boolean ok,String message){if(!ok)throw new AssertionError(message);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object v,Class<?> c,String n)throws Exception{return field(c,n).get(v);}
    static void put(Object v,Class<?> c,String n,Object x)throws Exception{field(c,n).set(v,x);}
    static String hex(byte[] b){return HexFormat.of().formatHex(b);}
    static String dword(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static Object scalar(Object v){
        if(v==null||v instanceof Boolean||v instanceof Integer||v instanceof Byte||v instanceof Short)return v;
        if(v instanceof Character)return (int)(Character)v;
        if(v instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)v));
        if(v instanceof Float)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits((Float)v)));
        if(v instanceof Double)return map("D",dword((Double)v));
        return map("identity",identities.computeIfAbsent(v,k->identities.size()+1));
    }
    static Object fields(Object v,Class<?>... types)throws Exception{
        if(v==null)return null;Map<String,Object> result=new TreeMap<>();
        List<Field> selected=new ArrayList<>();for(Class<?> c:types)for(Field f:c.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers()))selected.add(f);
        selected.sort(Comparator.comparing(f->f.getDeclaringClass().getName()+"."+f.getName()));
        for(Field f:selected){f.setAccessible(true);result.put(f.getDeclaringClass().getName()+"."+f.getName(),scalar(f.get(v)));}return result;
    }
    static Object random(Object v)throws Exception{
        if(v==null)return null;return Arrays.asList(String.format(Locale.ROOT,"%016x",((AtomicLong)get(v,Random.class,"seed")).get()),
            get(v,Random.class,"haveNextNextGaussian"),dword((Double)get(v,Random.class,"nextNextGaussian")));
    }
    static Object state(ClassLoader l)throws Exception{
        Class<?> sn=l.loadClass("sn"),gs=l.loadClass("gs"),fd=l.loadClass("fd");
        Object box=get(currentActor,sn,"aW");List<Object> coords=new ArrayList<>();
        for(String n:new String[]{"v","b","bN"}){Object c=get(currentActor,gs,n);coords.add(Arrays.asList(scalar(c),c==null?null:fields(c,l.loadClass("br"))));}
        return map("actor",fields(currentActor,sn,l.loadClass("ls"),gs,l.loadClass("dc")),
            "bounds",fields(box,l.loadClass("eq")),"coordinates",coords,
            "world",fields(currentWorld,fd),"provider",fields(currentProvider,l.loadClass("xa")),
            "random",Arrays.asList(random(get(currentActor,sn,"bs")),random(get(currentWorld,fd,"r"))));
    }
    static Object enumeration(Object result)throws Exception{
        if(result==null)return null;Class<?> c=result.getClass();List<Object> cells=new ArrayList<>();List<Boolean> valuesSame=new ArrayList<>();
        Object[] values=(Object[])get(null,c,"f");List<Object> refs=new ArrayList<>();
        for(String n:new String[]{"a","b","c","d","e"}){
            Object v=get(null,c,n);refs.add(v);Enum<?> e=(Enum<?>)v;List<Integer> units=new ArrayList<>();for(char u:e.name().toCharArray())units.add((int)u);
            cells.add(map("field",n,"name_utf16",units,"ordinal",e.ordinal(),"result_same",result==v,"name_interned",e.name()==e.name().intern()));
        }
        List<Object> matrix=new ArrayList<>();for(Object a:refs){List<Boolean> row=new ArrayList<>();for(Object b:refs)row.add(a==b);matrix.add(row);}
        need(values.length==5,"actual CW values extent");for(int i=0;i<5;++i)valuesSame.add(values[i]==refs.get(i));
        return map("statics",cells,"identity_matrix",matrix,"values_same",valuesSame,"values_distinct",values!=refs.toArray());
    }
    static void bind(ClassLoader l,Path names,PrintWriter out,int profile)throws Exception{
        for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){
            Class<?> c=Class.forName(name.replace('/','.'),false,l);
            need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"actual class origin "+name);
            byte[] b;try(InputStream in=c.getResourceAsStream("/"+name+".class")){need(in!=null,"original class bytes");b=in.readAllBytes();}
            out.println(profile+"\t"+name+"\t"+b.length+"\t"+hex(MessageDigest.getInstance("SHA-256").digest(b)));
        }out.flush();need(!out.checkError(),"binding output");
    }
    @SuppressWarnings("removal") static final class Guard extends SecurityManager{
        public void checkPermission(Permission p){}
        public void checkConnect(String h,int p){++network;throw new SecurityException("unexpected network");}
        public void checkListen(int p){++network;throw new SecurityException("unexpected listener");}
        public void checkLink(String n){++nativeLinks;throw new SecurityException("unexpected native link");}
        public void checkWrite(String n){if(!Path.of(n).toAbsolutePath().normalize().startsWith(directory)){++rejectedWrites;throw new SecurityException("write outside work");}}
        public void checkDelete(String n){checkWrite(n);}
    }
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception{
        need(args.length==6,"jar work profiles names bindings sdk-classpath");archive=Path.of(args[0]).toRealPath();directory=Path.of(args[1]).toAbsolutePath().normalize();Files.createDirectories(directory);
        System.setSecurityManager(new Guard());List<URL> urls=new ArrayList<>();urls.add(archive.toUri().toURL());for(String n:args[5].split(java.util.regex.Pattern.quote(File.pathSeparator)))urls.add(Path.of(n).toUri().toURL());
        int number=0;try(PrintWriter bindings=new PrintWriter(Files.newBufferedWriter(Path.of(args[4]),StandardCharsets.UTF_8))){
        for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            String[] w=line.split(" ");need(w.length==15&&Integer.parseInt(w[0])==number,"ordered exact profile");
            try(URLClassLoader loader=new URLClassLoader(urls.toArray(new URL[0]),ClassLoader.getPlatformClassLoader())){
                Class<?> fd=loader.loadClass("fd"),xa=loader.loadClass("xa"),gs=loader.loadClass("gs"),sn=loader.loadClass("sn"),dc=loader.loadClass("dc"),gr=loader.loadClass("gr");
                currentProvider=xa.getMethod("a",int.class).invoke(null,0);
                currentWorld=fd.getConstructor(loader.loadClass("wt"),String.class,xa,long.class).newInstance(loader.loadClass("wy").getConstructor().newInstance(),"PlayerSleepReference",currentProvider,5143L);
                fd.getMethod("a",loader.loadClass("br")).invoke(currentWorld,loader.loadClass("br").getConstructor(int.class,int.class,int.class).newInstance(0,64,0));
                Object session=gr.getConstructor(String.class,String.class).newInstance("SleepReference","-");
                currentActor=dc.getConstructor(loader.loadClass("net.minecraft.client.Minecraft"),fd,gr,int.class).newInstance(null,currentWorld,session,0);
                sn.getMethod("e",double.class,double.class,double.class).invoke(currentActor,8.5,65.0,8.5);
                put(currentWorld,fd,"B",Integer.parseInt(w[2])!=0);put(currentActor,gs,"u",Integer.parseInt(w[3])!=0);put(currentActor,sn,"be",Integer.parseInt(w[4])!=0);put(currentActor,loader.loadClass("ls"),"Y",Integer.parseInt(w[5]));
                put(currentProvider,xa,"c",Integer.parseInt(w[7])!=0);put(currentWorld,fd,"f",Integer.parseInt(w[8]));
                put(currentActor,sn,"aM",Double.longBitsToDouble(Long.parseUnsignedLong(w[9],16)));put(currentActor,sn,"aN",Double.longBitsToDouble(Long.parseUnsignedLong(w[10],16)));put(currentActor,sn,"aO",Double.longBitsToDouble(Long.parseUnsignedLong(w[11],16)));
                put(currentActor,gs,"a",17);put(currentActor,gs,"w",.375f);put(currentActor,gs,"y",-.625f);
                put(currentActor,sn,"aP",.25);put(currentActor,sn,"aQ",-.375);put(currentActor,sn,"aR",.125);
                for(String n:new String[]{"v","b","bN"})put(currentActor,gs,n,loader.loadClass("br").getConstructor(int.class,int.class,int.class).newInstance(21,67,-23));
                ((Random)get(currentActor,sn,"bs")).setSeed(38);((Random)get(currentWorld,fd,"r")).setSeed(2468);
                if(Integer.parseInt(w[6])!=0)put(currentWorld,fd,"t",null);if(Integer.parseInt(w[1])!=0)put(currentActor,sn,"aI",null);
                currentCW=Class.forName("cw",false,loader);need(currentCW.getDeclaredFields().length==6,"passive CW structural preparation");identities.clear();Object before=state(loader);Object result=null;String exception="NONE";
                active=number;try{result=gs.getMethod("b",int.class,int.class,int.class).invoke(currentActor,Integer.parseInt(w[12]),Integer.parseInt(w[13]),Integer.parseInt(w[14]));}
                catch(InvocationTargetException e){need(e.getCause() instanceof NullPointerException,"only declared null source fault: "+e.getCause());exception="java/lang/NullPointerException";}
                finally{active=-1;}
                Object after=state(loader);Enum<?> e=result==null?null:(Enum<?>)result;List<String> input=Arrays.asList(w);
                System.out.println("PLAYER_SLEEP "+json(map("schema","PLAYER_SLEEP_RAW_V1","case",number,"input",input,"exception",exception,
                    "result",e==null?null:map("name",e.name(),"ordinal",e.ordinal()),"before",before,"after",after,"unchanged",before.equals(after),"enum",enumeration(result))));
                bind(loader,Path.of(args[3]),bindings,number);
                currentActor=null;currentWorld=null;currentProvider=null;currentCW=null;identities.clear();
            }++number;if(number%4==0)System.gc();
        }}need(network==0&&nativeLinks==0&&rejectedWrites==0,"external effects");
        System.out.println("Original Player sleep: "+number+" profiles; network=0; native=0; rejected_writes=0");
    }
    static Map<String,Object> map(Object... v){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<v.length;i+=2)r.put((String)v[i],v[i+1]);return r;}
    static String json(Object v){if(v==null)return "null";if(v instanceof Number||v instanceof Boolean)return v.toString();if(v instanceof String){StringBuilder r=new StringBuilder("\"");for(char c:((String)v).toCharArray())if(c=='"'||c=='\\')r.append('\\').append(c);else if(c<32||c>126)r.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else r.append(c);return r.append('"').toString();}if(v instanceof Map){StringJoiner r=new StringJoiner(",","{","}");for(Map.Entry<?,?> e:((Map<?,?>)v).entrySet())r.add(json(e.getKey())+":"+json(e.getValue()));return r.toString();}if(v instanceof Iterable){StringJoiner r=new StringJoiner(",","[","]");for(Object e:(Iterable<?>)v)r.add(json(e));return r.toString();}throw new IllegalArgumentException("JSON transport");}
}
