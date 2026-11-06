import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Actual original constructors; reflection observes fields and never modifies game or RNG state. */
public final class LightningConstructionTarget {
    public static int activeCase=-1;
    public static c result;
    public static fd world;
    private static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    private static Object get(Object receiver,Class<?> owner,String name) throws Exception {
        Field f=owner.getDeclaredField(name);f.setAccessible(true);return f.get(receiver);
    }
    private static Object ref(Object value) {
        if(value==null)return null;
        int id=identities.computeIfAbsent(value,key->identities.size()+1);
        Map<String,Object> out=ConstructorJson.map("class",value.getClass().getName(),"identity",id);
        if(value instanceof String)out.put("string",value);
        return out;
    }
    private static Map<String,Object> fields(Object receiver) throws Exception {
        if(receiver==null)return null;
        Map<String,Object> out=new TreeMap<>();
        for(Class<?> owner=receiver.getClass();owner!=Object.class;owner=owner.getSuperclass())
            for(Field f:owner.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())) {
                f.setAccessible(true);Object value=f.get(receiver);
                if(f.getType()==float.class)value=ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits((Float)value)));
                else if(f.getType()==double.class)value=ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)value)));
                else if(f.getType()==long.class)value=ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)value));
                else if(!f.getType().isPrimitive())value=ref(value);
                out.put(owner.getName()+"."+f.getName(),value);
            }
        return out;
    }
    private static long seed(Random random) throws Exception { return ((AtomicLong)get(random,Random.class,"seed")).get(); }
    private static String seedHex(Random random) throws Exception {return String.format(Locale.ROOT,"%012x",seed(random));}
    private static Map<String,Object> random(Random random) throws Exception {
        return ConstructorJson.map("seed48",seedHex(random),"gaussian",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)get(random,Random.class,"nextNextGaussian"))),"cached",get(random,Random.class,"haveNextNextGaussian"));
    }
    private static Map<String,Object> watcher(ud watcher) throws Exception {
        Map<?,?> entries=(Map<?,?>)get(watcher,ud.class,"b");List<Object> rows=new ArrayList<>();
        List<Integer> keys=new ArrayList<>();for(Object key:entries.keySet())keys.add((Integer)key);Collections.sort(keys);
        for(int key:keys){ma record=(ma)entries.get(key);rows.add(ConstructorJson.map("index",record.a(),"type",record.c(),"value",record.b(),"dirty",get(record,ma.class,"d")));}
        return ConstructorJson.map("size",entries.size(),"dirty",get(watcher,ud.class,"c"),"records",rows);
    }
    private static void bindOrigins(Path jar) throws Exception {
        for(String name:Files.readAllLines(Path.of(System.getProperty("mob.probe.classes")))) {
            Class<?> type=Class.forName(name,false,LightningConstructionTarget.class.getClassLoader());
            if(!Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new AssertionError("foreign class "+name);
            byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name.replace('.','/')+".class")){bytes=in.readAllBytes();}
            System.out.println("BIND "+name+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        }
    }
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[1]).toRealPath();
        world=new fd(new wy(),"LightningConstructorReference",xa.a(0),1234L);
        new gr("LightningSetup","");
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);
        Random math=(Random)get(null,holder,"randomNumberGenerator");
        int[] difficulty={0,1,2,2,2};double[] ys={64.5,-1.5,140.0,-20.0,64.5};
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))) {
            out.println(ConstructorJson.json(ConstructorJson.map("event","setup","world_class",world.getClass().getName(),"entropy_policy","unchanged ordinary private Random, read-only actual clocks; no Entity/RNG/source counter assignments")));
            for(int id=0;id<5;++id) {
                world.q=difficulty[id];fd argument=id==4?null:world;result=null;
                int counter=(Integer)get(null,sn.class,"a");String before=seedHex(math),exception=null;
                activeCase=id;
                try {result=new c(argument,-17.25,ys[id],31.75);} catch(NullPointerException failure) {exception=failure.getClass().getName();}
                finally {activeCase=-1;}
                if(!seedHex(math).equals(before)||(Integer)get(null,sn.class,"a")!=counter+1)throw new AssertionError("constructor no-Math/ID work");
                identities.clear();ref(result);ref(argument);
                out.println(ConstructorJson.json(ConstructorJson.map("event","case","case",id,"difficulty",difficulty[id],"x",-17.25,"y",ys[id],"z",31.75,"null_world",argument==null,"exception",exception,
                    "entity_counter_before",counter,"entity_counter_after",get(null,sn.class,"a"),"math_seed_before",before,"math_seed_after",seedHex(math),
                    "fields",fields(result),"bounds",result==null?null:fields(result.aW),"watcher",result==null?null:watcher(result.ad()),"private_random",result==null?null:random(result.bs))));
                out.flush();System.out.println("CASE "+id);
            }
            if(out.checkError())throw new IOException("observation output");
        }
        bindOrigins(jar);
    }
}
