import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Declared fixture operands and observations; unchanged original methods execute every tick. */
@SuppressWarnings({"rawtypes","unchecked"})
public final class MobTickTarget {
    public static int active=-1,constructor_case=-1;
    public static fd world;
    public static sn actor,constructed;
    public static Observer currentObserver;
    public static final List<sn> others=new ArrayList<>();
    static Random sharedMath;
    static Path inputs;static PrintWriter output;
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static String seed(Random r)throws Exception{return r==null?"-":String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,Random.class,"seed")).get());}
    static Random math()throws Exception{Class<?> c=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);return(Random)get(null,c,"randomNumberGenerator");}
    static void need(boolean ok,String text){if(!ok)throw new AssertionError(text);}
    static Object value(Object v){if(v==null)return null;if(v instanceof Float)return ConstructorJson.map("F",bits((Float)v));if(v instanceof Double)return ConstructorJson.map("D",bits((Double)v));if(v instanceof Long)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)v));if(v instanceof String||v instanceof Number||v instanceof Boolean)return v;return ConstructorJson.map("class",v.getClass().getName(),"identity",identities.computeIfAbsent(v,k->identities.size()+1));}
    static Map<String,Object> fields(Object o)throws Exception{if(o==null)return null;Map<String,Object> out=new TreeMap<>();for(Class<?> c=o.getClass();c!=Object.class;c=c.getSuperclass())for(Field f:c.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){f.setAccessible(true);out.put(c.getName()+"."+f.getName(),value(f.get(o)));}return out;}
    static Map<String,Object> watcher(sn e)throws Exception{ud w=(ud)get(e,sn.class,"bD");if(w==null)return null;Map map=(Map)get(w,ud.class,"b");List<Integer> keys=new ArrayList<>(map.keySet());Collections.sort(keys);List<Object> out=new ArrayList<>();for(int key:keys){ma r=(ma)map.get(key);out.add(ConstructorJson.map("index",key,"type",r.c(),"value",value(r.b()),"dirty",get(r,ma.class,"d")));}return ConstructorJson.map("dirty",get(w,ud.class,"c"),"records",out);}
    static void role(Map<String,Object> out,String name,Object o){if(o!=null)out.put(name,value(o));}
    static Map<String,Object> roles()throws Exception{Map<String,Object> r=new LinkedHashMap<>();role(r,"actor",actor);role(r,"world",world);role(r,"actor.bounds",actor.aW);role(r,"actor.random",actor.bs);role(r,"actor.watcher",get(actor,sn.class,"bD"));for(int i=0;i<others.size();i++){sn e=others.get(i);String p="secondary"+i;role(r,p,e);role(r,p+".bounds",e.aW);role(r,p+".random",e.bs);role(r,p+".watcher",get(e,sn.class,"bD"));}return r;}
    static Map cache()throws Exception{return(Map)get(get(world,fd.class,"v"),ok.class,"e");}
    static Map<String,Object> state()throws Exception{
        Map<String,Object> r=ConstructorJson.map("fields",fields(actor),"bounds",fields(actor.aW),"watcher",watcher(actor),"own_seed",seed(actor.bs),"world_seed",seed(world.r),"math_seed",seed(sharedMath),"box_cursor",field(eq.class,"h").getInt(null),"vector_cursor",field(bt.class,"e").getInt(null),"loaded_entities",world.b.size(),"players",world.d.size(),"cache_size",cache().size(),"reference_roles",roles());
        List<Object> extra=new ArrayList<>();for(sn e:others)extra.add(ConstructorJson.map("fields",fields(e),"bounds",fields(e.aW),"watcher",watcher(e),"own_seed",seed(e.bs)));r.put("secondary_entities",extra);return r;
    }
    static List<String[]> table(String name)throws Exception{List<String[]> r=new ArrayList<>();List<String> lines=Files.readAllLines(inputs.resolve(name));for(int i=1;i<lines.size();i++)r.add(lines.get(i).split("\t",-1));return r;}
    static fd make()throws Exception{fd w=new fd(new wy(),"MobTickReference",xa.a(0),23L);w.B=false;w.q=1;w.f=0;Map map=(Map)get(get(w,fd.class,"v"),ok.class,"e");map.clear();List loaded=(List)get(get(w,fd.class,"v"),ok.class,"f");loaded.clear();for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){lm c=new lm(w,new byte[32768],x,z);c.n=true;for(int xx=0;xx<16;xx++)for(int zz=0;zz<16;zz++)c.b[(xx<<11)|(zz<<7)|63]=1;Arrays.fill(c.h,(byte)64);c.i=64;map.put(yy.a(x,z),c);loaded.add(c);}return w;}
    static sn construct(String kind,int token,List<Object> contexts)throws Exception{
        String before=sharedMath==null?null:seed(sharedMath);constructor_case=token;constructed=null;sn e=(sn)Class.forName(kind).getDeclaredConstructor(fd.class).newInstance(world);constructed=e;constructor_case=-1;sharedMath=math();
        identities.clear();value(e);value(world);contexts.add(ConstructorJson.map("token",token,"kind",kind,"fields",fields(e),"bounds",fields(e.aW),"watcher",watcher(e),"own_seed",seed(e.bs),"math_seed_before",before,"math_seed_after",seed(sharedMath),"default_entropy_rebound",false));return e;
    }
    static void pose(sn e,String text){String[] a=text.split(",");e.c(Double.parseDouble(a[0]),Double.parseDouble(a[1]),Double.parseDouble(a[2]),Float.parseFloat(a[3]),Float.parseFloat(a[4]));}
    static void size(sn e,String text)throws Exception{Method m=uw.class.getDeclaredMethod("e",int.class);m.setAccessible(true);m.invoke(e,Integer.parseInt(text));}
    static void set(sn e,String owner,String member,String type,String text)throws Exception{
        Object target=owner.equals("fd")?world:e;Field f=field(Class.forName(owner),member);
        switch(type){case "I":f.setInt(target,Integer.parseInt(text));break;case "Z":f.setBoolean(target,Boolean.parseBoolean(text));break;case "F":f.setFloat(target,Float.intBitsToFloat((int)Long.parseUnsignedLong(text,16)));break;case "D":f.setDouble(target,Double.longBitsToDouble(Long.parseUnsignedLong(text,16)));break;case "R":f.set(target,text.equals("NULL")?null:others.get(0));break;default:throw new AssertionError(type);}
    }
    static void common(sn e,String position,long own,long worldSeed)throws Exception{
        for(String[] p:table("common-preparation.tsv")){
            String op=p[0];boolean applies=!op.contains("_when_")||e.getClass().getName().equals(op.substring(op.indexOf("_when_")+6));if(!applies)continue;
            if(op.startsWith("field"))set(e,p[1],p[2],p[3],p[4]);
            else if(p[1].equals("uw"))size(e,p[4]);
            else if(p[1].equals("sn"))pose(e,position==null?p[4]:position);
            else if(p[1].equals("fd"))need(world.b(e),"actual ordinary Entity/Chunk publication FD.b");
            else if(p[4].equals("profile_own_seed"))e.bs.setSeed(own);else world.r.setSeed(worldSeed);
        }
    }
    static void cell(String text)throws Exception{String[] s=text.split(",");int x=Integer.parseInt(s[0]),y=Integer.parseInt(s[1]),z=Integer.parseInt(s[2]),id=Integer.parseInt(s[3]),meta=Integer.parseInt(s[4]);lm c=(lm)cache().get(yy.a(x>>4,z>>4));need(c!=null&&y>=0&&y<128,"authored cell within cached input");c.b[((x&15)<<11)|((z&15)<<7)|y]=(byte)id;c.e.a(x&15,y,z&15,meta);if(id!=0)c.h[((z&15)<<4)|(x&15)]=(byte)Math.max(c.h[((z&15)<<4)|(x&15)]&255,y+1);}
    static void scenario(int id,List<Object> contexts,long worldSeed)throws Exception{
        for(String[] p:table("scenario-preparation.tsv"))if(Integer.parseInt(p[0])==id){String op=p[1],owner=p[2],member=p[3],type=p[4],text=p[5];
            switch(op){case "field":set(actor,owner,member,type,text);break;case "pose":pose(actor,text);break;case "invoke":size(actor,text);break;
                case "watcher":actor.ad().b(Integer.parseInt(member),type.equals("Byte")?(Object)Byte.valueOf(text):(Object)Integer.valueOf(text));break;
                case "cell":cell(text);break;
                case "fill":{String[] s=text.split(",");for(int x=Integer.parseInt(s[0]);x<=Integer.parseInt(s[3]);x++)for(int y=Integer.parseInt(s[1]);y<=Integer.parseInt(s[4]);y++)for(int z=Integer.parseInt(s[2]);z<=Integer.parseInt(s[5]);z++)cell(x+","+y+","+z+","+s[6]+","+s[7]);break;}
                case "secondary":{sn other=construct(owner,id*2+1,contexts);common(other,text,17,worldSeed);others.add(other);break;}
                default:throw new AssertionError(op);
            }
        }
    }
    static final class Observer implements pm {
        final List<String> events=new ArrayList<>();final int id;final String action;
        Observer(int id,String action){this.id=id;this.action=action;}
        public void a(String n,double x,double y,double z,float v,float p){events.add("SOUND "+id+" "+events.size()+" "+n+" "+bits(x)+" "+bits(y)+" "+bits(z)+" "+bits(v)+" "+bits(p));if(action.equals("fail_sound"))throw new IllegalStateException("authored reached sound callback failure");}
        public void a(String n,double x,double y,double z,double a,double b,double c){events.add("PARTICLE "+id+" "+events.size()+" "+n+" "+bits(x)+" "+bits(y)+" "+bits(z)+" "+bits(a)+" "+bits(b)+" "+bits(c));}
        public void a(sn e){events.add("ADD "+id+" "+events.size()+" "+e.getClass().getName()+" "+bits(e.aM)+" "+bits(e.aN)+" "+bits(e.aO));}
        public void b(sn e){events.add("REMOVE "+id+" "+events.size()+" "+e.getClass().getName());}
        public void a(int x,int y,int z){events.add("CELL "+id+" "+events.size()+" "+x+" "+y+" "+z);}
        public void b(int x,int y,int z,int a,int b,int c){events.add("RANGE "+id+" "+events.size()+" "+x+" "+y+" "+z+" "+a+" "+b+" "+c);}
        public void e(){events.add("REFRESH");}public void a(String s,int x,int y,int z){events.add("RECORD");}public void a(int x,int y,int z,ow t){events.add("TILE");}public void a(gs p,int a,int b,int c,int d,int e){events.add("LEVEL");}
    }
    static void tick(String[] p)throws Exception{
        int id=Integer.parseInt(p[0]);world=make();actor=null;others.clear();List<Object> contexts=new ArrayList<>();actor=construct(p[1],id*2,contexts);common(actor,null,Long.parseLong(p[3]),Long.parseLong(p[4]));scenario(id,contexts,Long.parseLong(p[4]));
        Observer observer=new Observer(id,p[5]);currentObserver=observer;world.a(observer);eq.b();bt.b();identities.clear();value(actor);value(world);value(actor.aW);value(actor.bs);value(get(actor,sn.class,"bD"));for(sn e:others)value(e);
        Map<String,Object> before=state();int outcome=1;String failure=null,top=null;active=id;try{actor.w_();}catch(RuntimeException e){outcome=-1;failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}active=-1;
        output.println(ConstructorJson.json(ConstructorJson.map("event","tick","case",id,"kind",p[1],"scenario",p[2],"input",List.of(p),"outcome",outcome,"failure",failure,"top",top,"constructor_contexts",contexts,"before",before,"after",state(),"events",observer.events,"default_constructor_entropy_rebound",false,"actual_entity_published",world.b.contains(actor))));output.flush();System.out.println("TICK "+id+" "+p[1]+" "+outcome);
    }
    static void bindings(Path jar)throws Exception{for(String name:Files.readAllLines(Path.of(System.getProperty("mobtick.classes")))){Class<?> c=Class.forName(name,false,MobTickTarget.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");byte[] data;try(InputStream in=c.getResourceAsStream("/"+name.replace('.','/')+".class")){data=in.readAllBytes();}System.out.println("BIND "+name+" "+data.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data)));}}
    public static void main(String[] args)throws Exception{new gr("MobTickSetup","");inputs=Path.of(args[2]);try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){output=out;for(String[] p:table("profiles.tsv"))tick(p);if(out.checkError())throw new IOException("output");}bindings(Path.of(args[1]).toRealPath());}
}
