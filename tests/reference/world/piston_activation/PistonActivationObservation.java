import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Genuine stable saved Block activation and FD.g; observation never repairs fields. */
public final class PistonActivationObservation {
    static final String[] CORE={"xr","jq","h","ut","uk","ow","wj","uu","gm","fd","lm","mx","fm","ei","rh","xa","ok","ld","nu","rw","pm","eq","yy","wi"};
    static int network,natives,execs,deniedWrites;
    static final List<Object> writes=new ArrayList<>();
    static void need(boolean v,String message){if(!v)throw new AssertionError(message);}
    static Map<String,Object> map(Object...v){return ConstructorJson.map(v);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static String f(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static String d(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static String sha(byte[] raw)throws Exception{StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    static Object rng(Random r)throws Exception{return map("seed",((AtomicLong)get(r,Random.class,"seed")).get(),"cached",get(r,Random.class,"haveNextNextGaussian"),"gaussian_bits",d((Double)get(r,Random.class,"nextNextGaussian")));}
    static final class Observation implements pm {
        final fd world;final lm chunk;final uu base;
        final List<ow> roots=new ArrayList<>();final List<Object> events=new ArrayList<>();
        String stage="initial";
        Observation(fd w,lm c,uu b){world=w;chunk=c;base=b;}
        String label(ow value){if(value==null)return null;for(int i=0;i<roots.size();i++)if(roots.get(i)==value)return "T"+i;roots.add(value);return "T"+(roots.size()-1);}
        Object tile(ow v)throws Exception {
            if(v==null)return null;
            Map<String,Object> out=map("label",label(v),"class",v.getClass().getName(),"world_same",v.d==world,"xyz",List.of(v.e,v.f,v.g),"invalid",get(v,ow.class,"h"));
            if(v instanceof uk){uk t=(uk)v;out.put("piston",map("block",t.a(),"metadata",t.e(),"facing",t.d(),"extending",t.b(),"render_head",t.k(),"current_bits",f((Float)get(t,uk.class,"k")),"previous_bits",f((Float)get(t,uk.class,"l"))));}
            return out;
        }
        Object tiles(List<?> list)throws Exception{List<Object> out=new ArrayList<>();for(Object v:list)out.add(tile((ow)v));return out;}
        Object mapTiles()throws Exception{List<Object> out=new ArrayList<>();for(Object raw:chunk.l.entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)raw;Object key=e.getKey();Map<String,Object> fields=new TreeMap<>();for(Field field:key.getClass().getDeclaredFields())if(!java.lang.reflect.Modifier.isStatic(field.getModifiers())&&field.getType().isPrimitive()){field.setAccessible(true);fields.put(field.getName(),field.get(key));}out.add(map("key_class",key.getClass().getName(),"key_fields",fields,"tile",tile((ow)e.getValue())));}return out;}
        Object cells(){List<Object> out=new ArrayList<>();for(int z=0;z<16;z++)out.add(List.of(8,64,z,world.a(8,64,z),world.e(8,64,z)));out.add(List.of(7,64,1,world.a(7,64,1),world.e(7,64,1)));out.add(List.of(7,64,2,world.a(7,64,2),world.e(7,64,2)));return out;}
        Object snapshot()throws Exception {
            Object active=tiles(world.c),pending=tiles((List<?>)get(world,fd.class,"G")),mapped=mapTiles();
            List<Object> retained=new ArrayList<>();for(int i=0;i<roots.size();i++)retained.add(tile(roots.get(i)));
            ei info=(ei)get(world,fd.class,"x");
            return map("world_class",world.getClass().getName(),"world_time",get(info,ei.class,"e"),"world_name",get(info,ei.class,"j"),"world_seed",get(info,ei.class,"a"),"remote",world.B,"iterating",get(world,fd.class,"L"),"entity_count",world.b.size(),"player_count",world.d.size(),"weather_entities",world.e.size(),"unloaded_entities",((List<?>)get(world,fd.class,"D")).size(),"loaded_chunks",((Map<?,?>)get(get(world,fd.class,"v"),ok.class,"e")).size(),"chunk_flags",map("loaded",chunk.c,"terrain_populated",chunk.n,"dirty",chunk.o,"never_save",chunk.p,"has_entities",chunk.q,"last_save_time",chunk.r,"minimum_height",chunk.i),"chunk_array_sha256",map("Blocks",sha(chunk.b),"Data",sha((byte[])get(chunk.e,wi.class,"a")),"SkyLight",sha((byte[])get(chunk.f,wi.class,"a")),"BlockLight",sha((byte[])get(chunk.g,wi.class,"a")),"HeightMap",sha(chunk.h)),"active",active,"pending",pending,"map",mapped,"retained",retained,"cells",cells(),"suppression",get(base,jq.class,"b"),"world_random",rng(world.r));
        }
        void event(String kind,Object args){events.add(map("index",events.size(),"stage",stage,"kind",kind,"arguments",args));}
        public void a(int x,int y,int z){event("cell",List.of(x,y,z));}
        public void b(int x,int y,int z,int a,int b,int c){event("range",List.of(x,y,z,a,b,c));}
        public void a(String name,double x,double y,double z,float volume,float pitch){try{events.add(map("index",events.size(),"stage",stage,"kind","sound","name",name,"position_bits",List.of(d(x),d(y),d(z)),"volume_bits",f(volume),"pitch_bits",f(pitch),"state",snapshot()));}catch(Exception e){throw new AssertionError("passive sound observation",e);}}
        public void a(String name,double x,double y,double z,double a,double b,double c){event("particle",map("name",name,"position_bits",List.of(d(x),d(y),d(z)),"velocity_bits",List.of(d(a),d(b),d(c))));}
        public void a(sn entity){event("entity_added",entity==null?null:entity.getClass().getName());}
        public void b(sn entity){event("entity_removed",entity==null?null:entity.getClass().getName());}
        public void e(){event("sky",null);}
        public void a(String name,int x,int y,int z){event("music",map("name",name,"xyz",List.of(x,y,z)));}
        public void a(int x,int y,int z,ow value){event("tile_dirty",map("xyz",List.of(x,y,z),"tile",label(value)));}
        public void a(gs player,int a,int x,int y,int z,int b){event("auxiliary",map("player_class",player==null?null:player.getClass().getName(),"arguments",List.of(a,x,y,z,b)));}
        Object step(String name,boolean activation)throws Exception{
            stage=name;int start=events.size();Throwable failure=null;Boolean result=null;
            try{if(activation)result=uu.m[69].a(world,7,64,1,(gs)null);else world.g();}catch(Throwable error){failure=error;}
            List<Object> frames=new ArrayList<>();if(failure!=null)for(StackTraceElement frame:failure.getStackTrace())frames.add(map("class",frame.getClassName(),"method",frame.getMethodName(),"line",frame.getLineNumber()));
            return map("stage",name,"operation",activation?"XR.a(FD,III,NULL)":"FD.g()V","returned",failure==null,"activation_result",result,"failure",failure==null?null:map("class",failure.getClass().getName(),"message",failure.getMessage(),"frames",frames),"event_start",start,"event_end",events.size(),"state",snapshot());
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        need(args.length==3,"jar owned-saves profiles");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),saves=Path.of(args[1]).toRealPath(),profiles=Path.of(args[2]).toRealPath();
        List<String[]> cases=new ArrayList<>();Set<Path> locks=new HashSet<>(),regions=new HashSet<>(),dirs=new HashSet<>();
        for(String line:Files.readAllLines(profiles)){String[] row=line.split("\\t",-1);need(row.length==7,"closed profile columns");cases.add(row);Path folder=saves.resolve(row[1]).toRealPath();need(folder.getParent().equals(saves),"contained owned World");dirs.add(folder);locks.add(folder.resolve("session.lock"));regions.add(folder.resolve("region/r.0.0.mcr"));}
        Map<String,Object> origins=map();for(String name:CORE){Class<?> c=Class.forName(name,false,PistonActivationObservation.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"actual original Class owner");try(InputStream in=c.getResourceAsStream("/"+name+".class")){need(in!=null,"original Class resource");origins.put(name,sha(in.readAllBytes()));}}
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){network++;throw new SecurityException("network");}
            public void checkListen(int p){network++;throw new SecurityException("listener");}
            public void checkLink(String p){natives++;throw new SecurityException("native link");}
            public void checkExec(String p){execs++;throw new SecurityException("process");}
            public void checkDelete(String p){deniedWrites++;throw new SecurityException("delete");}
            public void checkWrite(String name){Path p=Path.of(name).toAbsolutePath().normalize();String role=locks.contains(p)?"owned-session-lock":regions.contains(p)?"existing-owned-region-rw-open":dirs.contains(p)?"existing-world-directory":null;writes.add(map("role",role));if(role==null){deniedWrites++;throw new SecurityException("write outside declared copied input");}}
        });
        Class.forName("uu");Class.forName("gm");
        System.out.println("PISTON_ACTIVATION_ORIGINS "+ConstructorJson.json(map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins)));
        for(String[] profile:cases){
            int index=Integer.parseInt(profile[0]);boolean blocked=Boolean.parseBoolean(profile[6]);
            fd world=new fd(new mx(saves.toFile(),profile[1],false),"Piston",-99L);lm chunk=world.c(0,0);
            need(chunk!=null&&chunk.getClass()==lm.class&&chunk.l.isEmpty()&&world.c.isEmpty()&&world.b.isEmpty()&&world.d.isEmpty(),"actual saved stable cell inputs without Entity/Tile injection");
            need(uu.m[69].getClass()==xr.class&&uu.m[Integer.parseInt(profile[3])].getClass()==jq.class,"actual registry Lever/Piston constructors");
            Observation observer=new Observation(world,chunk,uu.m[Integer.parseInt(profile[3])]);world.a(observer);
            List<Object> steps=new ArrayList<>();steps.add(map("stage","initial","operation","observe","returned",true,"activation_result",null,"failure",null,"event_start",0,"event_end",0,"state",observer.snapshot()));
            steps.add(observer.step("activate-on",true));
            if(blocked){for(int i=1;i<=3;i++)steps.add(observer.step("blocked-quiet-"+i,false));}
            else{
                for(int i=1;i<=3;i++)steps.add(observer.step("extend-"+i,false));
                steps.add(observer.step("activate-off",true));
                for(int i=1;i<=3;i++)steps.add(observer.step("retract-"+i,false));
                for(int i=1;i<=2;i++)steps.add(observer.step("quiet-"+i,false));
            }
            world.b(observer);
            System.out.println("PISTON_ACTIVATION_ROW "+ConstructorJson.json(map("index",index,"profile",Arrays.asList(profile),"steps",steps,"events",observer.events)));
            for(Object raw:steps)need(((Map<?,?>)raw).get("returned")==Boolean.TRUE,"all declared original direct entries returned");
        }
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"bounded external effects");
        System.out.println("PISTON_ACTIVATION_COMPLETE "+ConstructorJson.json(map("rows",cases.size(),"network",network,"natives",natives,"execs",execs,"denied_writes",deniedWrites,"write_checks",writes,"raw_allocations",0,"source_field_repairs",0,"full_world_ticks",0)));
    }
}
