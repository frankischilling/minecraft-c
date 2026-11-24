import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored mutable inputs and observation transport. Original classes are unchanged. */
@SuppressWarnings({"rawtypes", "unchecked"})
public final class MonsterTarget {
    public static int active = -1;
    public static uz actor;
    public static wh target;
    public static fd world;
    public static boolean targetNull;
    static PrintWriter output;
    static Field field(Class<?> c, String name) throws Exception {
        Field f = c.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object get(Object o, Class<?> c, String n) throws Exception { return field(c,n).get(o); }
    static String bits(double v) { return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v)); }
    static String bits(float v) { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v)); }
    static String seed(Random r) throws Exception {
        return r == null ? null : String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,Random.class,"seed")).get());
    }
    static Object value(Object o) {
        if(o == null) return null;
        if(o instanceof Float) return ConstructorJson.map("F",bits((Float)o));
        if(o instanceof Double) return ConstructorJson.map("D",bits((Double)o));
        if(o instanceof Long) return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)o));
        if(o instanceof String || o instanceof Boolean || o instanceof Number) return o;
        return ConstructorJson.map("class",o.getClass().getName(),"role",o==actor?"actor":o==target?"target":o==world?"world":"other");
    }
    static Map<String,Object> fields(Object o) throws Exception {
        if(o == null) return null;
        Map<String,Object> result = new TreeMap<>();
        for(Class<?> c=o.getClass();c!=Object.class;c=c.getSuperclass())
            for(Field f:c.getDeclaredFields()) if(!Modifier.isStatic(f.getModifiers())) {
                f.setAccessible(true); result.put(c.getName()+"."+f.getName(),value(f.get(o)));
            }
        return result;
    }
    static Map<String,Object> snapshot() throws Exception {
        // Both genuine Living constructors have already reached Math.random.
        // No preconstructor Math-holder query, seed assignment or draw is added.
        Random math = (Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");
        return ConstructorJson.map("actor",fields(actor),"target",fields(target),
            "actor_bounds",fields(actor.aW),"target_bounds",fields(target.aW),
            "actor_seed",seed(actor.bs),"target_seed",seed(target.bs),
            "world_seed",seed(world.r),"math_seed",seed(math),
            "box_cursor",get(null,eq.class,"h"),"vector_cursor",get(null,bt.class,"e"));
    }
    static fd make() throws Exception {
        fd w=new fd(new wy(),"MonsterAttackReference",xa.a(0),23L);
        w.B=false;w.q=1;w.f=0;
        Map cache=(Map)get(get(w,fd.class,"v"),ok.class,"e");cache.clear();
        List loaded=(List)get(get(w,fd.class,"v"),ok.class,"f");loaded.clear();
        for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){
            lm c=new lm(w,new byte[32768],x,z);c.n=true;cache.put(yy.a(x,z),c);loaded.add(c);
        }
        if(!w.b.isEmpty()||!w.d.isEmpty()||!((List)get(w,fd.class,"u")).isEmpty())
            throw new AssertionError("actual empty World lists");
        return w;
    }
    static void one(String[] p) throws Exception {
        int id=Integer.parseInt(p[0]); world=make();
        actor=new uz(world);target=new wh(world);
        Map<String,Object> raw=snapshot();
        actor.c(8.5,64.5,8.5,0,0);target.c(9.5,64.5,8.5,0,0);
        // Declared Source geometry operands on the actual allocated bounds.
        actor.aW.b=64.0;actor.aW.e=66.0;
        target.aW.b=Double.parseDouble(p[7]);target.aW.e=Double.parseDouble(p[8]);
        field(ls.class,"ae").setInt(actor,Integer.parseInt(p[2]));
        targetNull=p[4].equals("1");
        if(p[5].equals("1"))field(sn.class,"aW").set(target,null);
        if(p[6].equals("1"))field(sn.class,"aW").set(actor,null);
        Map<String,Object> before=snapshot();
        Method method=gz.class.getDeclaredMethod("a",sn.class,float.class);method.setAccessible(true);
        String failure=null;boolean returned=false;
        active=id;
        try{method.invoke(actor,targetNull?null:target,Float.intBitsToFloat((int)Long.parseUnsignedLong(p[3],16)));returned=true;}
        catch(InvocationTargetException e){failure=e.getCause().getClass().getName();}
        active=-1;
        output.println(ConstructorJson.json(ConstructorJson.map("event","case","case",id,"input",List.of(p),
            "constructor",raw,"before",before,"after",snapshot(),"returned",returned,"failure",failure,
            "default_entropy_rebound",false,"world_access_list_size",((List)get(world,fd.class,"u")).size())));
        output.flush();
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=2)throw new IllegalArgumentException("output profiles");
        new gr("MonsterAttackSetup","");
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){
            output=writer;List<String> rows=Files.readAllLines(Path.of(args[1]));
            for(int i=1;i<rows.size();i++)one(rows.get(i).split("\t",-1));
            if(writer.checkError())throw new IOException("transport");
        }
    }
}
