import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.IdentityHashMap;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
import java.util.Map;

/** Read copied native save bytes through genuine unchanged original owners. */
public final class SpawnerTickObservation {
    static int network,natives,execs,deniedWrites;
    static final List<Object> writeChecks=new ArrayList<Object>();
    static final String[] CORE={"cy","ow","fd","mx","fm","ei","lm","rh","xa","ok","ld","qj","rj","rw","nu","sp","ij","as","uu","gm","jc","sn","ls","ii","bg","wh","sl","gs","dc","gr","pm","eq","ud","ma"};
    static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Object get(Object owner,Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f.get(owner);}
    static Map<String,Object> map(Object...values){Map<String,Object> out=new LinkedHashMap<String,Object>();for(int i=0;i<values.length;i+=2)out.put((String)values[i],values[i+1]);return out;}
    static String json(Object value){
        if(value==null)return "null";
        if(value instanceof String){StringBuilder out=new StringBuilder("\"");for(char c:((String)value).toCharArray()){if(c=='"'||c=='\\')out.append('\\').append(c);else if(c<32||c>=127)out.append(String.format("\\u%04x",(int)c));else out.append(c);}return out.append('"').toString();}
        if(value instanceof Map){StringBuilder out=new StringBuilder("{");for(Map.Entry<?,?> e:((Map<?,?>)value).entrySet()){if(out.length()>1)out.append(',');out.append(json(e.getKey())).append(':').append(json(e.getValue()));}return out.append('}').toString();}
        if(value instanceof Iterable){StringBuilder out=new StringBuilder("[");for(Object v:(Iterable<?>)value){if(out.length()>1)out.append(',');out.append(json(v));}return out.append(']').toString();}
        need(value instanceof Number||value instanceof Boolean,"closed JSON type");return value.toString();
    }
    static String sha(byte[] raw)throws Exception{StringBuilder out=new StringBuilder();for(byte v:MessageDigest.getInstance("SHA-256").digest(raw))out.append(String.format("%02x",v&255));return out.toString();}


