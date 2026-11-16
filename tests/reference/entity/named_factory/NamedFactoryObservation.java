import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.atomic.AtomicLong;

/** Authored observer only. Original game classes are loaded from the verified JAR. */
public final class NamedFactoryObservation {
    static final String[] CORE={"jc","sn","ls","ii","bg","wh","ud","ma","eq","fd","mx","ei","lm","rh","xa","ok","ld","qj","rj","sk","ow","uu","gm"};
    static final IdentityHashMap<sn,String> entities=new IdentityHashMap<sn,String>();
    static final List<sn> strong=new ArrayList<sn>();
    static final List<Object> writeChecks=new ArrayList<Object>();
    static int network,natives,execs,deniedWrites;
    static fd world;
    static lm chunk;
    static sn firstPig;
    static Integer previousId;
    static void need(boolean v,String m){if(!v)throw new AssertionError(m);}
    static Object get(Object v,Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f.get(v);}
    static Map<String,Object> map(Object...v){Map<String,Object> r=new LinkedHashMap<String,Object>();for(int i=0;i<v.length;i+=2)r.put((String)v[i],v[i+1]);return r;}
    static String sha(byte[] raw)throws Exception{StringBuilder r=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))r.append(String.format("%02x",b&255));return r.toString();}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String){StringBuilder r=new StringBuilder("\"");for(char c:((String)v).toCharArray()){if(c=='"'||c=='\\')r.append('\\').append(c);else if(c<32||c>=127)r.append(String.format("\\u%04x",(int)c));else r.append(c);}return r.append('"').toString();}
        if(v instanceof Map){StringBuilder r=new StringBuilder("{");for(Map.Entry<?,?> e:((Map<?,?>)v).entrySet()){if(r.length()>1)r.append(',');r.append(json(e.getKey())).append(':').append(json(e.getValue()));}return r.append('}').toString();}
        if(v instanceof Iterable){StringBuilder r=new StringBuilder("[");for(Object e:(Iterable<?>)v){if(r.length()>1)r.append(',');r.append(json(e));}return r.append(']').toString();}
        need(v instanceof Number||v instanceof Boolean,"closed JSON observation type");return v.toString();
    }
    static String bits(Object v){return v instanceof Float?String.format("%08x",Float.floatToRawIntBits((Float)v)):String.format("%016x",Double.doubleToRawLongBits((Double)v));}
    static List<Object> units(String v){if(v==null)return null;List<Object> r=new ArrayList<Object>();for(int i=0;i<v.length();++i)r.add((int)v.charAt(i));return r;}
    static String label(sn e){if(e==null)return null;String r=entities.get(e);if(r==null){r="E"+entities.size();entities.put(e,r);strong.add(e);}return r;}
    static List<Object> entityList(List<?> values){List<Object> r=new ArrayList<Object>();for(Object v:values){need(v instanceof sn,"actual World Entity list");r.add(label((sn)v));}return r;}
    static Object worldState()throws Exception{
        if(world==null)return null;
        ei info=(ei)get(world,fd.class,"x");
        List<Object> tiles=new ArrayList<Object>();for(Object v:world.c){ow t=(ow)v;tiles.add(map("class",t.getClass().getName(),"xyz",List.of(t.e,t.f,t.g)));}
        return map("class",world.getClass().getName(),"remote",world.B,"time",get(info,ei.class,"e"),"name",get(info,ei.class,"j"),
            "provider_class",world.t.getClass().getName(),"provider_fields",List.of(world.t.c,world.t.d,world.t.e,world.t.g),"chunk_provider_class",get(world,fd.class,"v").getClass().getName(),
            "chunk",List.of(chunk.j,chunk.k),"chunk_loaded",chunk.c,"block",world.a(0,64,2),"metadata",world.e(0,64,2),
            "entities",entityList(world.b),"players",entityList(world.d),"tiles",tiles,"pending_tiles",((List<?>)get(world,fd.class,"G")).size());
    }
    static Object state(sn e)throws Exception{
        if(e==null)return null;
        Object actualWorld=get(e,sn.class,"aI");
        Map<String,Object> stable=map("label",label(e),"class",e.getClass().getName(),"id",get(e,sn.class,"aD"),"removed",get(e,sn.class,"be"),
            "world",actualWorld==null?null:actualWorld==world?"W0":"foreign", "world_class",actualWorld==null?null:actualWorld.getClass().getName(),
            "width",bits(get(e,sn.class,"bg")),"height",bits(get(e,sn.class,"bh")),"offset",bits(get(e,sn.class,"bf")),
            "position",List.of(bits(get(e,sn.class,"aM")),bits(get(e,sn.class,"aN")),bits(get(e,sn.class,"aO"))));
        if(e instanceof ls){stable.put("health",get(e,ls.class,"Y"));stable.put("texture",get(e,ls.class,"O"));}
        Map<String,Object> watchers=new TreeMap<String,Object>();
        Object watcher=get(e,sn.class,"bD");for(Map.Entry<?,?> x:((Map<?,?>)get(watcher,ud.class,"b")).entrySet()){
            Object v=((ma)x.getValue()).b();watchers.put(x.getKey().toString(),map("class",v.getClass().getName(),"value",v));}
        stable.put("watcher",watchers);
        Map<String,Object> entropy=map();
        if(e instanceof ls)for(String n:new String[]{"aj","G","F"})entropy.put("ls."+n,bits(get(e,ls.class,n)));
        entropy.put("sn.aS",bits(get(e,sn.class,"aS")));
        Object random=get(e,sn.class,"bs");entropy.put("sn.bs.seed",((AtomicLong)get(random,java.util.Random.class,"seed")).get());
        return map("stable",stable,"nondeterministic",entropy);
    }
    static Object registry()throws Exception{
        List<Object> maps=new ArrayList<Object>();
        for(String n:new String[]{"a","b","c","d"}){
            Map<?,?> actual=(Map<?,?>)get(null,jc.class,n);Map<String,Object> entries=new TreeMap<String,Object>();
            for(Map.Entry<?,?> e:actual.entrySet()){
                String k=e.getKey() instanceof Class?((Class<?>)e.getKey()).getName():e.getKey().toString();
                Object v=e.getValue() instanceof Class?((Class<?>)e.getValue()).getName():e.getValue();entries.put(k,v);}
            maps.add(map("field",n,"class",actual.getClass().getName(),"size",actual.size(),"entries",entries));
        }
        return maps;
    }
    static void row(String prefix,Object v){System.out.println(prefix+json(v));System.out.flush();}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        need(args.length==5,"jar copied-saves declarations serialized-name seed");
        Path jar=Path.of(args[0]).toRealPath(),saves=Path.of(args[1]).toRealPath(),owned=saves.resolve("alpha").toRealPath();
        need(owned.getParent().equals(saves),"owned existing contained copied World");Path lock=owned.resolve("session.lock");
        List<Path> regions=new ArrayList<Path>();try(java.util.stream.Stream<Path> paths=Files.list(owned.resolve("region"))){paths.filter(p->p.getFileName().toString().endsWith(".mcr")).forEach(p->{need(Files.isRegularFile(p)&&!Files.isSymbolicLink(p),"existing regular copied region");regions.add(p.toAbsolutePath().normalize());});}
        Map<String,Object> origins=map();for(String n:CORE){Class<?> c=Class.forName(n,false,NamedFactoryObservation.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"actual original JAR class owner");try(InputStream in=c.getResourceAsStream("/"+n+".class")){need(in!=null,"original resource");origins.put(n,sha(in.readAllBytes()));}}
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
            public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String n){++natives;throw new SecurityException("native link");}
            public void checkExec(String n){++execs;throw new SecurityException("process");}
            public void checkWrite(String n){Path p=Path.of(n).toAbsolutePath().normalize();String role=p.equals(lock)?"owned-session-lock":p.equals(owned)?"existing-world-directory":regions.contains(p)?"existing-copied-region-rw-open":null;writeChecks.add(map("operation","write","role",role));if(role==null){++deniedWrites;throw new SecurityException("outside owned copied read paths");}}
            public void checkDelete(String n){++deniedWrites;throw new SecurityException("delete");}
        });
        Class.forName("uu");Class.forName("gm");
        row("NAMED_FACTORY_ORIGINS ",map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins));
        List<String> lines=Files.readAllLines(Path.of(args[2]));need(lines.size()==10,"ten declared calls");
        String firstName=null;int index=0;Object firstRegistry=null;
        for(String line:lines){
            String[] v=line.split("\t",-1);need(v.length==4&&Integer.parseInt(v[0])==index,"ordered declared call");
            if(index==7){world=new fd(new mx(saves.toFile(),"alpha",false),args[3],Long.parseLong(args[4]));chunk=world.c(0,0);need(chunk!=null&&chunk.getClass()==lm.class,"actual original loaded Chunk");row("NAMED_FACTORY_WORLD ",worldState());}
            String name=v[1].equals("~null")?null:v[1];if(v[3].equals("true"))name=new String(name);
            if(index==5)firstName=name;
            fd argument=v[2].equals("W0")?world:null;need(!v[2].equals("W0")||argument!=null,"prepared actual World argument");
            Object before=worldState();sn result=null;Throwable escaped=null;
            try{result=jc.a(name,argument);}catch(Throwable actual){escaped=actual;}
            if(firstRegistry==null)firstRegistry=registry();
            Object observed=state(result);Integer id=result==null?null:(Integer)get(result,sn.class,"aD");
            row("NAMED_FACTORY_ROW ",map("index",index,"name_utf16",units(name),"world_argument",argument==null?null:"W0","distinct_string",v[3].equals("true"),
                "name_same_as_first_pig",firstName!=null&&name==firstName,"name_equals_first_pig",firstName!=null&&firstName.equals(name),
                "returned",escaped==null,"escaped_class",escaped==null?null:escaped.getClass().getName(),"escaped_message",escaped==null?null:escaped.getMessage(),
                "result",observed,"entity_id_delta",id==null||previousId==null?null:id-previousId,"world_before",before,"world_after",worldState()));
            if(id!=null)previousId=id;if(index==5)firstPig=result;++index;
        }
        need(firstPig!=null,"actual case5 receiver for original rebind");
        Object before=state(firstPig),beforeWorld=worldState();
        firstPig.a(world);
        row("NAMED_FACTORY_REBIND ",map("operation","sn.a(Lfd;)V","before",before,"after",state(firstPig),"world_before",beforeWorld,"world_after",worldState()));
        row("NAMED_FACTORY_REGISTRY ",map("after_first_call",firstRegistry,"after_all_calls",registry()));
        row("NAMED_FACTORY_COMPLETE ",map("rows",index,"rebinds",1,"retained_entities",entities.size(),"network",network,"native",natives,"execs",execs,"denied_writes",deniedWrites,"write_checks",writeChecks));
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no forbidden external attempts");
    }
}
