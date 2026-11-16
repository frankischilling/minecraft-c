import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Read copied native save bytes through genuine unchanged original owners. */
public final class FurnaceReadback {
    static int network,natives,execs,deniedWrites;
    static final List<Object> writeChecks=new ArrayList<Object>();
    static final String[] CORE={"fd","lm","mx","ei","sk","ow","tc","rw","nu","sp","ij","iz","ix","gm","uu","ln","rh","ok","to","ld","wi","xa","qj","rj"};
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
    static Object stack(iz value)throws Exception{return value==null?null:List.of(value.c,value.a,(Integer)get(value,iz.class,"d"),value.b);}
    static List<Object> slots(iz[] values)throws Exception{List<Object> out=new ArrayList<Object>();for(iz value:values)out.add(stack(value));return out;}
    static Object tile(sk value,fd world)throws Exception{return map("class",value.getClass().getName(),"world_same",value.d==world,"xyz",List.of(value.e,value.f,value.g),"invalid",get(value,ow.class,"h"),"fields",List.of(value.a,value.b,value.c),"slots",slots((iz[])get(value,sk.class,"i")));}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        need(args.length==7,"jar owned copied saves world-folder x y z serialized-name");
        Path jar=Path.of(args[0]).toRealPath(),saves=Path.of(args[1]).toRealPath(),owned=saves.resolve(args[2]).toRealPath();
        need(owned.getParent().equals(saves),"one contained existing copied World");Path lock=owned.resolve("session.lock");
        final List<Path> existingRegions=new ArrayList<Path>();
        try(java.util.stream.Stream<Path> paths=Files.list(owned.resolve("region"))){paths.filter(p->p.getFileName().toString().endsWith(".mcr")).forEach(p->{need(Files.isRegularFile(p)&&!Files.isSymbolicLink(p),"existing copied region only");existingRegions.add(p.toAbsolutePath().normalize());});}
        int x=Integer.parseInt(args[3]),y=Integer.parseInt(args[4]),z=Integer.parseInt(args[5]);
        Map<String,Object> origins=map();
        for(String name:CORE){Class<?> type=Class.forName(name,false,FurnaceReadback.class.getClassLoader());need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original JAR class owner");try(InputStream in=type.getResourceAsStream("/"+name+".class")){need(in!=null,"original resource");origins.put(name,sha(in.readAllBytes()));}}
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
            public void checkListen(int port){++network;throw new SecurityException("listen");}
            public void checkLink(String name){++natives;throw new SecurityException("native link");}
            public void checkExec(String name){++execs;throw new SecurityException("process");}
            public void checkWrite(String name){Path p=Path.of(name).toAbsolutePath().normalize();String role=p.equals(owned)?"existing-world-directory":p.equals(lock)?"owned-session-lock":existingRegions.contains(p)?"existing-copied-region-rw-open":null;writeChecks.add(map("operation","write","role",role));if(role==null){++deniedWrites;throw new SecurityException("outside allowed existing copied read paths");}}
            public void checkDelete(String name){++deniedWrites;throw new SecurityException("delete");}
        });
        Class.forName("uu");Class.forName("gm");
        fd world=new fd(new mx(saves.toFile(),args[2],false),args[6],123L);
        ei info=(ei)get(world,fd.class,"x");
        lm chunk=world.c(x>>4,z>>4);
        need(chunk!=null&&chunk.getClass()==lm.class,"actual loaded Chunk");
        List<Object> mapped=new ArrayList<Object>();sk loaded=null;
        for(Object raw:chunk.l.values()){ow value=(ow)raw;mapped.add(map("class",value.getClass().getName(),"xyz",List.of(value.e,value.f,value.g)));if(value.e==x&&value.f==y&&value.g==z){need(value.getClass()==sk.class,"actual SK from saved TileEntities factory");loaded=(sk)value;}}
        need(loaded!=null,"saved Furnace exists before any lazy World Tile lookup");
        ow lookup=world.b(x,y,z);
        nu player=info.h();need(player!=null,"actual original WorldInfo serialized Player");
        ix inventory=new ix((gs)null);inventory.b(player.l("Inventory"));
        List<Object> active=new ArrayList<Object>();
        for(Object raw:world.c){ow value=(ow)raw;active.add(map("class",value.getClass().getName(),"same_saved_furnace",value==loaded,"xyz",List.of(value.e,value.f,value.g)));}
        xa provider=world.t;
        Object row=map("world_class",world.getClass().getName(),"remote",world.B,"provider_class",provider.getClass().getName(),"provider_fields",List.of(provider.c,provider.d,provider.e,provider.g),
            "chunk_provider_class",get(world,fd.class,"v").getClass().getName(),"chunk",List.of(chunk.j,chunk.k),"chunk_loaded",chunk.c,
            "metadata",map("seed",get(info,ei.class,"a"),"spawn",List.of(get(info,ei.class,"b"),get(info,ei.class,"c"),get(info,ei.class,"d")),"time",get(info,ei.class,"e"),"last_played",get(info,ei.class,"f"),"size_on_disk",get(info,ei.class,"g"),"dimension",get(info,ei.class,"i"),"version",get(info,ei.class,"k"),"name",get(info,ei.class,"j")),
            "block",world.a(x,y,z),"block_metadata",world.e(x,y,z),"map_before_lookup",mapped,"lookup_same",lookup==loaded,
            "furnace",tile(loaded,world),"active",active,"pending",((List<?>)get(world,fd.class,"G")).size(),
            "player_inventory_owner_class",inventory.getClass().getName(),"player_reference_null",inventory.d==null,
            "player_main",slots(inventory.a),"player_armor",slots(inventory.b));
        System.out.println("FURNACE_READBACK_ORIGINS "+json(map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins)));
        System.out.println("FURNACE_READBACK_ROW "+json(row));
        System.out.println("FURNACE_READBACK_COMPLETE "+json(map("rows",1,"network",network,"native",natives,"execs",execs,"denied_writes",deniedWrites,"write_checks",writeChecks)));
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no forbidden external attempts");
    }
}
