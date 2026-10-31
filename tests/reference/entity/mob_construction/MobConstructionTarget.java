import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Actual original constructors; reflection observes fields and never modifies game or RNG state. */
public final class MobConstructionTarget {
    public static int activeCase=-1;
    public static sn result;
    public static fd world;
    private static Object skeletonStack;
    private static Object sheepColors;
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
    private static Map<String,Object> sheepColors() throws Exception {
        float[][] colors=dl.a;List<Object> rows=new ArrayList<>();
        IdentityHashMap<float[],Boolean> distinct=new IdentityHashMap<>();
        for(float[] row:colors) {
            List<Object> values=new ArrayList<>();
            for(float value:row)values.add(ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value))));
            rows.add(ConstructorJson.map("identity",ref(row),"length",row.length,"values",values));distinct.put(row,true);
        }
        return ConstructorJson.map("identity",ref(colors),"length",colors.length,"distinct_rows",distinct.size(),"rows",rows);
    }
    private static void bindOrigins(Path jar) throws Exception {
        for(String name:Files.readAllLines(Path.of(System.getProperty("mob.probe.classes")))) {
            Class<?> type=Class.forName(name,false,MobConstructionTarget.class.getClassLoader());
            if(!Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new AssertionError("foreign class "+name);
            byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name.replace('.','/')+".class")){bytes=in.readAllBytes();}
            System.out.println("BIND "+name+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        }
    }
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[1]).toRealPath();
        world=new fd(new wy(),"MobConstructorReference",xa.a(0),1234L);
        new gr("MobSetup",""); // Actual Session prepares the ordinary uu-before-gm registry order.
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);
        Random math=(Random)get(null,holder,"randomNumberGenerator");
        String[] names={"cn","uz","fr","gb","uw","wh","dl","bx","ww","gi","xt"};
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))) {
            out.println(ConstructorJson.json(ConstructorJson.map("event","setup","world_class",world.getClass().getName(),"save_handler_class","wy","dimension_class",world.t.getClass().getName(),"prep_session_class","gr","registry_context","actual gr static initialization prepares uu before gm","entropy_policy","read-only actual ordinary Random; no seed/clock/ID modifications")));
            for(int id=0;id<33;++id) {
                int pass=id<15?id/5:(id-15)/6,kind=id<15?id%5:5+(id-15)%6;
                world.B=pass==1;fd argument=pass==2?null:world;
                result=null;int counter=(Integer)get(null,sn.class,"a");long before=seed(math);
                Random oracle=new Random(before^0x5deece66dL);List<String> draws=new ArrayList<>();
                for(int i=0;i<4;++i)draws.add(String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(oracle.nextDouble())));
                activeCase=id;
                result=(sn)Class.forName(names[kind]).getDeclaredConstructor(fd.class).newInstance(argument);
                activeCase=-1;
                if(seed(math)!=seed(oracle)||(Integer)get(null,sn.class,"a")!=counter+1)throw new AssertionError("constructor Math/ID work");
                identities.clear();ref(result);ref(argument);
                Map<String,Object> row=ConstructorJson.map("event","case","case",id,"kind",names[kind],"pass",pass,"remote",world.B,"null_world",argument==null,
                    "entity_counter_before",counter,"entity_counter_after",get(null,sn.class,"a"),"same_world",result.aI==argument,
                    "math_seed_before",String.format(Locale.ROOT,"%012x",before),"math_seed_after",seedHex(math),"draws",draws,
                    "fields",fields(result),"bounds",fields(result.aW),"watcher",watcher(result.ad()),"private_random",random(result.bs));
                if(kind==2){Object stack=get(null,fr.class,"a");if(skeletonStack==null)skeletonStack=stack;row.put("skeleton_stack",fields(stack));row.put("skeleton_stack_alias",((fr)result).r_()==stack);row.put("skeleton_stack_same",skeletonStack==stack);}
                if(names[kind].equals("dl")){Object colors=dl.a;if(sheepColors==null)sheepColors=colors;row.put("sheep_colors",sheepColors());row.put("sheep_colors_same",sheepColors==colors);}
                out.println(ConstructorJson.json(row));out.flush();System.out.println("CASE "+id+" "+names[kind]);
            }
            if(out.checkError())throw new IOException("observation output");
        }
        bindOrigins(jar);
    }
}