    static final IdentityHashMap<sn,String> labels=new IdentityHashMap<sn,String>();
    static final List<sn> retained=new ArrayList<sn>();
    static fd world;static cy tile;static List<lm> chunks;static Sink sink;
    static String label(sn value){if(value==null)return null;String text=labels.get(value);if(text==null){text="E"+labels.size();labels.put(value,text);retained.add(value);}return text;}
    static String db(double v){return String.format("%016x",Double.doubleToRawLongBits(v));}
    static String fb(float v){return String.format("%08x",Float.floatToRawIntBits(v));}
    static Object rng(Random value)throws Exception{return value==null?null:map("seed",((AtomicLong)get(value,Random.class,"seed")).get(),"cached",get(value,Random.class,"haveNextNextGaussian"),"gaussian_bits",db((Double)get(value,Random.class,"nextNextGaussian")));}
    static Object math()throws Exception{Class<?> type=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);return rng((Random)get(null,type,"randomNumberGenerator"));}
    static int counter()throws Exception{return (Integer)get(null,sn.class,"a");}
    static Object entity(sn e)throws Exception {
        List<Object> ctor=new ArrayList<Object>();if(e instanceof ls)for(String n:new String[]{"aj","G","F"})ctor.add(fb((Float)get(e,ls.class,n)));
        eq box=e.aW;
        return map("label",label(e),"class",e.getClass().getName(),"id",get(e,sn.class,"aD"),"world_same",e.aI==world,"removed",get(e,sn.class,"be"),
            "position_bits",List.of(db(e.aM),db(e.aN),db(e.aO)),"yaw_bits",fb(e.aS),"pitch_bits",fb(e.aT),"size_bits",List.of(fb(e.bg),fb(e.bh),fb(e.bf)),
            "bounds_bits",List.of(db(box.a),db(box.b),db(box.c),db(box.d),db(box.e),db(box.f)),"random",rng(e.bs),"living_ctor_bits",ctor);
    }
    static Object ids(List<?> values){List<Object> out=new ArrayList<Object>();for(Object raw:values){need(raw instanceof sn,"actual Entity list occurrence");sn e=(sn)raw;out.add(map("label",label(e),"class",e.getClass().getName(),"id",getUnchecked(e,sn.class,"aD")));}return out;}
    static Object getUnchecked(Object o,Class<?> c,String n){try{return get(o,c,n);}catch(Exception e){throw new RuntimeException(e);}}
    static Object tileList(List<?> values)throws Exception{List<Object> out=new ArrayList<Object>();for(Object raw:values){ow t=(ow)raw;out.add(map("class",t.getClass().getName(),"same_selected",t==tile,"world_same",t.d==world,"xyz",List.of(t.e,t.f,t.g),"invalid",get(t,ow.class,"h")));}return out;}
    static Object state()throws Exception {
        List<Object> stores=new ArrayList<Object>();for(lm chunk:chunks){List<Object> slices=new ArrayList<Object>();for(Object raw:chunk.m)slices.add(ids((List<?>)raw));stores.add(map("chunk",List.of(chunk.j,chunk.k),"entities",slices));}
        List<Object> actors=new ArrayList<Object>();for(sn actor:new ArrayList<sn>(retained))actors.add(entity(actor));
        ei info=(ei)get(world,fd.class,"x");
        return map("counter",counter(),"delay",tile.a,"rotation_bits",db(tile.b),"previous_rotation_bits",db(tile.c),"name",tile.a(),"world_time",get(info,ei.class,"e"),
            "world_random",rng(world.r),"math_random",math(),"entities",ids(world.b),"players",ids(world.d),"active_tiles",tileList(world.c),"pending_tiles",tileList((List<?>)get(world,fd.class,"G")),
            "world_L",get(world,fd.class,"L"),"chunks",stores,"retained_entities",actors,"eq_cursor",get(null,eq.class,"h"));
    }
    static final class Sink implements pm {
        final List<Object> events=new ArrayList<Object>();
        void record(Object value){need(events.size()<4096,"bounded passive callback log");events.add(value);}
        public void a(sn e){try{label(e);record(map("event","entity_added","entity",entity(e),"delay",tile.a,"world_random",rng(world.r),"world_entity_count",world.b.size()));}catch(Exception failure){throw new RuntimeException(failure);}}
        public void a(String name,double x,double y,double z,double vx,double vy,double vz){try{record(map("event","particle","name",name,"position_bits",List.of(db(x),db(y),db(z)),"velocity_bits",List.of(db(vx),db(vy),db(vz)),"delay",tile.a,"world_random",rng(world.r)));}catch(Exception failure){throw new RuntimeException(failure);}}
        public void a(int x,int y,int z){record(map("event","block_changed","xyz",List.of(x,y,z)));}
        public void b(int a,int b,int c,int d,int e,int f){record(map("event","range_changed","args",List.of(a,b,c,d,e,f)));}
        public void a(String n,double x,double y,double z,float v,float p){record(map("event","sound","name",n));}
        public void b(sn e){record(map("event","entity_removed","entity",label(e)));}
        public void e(){record(map("event","refresh"));}
        public void a(String n,int x,int y,int z){record(map("event","record","name",n));}
        public void a(int x,int y,int z,ow t){record(map("event","tile_changed"));}
        public void a(gs p,int a,int b,int c,int d,int e){record(map("event","aux"));}
    }
    static Object failure(Throwable t){if(t==null)return null;List<Object> frames=new ArrayList<Object>();for(StackTraceElement frame:t.getStackTrace())frames.add(map("class",frame.getClassName(),"method",frame.getMethodName(),"file",frame.getFileName(),"line",frame.getLineNumber()));return map("class",t.getClass().getName(),"message",t.getMessage(),"frames",frames);}
    static void run(String[] profile,Path saves)throws Exception {
        int index=Integer.parseInt(profile[0]),prep=Integer.parseInt(profile[3]);labels.clear();retained.clear();
        world=new fd(new mx(saves.toFile(),profile[1],false),profile[4],Long.parseLong(profile[5]));chunks=new ArrayList<lm>();
        for(int cx=-1;cx<=0;++cx)for(int cz=-1;cz<=0;++cz){lm c=world.c(cx,cz);need(c!=null&&c.getClass()==lm.class&&c.j==cx&&c.k==cz,"actual prepared saved normal Chunk identity");chunks.add(c);}
        tile=null;for(lm c:chunks)for(Object raw:c.l.values()){ow t=(ow)raw;if(t.e==0&&t.f==64&&t.g==2){need(t.getClass()==cy.class&&tile==null,"one actual saved CY");tile=(cy)t;}}
        need(tile!=null&&tile.a==0&&tile.a().equals(profile[2])&&tile.d==world,"exact authored saved Tile before source entry");
        gr session=new gr("SpawnerReferencePlayer","");dc player=new dc(null,world,session,0);label(player);player.c(14.5,64,2.5,0,0);need(world.b(player),"actual Player publication");
        for(int i=0;i<prep;++i){sn actor=jc.a("Pig",world);need(actor!=null&&actor.getClass()==wh.class,"actual prep Pig factory");label(actor);actor.c(2.5,64,2.5,0,0);need(world.b(actor),"actual prep Pig publication");}
        sink=new Sink();world.a(sink);
        Object before=state();Throwable escaped=null;try{tile.n_();}catch(Throwable actual){escaped=actual;}
        Object after=state();
        System.out.println("SPAWNER_TICK_ROW "+json(map("index",index,"profile",List.of(profile),"before",before,"returned",escaped==null,"failure",failure(escaped),"events",sink.events,"after",after)));
        world.b(sink);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        need(args.length==3,"jar owned-copy-saves profiles");Path jar=Path.of(args[0]).toRealPath(),saves=Path.of(args[1]).toRealPath();
        final List<Path> locks=new ArrayList<Path>(),regions=new ArrayList<Path>(),folders=new ArrayList<Path>();
        for(String line:Files.readAllLines(Path.of(args[2]))){String[] v=line.split("\t",-1);Path owned=saves.resolve(v[1]).toRealPath();need(owned.getParent().equals(saves),"owned contained existing World");folders.add(owned);locks.add(owned.resolve("session.lock"));try(java.util.stream.Stream<Path> files=Files.list(owned.resolve("region"))){files.forEach(p->{need(Files.isRegularFile(p)&&!Files.isSymbolicLink(p),"regular existing owned region");regions.add(p.toAbsolutePath().normalize());});}}
        Map<String,Object> origins=map();for(String n:CORE){Class<?> c=Class.forName(n,false,SpawnerTickObservation.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"actual original Class CodeSource");try(InputStream in=c.getResourceAsStream("/"+n+".class")){need(in!=null,"original Class resource");origins.put(n,sha(in.readAllBytes()));}}
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String n){++natives;throw new SecurityException("native link");}public void checkExec(String n){++execs;throw new SecurityException("process");}
            public void checkWrite(String n){Path p=Path.of(n).toAbsolutePath().normalize();String role=locks.contains(p)?"owned-session-lock":folders.contains(p)?"existing-world-directory":regions.contains(p)?"existing-owned-region-rw-open":null;writeChecks.add(map("operation","write","role",role));if(role==null){++deniedWrites;throw new SecurityException("outside declared owned copies");}}
            public void checkDelete(String n){++deniedWrites;throw new SecurityException("delete");}
        });
        Class.forName("uu");Class.forName("gm");
        System.out.println("SPAWNER_TICK_ORIGINS "+json(map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins)));
        int index=0;for(String line:Files.readAllLines(Path.of(args[2]))){String[] v=line.split("\t",-1);need(v.length==6&&Integer.parseInt(v[0])==index,"closed ordered profile");run(v,saves);++index;}
        System.out.println("SPAWNER_TICK_COMPLETE "+json(map("rows",index,"network",network,"native",natives,"execs",execs,"denied_writes",deniedWrites,"write_checks",writeChecks)));
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no forbidden external attempts");
    }
}
