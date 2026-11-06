import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored operands and passive transport; the original protected GB bodies execute. */
@SuppressWarnings({"rawtypes","unchecked"})
public final class CreeperTarget {
    public static int active=-1,constructor_case=-1;
    public static fd world;
    public static gb actor,constructed;
    static Random sharedMath;
    static PrintWriter output;
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static String seed(Random r)throws Exception{return r==null?null:String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,Random.class,"seed")).get());}
    static Random math()throws Exception{return(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");}
    static Object value(Object v){if(v==null)return null;if(v instanceof Float)return ConstructorJson.map("F",bits((Float)v));if(v instanceof Double)return ConstructorJson.map("D",bits((Double)v));if(v instanceof Long)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)v));if(v instanceof String||v instanceof Number||v instanceof Boolean)return v;return ConstructorJson.map("class",v.getClass().getName(),"identity",identities.computeIfAbsent(v,k->identities.size()+1));}
    static Map<String,Object> fields(Object o)throws Exception{if(o==null)return null;Map<String,Object> out=new TreeMap<>();for(Class<?> c=o.getClass();c!=Object.class;c=c.getSuperclass())for(Field f:c.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){f.setAccessible(true);out.put(c.getName()+"."+f.getName(),value(f.get(o)));}return out;}
    static Map watcher()throws Exception{return(Map)get(get(actor,sn.class,"bD"),ud.class,"b");}
    static Map<String,Object> state()throws Exception{
        ud w=actor.ad();ma a=(ma)watcher().get(16),b=(ma)watcher().get(17);
        return ConstructorJson.map("fuse",get(actor,gb.class,"a"),"previous",get(actor,gb.class,"b"),"attacking",get(actor,ii.class,"e"),"dead",actor.be,"world_null",actor.aI==null,"direction",((Byte)a.b()).intValue(),"powered",((Byte)b.b()).intValue(),"dirty16",get(a,ma.class,"d"),"dirty17",get(b,ma.class,"d"),"dirty",get(w,ud.class,"c"),"x",bits(actor.aM),"y",bits(actor.aN),"z",bits(actor.aO));
    }
    static Map<String,Object> snapshot()throws Exception{return ConstructorJson.map("state",state(),"fields",fields(actor),"bounds",fields(actor.aW),"own_seed",seed(actor.bs),"world_seed",seed(world.r),"math_seed",seed(sharedMath),"box_cursor",field(eq.class,"h").getInt(null),"vector_cursor",field(bt.class,"e").getInt(null));}
    static fd make()throws Exception{
        fd w=new fd(new wy(),"CreeperAttackReference",xa.a(0),23L);w.B=false;w.q=1;w.f=0;
        Map cache=(Map)get(get(w,fd.class,"v"),ok.class,"e");cache.clear();List loaded=(List)get(get(w,fd.class,"v"),ok.class,"f");loaded.clear();
        for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){lm c=new lm(w,new byte[32768],x,z);c.n=true;cache.put(yy.a(x,z),c);loaded.add(c);}
        if(!w.b.isEmpty()||!w.d.isEmpty()||!((List)get(w,fd.class,"u")).isEmpty())throw new AssertionError("genuine empty Entity/Player/WorldAccess lists");
        return w;
    }
    static void one(String[] p)throws Exception{
        int id=Integer.parseInt(p[0]);identities.clear();world=make();sharedMath=math();String mathBefore=seed(sharedMath);constructed=null;
        constructor_case=id;actor=new gb(world);constructed=actor;constructor_case=-1;
        Map<String,Object> context=ConstructorJson.map("raw",snapshot(),"math_before",mathBefore,"math_after",seed(sharedMath),"default_entropy_rebound",false);
        actor.c(8.5,64.5,8.5,0,0);field(gb.class,"a").setInt(actor,Integer.parseInt(p[7]));field(gb.class,"b").setInt(actor,Integer.parseInt(p[8]));field(ii.class,"e").setBoolean(actor,p[11].equals("1"));
        actor.ad().b(16,Byte.valueOf(p[9]));actor.ad().b(17,Byte.valueOf(p[10]));
        for(Object r:watcher().values())field(ma.class,"d").setBoolean(r,false);field(ud.class,"c").setBoolean(actor.ad(),false);
        world.B=p[3].equals("1");if(p[4].equals("1"))actor.aI=null;
        Map<String,Object> before=snapshot();String failure=null;int outcome=1;boolean unseen=p[2].equals("1");
        Method m=gb.class.getDeclaredMethod(unseen?"b":"a",sn.class,float.class);m.setAccessible(true);
        active=id;try{m.invoke(actor,p[5].equals("1")?actor:null,Float.intBitsToFloat((int)Long.parseUnsignedLong(p[6],16)));}catch(InvocationTargetException e){outcome=-1;failure=e.getCause().getClass().getName();}active=-1;
        output.println(ConstructorJson.json(ConstructorJson.map("event","case","case",id,"scenario",p[1],"input",List.of(p),"constructor",context,"before",before,"after",snapshot(),"outcome",outcome,"failure",failure,"default_entropy_rebound",false,"world_access_list_size",((List)get(world,fd.class,"u")).size())));output.flush();
    }
    static void origins(Path jar,Path list)throws Exception{
        for(String name:Files.readAllLines(list)){Class<?> c=Class.forName(name,false,CreeperTarget.class.getClassLoader());String source=c.getProtectionDomain().getCodeSource().getLocation().toURI().toString();try(InputStream in=c.getResourceAsStream("/"+name+".class")){output.println(ConstructorJson.json(ConstructorJson.map("event","origin","class",name,"location",source,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(in.readAllBytes())))));}}
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=4)throw new IllegalArgumentException("output jar profiles classes");
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){output=writer;new gr("CreeperAttackSetup","");List<String> rows=Files.readAllLines(Path.of(args[2]));for(int i=1;i<rows.size();i++)one(rows.get(i).split("\t",-1));origins(Path.of(args[1]),Path.of(args[3]));if(writer.checkError())throw new IOException("transport");}
    }
}
