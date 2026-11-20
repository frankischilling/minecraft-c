import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored operands and observations of unchanged original path methods. */
@SuppressWarnings({"unchecked","rawtypes"})
public final class PathTarget {
    public static int active=-1,constructor_case=-1;
    public static fd world;
    public static sn actor,target,constructed;
    public static ew cache;
    public static fw finder;
    public static Object result;
    public static Object[] rootObjects=new Object[0];
    static Path inputs;static PrintWriter output;static Random sharedMath;
    static List<Object> steps;
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static final Set<String> EXPAND=Set.of("fw","lf","jx","wm","d","dh","ew");
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,String owner,String n)throws Exception{return field(Class.forName(owner),n).get(o);}
    static void need(boolean ok,String text){if(!ok)throw new AssertionError(text);}
    static String bits(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static String bits(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static Object value(Object v){if(v==null)return null;if(v instanceof Float)return ConstructorJson.map("F",bits((Float)v));if(v instanceof Double)return ConstructorJson.map("D",bits((Double)v));if(v instanceof Long)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)v));if(v instanceof Number||v instanceof Boolean||v instanceof String)return v;return ConstructorJson.map("class",v.getClass().getName(),"identity",identities.computeIfAbsent(v,k->identities.size()+1));}
    static Map<String,Object> fields(Object o)throws Exception{if(o==null)return null;Map<String,Object> r=new TreeMap<>();for(Class<?> c=o.getClass();c!=Object.class;c=c.getSuperclass())for(Field f:c.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){f.setAccessible(true);r.put(c.getName()+"."+f.getName(),value(f.get(o)));}return r;}
    static String seed(Random r)throws Exception{return r==null?"-":String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,"java.util.Random","seed")).get());}
    static Random math()throws Exception{return(Random)field(Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator").get(null);}
    static Map chunks()throws Exception{return(Map)get(get(world,"fd","v"),"ok","e");}
    static fd make()throws Exception{fd w=new fd(new wy(),"PathReference",xa.a(0),23L);w.B=false;w.q=1;w.f=0;Map map=(Map)get(get(w,"fd","v"),"ok","e");map.clear();List loaded=(List)get(get(w,"fd","v"),"ok","f");loaded.clear();for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){lm c=new lm(w,new byte[32768],x,z);c.n=true;for(int xx=0;xx<16;xx++)for(int zz=0;zz<16;zz++)c.b[(xx<<11)|(zz<<7)|63]=1;Arrays.fill(c.h,(byte)64);c.i=64;map.put(yy.a(x,z),c);loaded.add(c);}return w;}
    static sn construct(int token,List<Object> contexts)throws Exception{String before=sharedMath==null?null:seed(sharedMath);constructor_case=token;constructed=null;sn e=new bx(world);constructed=e;constructor_case=-1;sharedMath=math();contexts.add(ConstructorJson.map("token",token,"fields",fields(e),"bounds",fields(e.aW),"own_seed",seed(e.bs),"math_seed_before",before,"math_seed_after",seed(sharedMath),"default_entropy_rebound",false));return e;}
    static float word(String s){return Float.intBitsToFloat((int)Long.parseUnsignedLong(s,16));}
    static List<String[]> table(String name)throws Exception{List<String[]>r=new ArrayList<>();List<String>a=Files.readAllLines(inputs.resolve(name));for(int i=1;i<a.size();i++)r.add(a.get(i).split("\t",-1));return r;}
    static void cell(String s)throws Exception{String[]a=s.split(",");int x=Integer.parseInt(a[0]),y=Integer.parseInt(a[1]),z=Integer.parseInt(a[2]),id=Integer.parseInt(a[3]),meta=Integer.parseInt(a[4]);lm c=(lm)chunks().get(yy.a(x>>4,z>>4));need(c!=null&&y>=0&&y<128,"authored cached cell");c.b[((x&15)<<11)|((z&15)<<7)|y]=(byte)id;c.e.a(x&15,y,z&15,meta);if(id>0)c.h[((z&15)<<4)|(x&15)]=(byte)Math.max(c.h[((z&15)<<4)|(x&15)]&255,y+1);}
    static int[] ints(String s){return Arrays.stream(s.split(",")).mapToInt(Integer::parseInt).toArray();}
    static Object invoke(Object o,Class<?> c,String n,Class<?>[]types,Object...args)throws Throwable{Method m=c.getDeclaredMethod(n,types);m.setAccessible(true);try{return m.invoke(o,args);}catch(InvocationTargetException e){throw e.getCause();}}
    static Map<String,Object> graph()throws Exception{
        Map<String,Object> roots=ConstructorJson.map("world",value(world),"actor",value(actor),"target",value(target),"provider_fallback",value(get(get(world,"fd","v"),"ok","b")),"cache",value(cache),"finder",value(finder),"result",value(result),"extra",value(rootObjects));
        ArrayDeque<Object> queue=new ArrayDeque<>();for(Object o:new Object[]{cache,finder,result,rootObjects})if(o!=null)queue.add(o);Set<Object> seen=Collections.newSetFromMap(new IdentityHashMap<>());List<Object> nodes=new ArrayList<>();
        while(!queue.isEmpty()){Object o=queue.remove();if(!seen.add(o))continue;Class<?> c=o.getClass();boolean array=c.isArray()&&!c.getComponentType().isPrimitive();if(!array&&!EXPAND.contains(c.getName()))continue;Map<String,Object> node=new LinkedHashMap<>((Map<String,Object>)value(o));if(array){int n=Array.getLength(o);List<Object> values=new ArrayList<>();for(int i=0;i<n;i++){Object v=Array.get(o,i);values.add(value(v));if(v!=null)queue.add(v);}node.put("length",n);node.put("values",values);}else{node.put("fields",fields(o));for(Class<?> owner=c;owner!=Object.class;owner=owner.getSuperclass())for(Field f:owner.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){f.setAccessible(true);Object v=f.get(o);if(v!=null&&!f.getType().isPrimitive())queue.add(v);}}nodes.add(node);}
        return ConstructorJson.map("roots",roots,"objects",nodes);
    }
    static Map<String,Object> state()throws Exception{return ConstructorJson.map("actor_fields",fields(actor),"actor_bounds",actor==null?null:fields(actor.aW),"target_fields",fields(target),"target_bounds",target==null?null:fields(target.aW),"own_seed",actor==null?"-":seed(actor.bs),"target_seed",target==null?"-":seed(target.bs),"world_seed",seed(world.r),"math_seed",seed(sharedMath),"box_cursor",field(eq.class,"h").getInt(null),"vector_cursor",field(bt.class,"e").getInt(null),"loaded_entities",world.b.size(),"cache_size",chunks().size(),"graph",graph());}
    static void snapshot(String label)throws Exception{steps.add(ConstructorJson.map("step",label,"graph",graph(),"box_cursor",field(eq.class,"h").getInt(null),"vector_cursor",field(bt.class,"e").getInt(null)));}
    static Object run(String[]p,List<String[]>ops)throws Throwable{
        String operation=p[1],scene=p[2];int x=(int)Double.parseDouble(p[6]),y=(int)Double.parseDouble(p[7]),z=(int)Double.parseDouble(p[8]);float range=word(p[9]);boolean nullSource=false,nullTarget=false,nullAccess=false;d extent=null;int step=0;String pairs=null;
        for(String[]a:ops){if(a[1].equals("operand")){if(a[3].equals("source"))nullSource=true;else if(a[3].equals("target"))nullTarget=true;else nullAccess=true;}else if(a[1].equals("extent")){int[]v=ints(a[5]);extent=new d(v[0],v[1],v[2]);}else if(a[1].equals("step"))step=Integer.parseInt(a[5]);else if(a[1].equals("points"))pairs=a[5];}
        if(operation.equals("FD_coordinates"))return world.a(nullSource?null:actor,x,y,z,range);
        if(operation.equals("FD_entity"))return world.a(actor,nullTarget?null:target,range);
        if(operation.startsWith("FW_")||operation.startsWith("EW_")){cache=new ew(world,-16,-16,-16,32,96,32);finder=new fw(nullAccess?null:cache);}
        if(extent!=null)rootObjects=new Object[]{extent};
        if(operation.equals("FW_coordinates"))return finder.a(actor,x,y,z,range);
        if(operation.equals("FW_clearance"))return invoke(finder,fw.class,"a",new Class[]{sn.class,int.class,int.class,int.class,d.class},actor,x,y,z,extent);
        if(operation.equals("FW_neighbor"))return invoke(finder,fw.class,"a",new Class[]{sn.class,int.class,int.class,int.class,d.class,int.class},actor,x,y,z,extent,step);
        if(operation.equals("EW_block"))return cache.a(x,y,z);
        if(operation.equals("EW_metadata"))return cache.e(x,y,z);
        if(operation.equals("FW_intern")||operation.equals("D_distance")||operation.equals("D_equals")){String[]a=pairs.split(";");Object[]v=new Object[a.length];for(int i=0;i<a.length;i++){int[]n=ints(a[i]);v[i]=operation.equals("FW_intern")?invoke(finder,fw.class,"a",new Class[]{int.class,int.class,int.class},n[0],n[1],n[2]):new d(n[0],n[1],n[2]);}rootObjects=v;if(operation.equals("D_distance"))return((d)v[0]).a((d)v[1]);if(operation.equals("D_equals"))return v[0].equals(v[1]);return v[0]==v[1];}
        if(operation.equals("LF_sequence")){lf heap=new lf();int n=scene.equals("heap_growth")?1025:scene.equals("duplicate_point")?1:4;d[]points=new d[n];rootObjects=new Object[]{heap,points};snapshot("constructed");for(int i=0;i<n;i++){d v=points[i]=new d(i,64,0);field(d.class,"g").setFloat(v,scene.equals("heap_growth")?(float)(i%17):1f);heap.a(v);}snapshot("inserted");if(scene.equals("duplicate_point"))return heap.a(points[0]);List<Object> popped=new ArrayList<>();if(scene.equals("heap_ties")){while(!heap.c())popped.add(value(heap.b()));steps.add(ConstructorJson.map("step","popped","values",popped));}return null;}
        if(operation.equals("JX_sequence")){jx map=new jx();d[]points=new d[13];rootObjects=new Object[]{map,points};snapshot("constructed");for(int i=0;i<13;i++){d v=points[i]=new d(i,64,0);map.a(v.hashCode(),v);}snapshot("inserted");if(scene.equals("map_clear")){map.a();snapshot("cleared");}return null;}
        if(operation.equals("DH_sequence")){if(scene.equals("null_array"))return new dh(null);d[]points=scene.equals("empty_last")?new d[0]:new d[]{new d(1,2,3),new d(4,5,6)};dh path=new dh(points);rootObjects=new Object[]{points,path,path};if(scene.equals("empty_last"))return path.c();if(scene.equals("bad_index_null_entity")){path.a();path.a();return path.a(null);}if(scene.equals("null_entity"))return path.a(null);result=path;snapshot("initial");for(int i=0;i<2;i++){bt v=path.a(actor);steps.add(ConstructorJson.map("step","waypoint"+i,"value",fields(v)));path.a();snapshot("advanced"+i);}steps.add(ConstructorJson.map("step","finished","value",path.b()));path.a();snapshot("past_end");return path;}
        throw new AssertionError(operation);
    }
    static void one(String[]p)throws Exception{
        int id=Integer.parseInt(p[0]);identities.clear();world=make();cache=null;finder=null;result=null;rootObjects=new Object[0];steps=new ArrayList<>();List<Object>contexts=new ArrayList<>();actor=construct(id*2,contexts);target=p[1].equals("FD_entity")?construct(id*2+1,contexts):null;actor.bg=word(p[10]);actor.bh=word(p[11]);actor.c(Double.parseDouble(p[3]),Double.parseDouble(p[4]),Double.parseDouble(p[5]),30f,0f);need(world.b(actor),"ordinary actor publication");if(target!=null){target.c(Double.parseDouble(p[6]),Double.parseDouble(p[7]),Double.parseDouble(p[8]),0f,0f);need(world.b(target),"ordinary target publication");target.bs.setSeed(17);}actor.bs.setSeed(0);world.r.setSeed(89);
        List<String[]>ops=new ArrayList<>();for(String[]a:table("scene-preparation.tsv"))if(Integer.parseInt(a[0])==id){ops.add(a);if(a[1].equals("cell"))cell(a[5]);else if(a[1].equals("blank_alias")){int[]v=ints(a[5]);Object provider=get(world,"fd","v");lm blank=(lm)get(provider,"ok","b");need(blank instanceof li,"actual original provider Blank");chunks().put(yy.a(v[0],v[1]),blank);List loaded=(List)get(provider,"ok","f");if(!loaded.contains(blank))loaded.add(blank);}else if(a[1].equals("fill")){int[]v=ints(a[5]);for(int x=v[0];x<=v[3];x++)for(int y=v[1];y<=v[4];y++)for(int z=v[2];z<=v[5];z++)cell(x+","+y+","+z+","+v[6]+","+v[7]);}else if(a[1].equals("field")||a[1].equals("target_field"))field(Class.forName(a[2]),a[3]).set(a[1].equals("field")?actor:target,null);}
        eq.b();bt.b();Map<String,Object>before=state();int outcome=1;String failure=null;String top=null;active=id;try{result=run(p,ops);}catch(Throwable e){outcome=-1;failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}active=-1;output.println(ConstructorJson.json(ConstructorJson.map("event","path","case",id,"input",List.of(p),"operation",p[1],"scene",p[2],"constructor_contexts",contexts,"before",before,"after",state(),"steps",steps,"outcome",outcome,"failure",failure,"top",top,"default_constructor_entropy_rebound",false)));output.flush();System.out.println("PATH "+id+" "+outcome+" "+failure);
    }
    static void bindings(Path jar)throws Exception{String names=System.getProperty("path.classes");if(names==null)return;for(String name:Files.readAllLines(Path.of(names))){Class<?> c=Class.forName(name,false,PathTarget.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");byte[]data;try(InputStream in=c.getResourceAsStream("/"+name.replace('.','/')+".class")){data=in.readAllBytes();}System.out.println("BIND "+name+" "+data.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data)));}}
    public static void main(String[]args)throws Exception{new gr("PathSetup","");inputs=Path.of(args[2]);try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){output=out;for(String[]p:table("profiles.tsv"))one(p);if(out.checkError())throw new IOException("output");}bindings(Path.of(args[1]).toRealPath());}
}
