import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Bounded owned-world persistence observation. No original field is written. */
public final class WorldStorageObservation {
    static final Map<String,Object> BINDINGS = new TreeMap<>(), FIELDS = new TreeMap<>();
    static final int[][] ORDINARY = {{0,0},{1,0},{-1,-1},{31,31},{32,32},{-32,-33}};
    static final int[][] EXTREME = {{Integer.MIN_VALUE,Integer.MAX_VALUE},{Integer.MAX_VALUE,Integer.MIN_VALUE}};
    static final String CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final String SERVER = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    final ClassLoader loader;
    final boolean client, observed;
    final Path parent, output;
    final URL jar;
    final Map<String,Object> origins = map();
    final List<Object> events = new ArrayList<>();
    final IdentityHashMap<Object,Integer> ids = new IdentityHashMap<>();
    final Class<?> worldType, providerType, chunkType, infoType, storageType, compoundType;
    final Object format, storage;

    static void check(boolean value, String message) { if (!value) throw new IllegalStateException(message); }
    static Map<String,Object> map(Object... values) {
        Map<String,Object> result = new LinkedHashMap<>();
        for (int i=0;i<values.length;i+=2) result.put((String)values[i],values[i+1]);
        return result;
    }
    static Object invoke(Method method, Object receiver, Object... args) throws Exception {
        try { return method.invoke(receiver,args); }
        catch (InvocationTargetException failure) {
            Throwable cause=failure.getCause();
            if(cause instanceof Exception e) throw e;
            if(cause instanceof Error e) throw e;
            throw failure;
        }
    }
    static Field field(Class<?> type,String name) throws Exception {
        Field result=type.getDeclaredField(name);result.setAccessible(true);
        FIELDS.put(type.getName()+"."+name,map("class",type.getName(),"name",name,"descriptor",descriptor(result.getType()),"modifiers",result.getModifiers()));return result;
    }
    static Object get(Object receiver,String name) throws Exception { return field(receiver.getClass(),name).get(receiver); }
    static Object inherited(Object receiver,Class<?> owner,String name) throws Exception { return field(owner,name).get(receiver); }
    static Method method(Class<?> owner,String name,Class<?> result,Class<?>... args) throws Exception {
        Method value=owner.getDeclaredMethod(name,args);
        check(value.getReturnType()==result && Modifier.isPublic(value.getModifiers()),"exact public method "+owner.getName()+"."+name);
        StringBuilder signature=new StringBuilder("(");for(Class<?> arg:args)signature.append(descriptor(arg));signature.append(')').append(descriptor(result));
        BINDINGS.put(owner.getName()+"."+name+signature,map("class",owner.getName(),"name",name,"descriptor",signature.toString(),"modifiers",value.getModifiers()));
        return value;
    }
    static String descriptor(Class<?> type) {
        if(type.isArray())return type.getName().replace('.','/');
        if(!type.isPrimitive())return "L"+type.getName().replace('.','/')+";";
        if(type==void.class)return "V";if(type==boolean.class)return "Z";if(type==byte.class)return "B";if(type==short.class)return "S";if(type==char.class)return "C";if(type==int.class)return "I";if(type==long.class)return "J";if(type==float.class)return "F";if(type==double.class)return "D";throw new IllegalStateException("primitive");
    }
    static Set<String> originalClassNames(Path archive) throws Exception {
        Set<String> names=new HashSet<>();try(java.util.zip.ZipFile zip=new java.util.zip.ZipFile(archive.toFile())){var entries=zip.entries();while(entries.hasMoreElements()){String name=entries.nextElement().getName();if(name.endsWith(".class"))names.add(name.substring(0,name.length()-6).replace('/','.'));}}check(names.size()<=4096,"bounded original class inventory");return names;
    }
    static List<Object> threadInventory(Set<String> originals) {
        List<Object> result=new ArrayList<>();var threads=Thread.getAllStackTraces();check(threads.size()<=64,"bounded threads");
        for(var entry:threads.entrySet()){Thread thread=entry.getKey();List<String> frames=new ArrayList<>();boolean original=false;check(entry.getValue().length<=128,"bounded thread frames");for(var frame:entry.getValue()){original|=originals.contains(frame.getClassName());frames.add(frame.toString());}check(!original&&(thread==Thread.currentThread()||thread.isDaemon()),"no concurrent original frames");result.add(map("id",thread.getId(),"name",thread.getName(),"daemon",thread.isDaemon(),"state",thread.getState().name(),"original_frame",original,"frames",frames));}return result;
    }
    Class<?> type(String name) throws Exception {
        Class<?> value=Class.forName(name,true,loader);
        check(value.getClassLoader()==loader && value.getProtectionDomain().getCodeSource().getLocation().equals(jar),"supplied class origin "+name);
        origins.put(name,value.getProtectionDomain().getCodeSource().getLocation().toExternalForm());return value;
    }
    Object construct(Class<?> type,Class<?>[] args,Object... values) throws Exception {
        Constructor<?> constructor=type.getDeclaredConstructor(args);
        check(constructor.getModifiers()==Modifier.PUBLIC,"actual public constructor "+type.getName());
        StringBuilder signature=new StringBuilder("(");for(Class<?> arg:args)signature.append(descriptor(arg));signature.append(")V");
        BINDINGS.put(type.getName()+".<init>"+signature,map("class",type.getName(),"name","<init>","descriptor",signature.toString(),"modifiers",constructor.getModifiers()));
        try { return constructor.newInstance(values); }
        catch(InvocationTargetException failure) { Throwable cause=failure.getCause();if(cause instanceof Exception e)throw e;if(cause instanceof Error e)throw e;throw failure; }
    }
    int id(Object value) { if(!ids.containsKey(value))ids.put(value,ids.size());return ids.get(value); }
    static String sha(byte[] raw) throws Exception { return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(raw)); }
    static Map<String,Object> blob(byte[] raw) throws Exception { return map("bytes",raw.length,"sha256",sha(raw),"hex",HexFormat.of().formatHex(raw)); }
    static String hex(long value) { return String.format(Locale.ROOT,"%016x",value); }
    static void json(Object value,StringBuilder out) {
        if(value==null){out.append("null");return;}
        if(value instanceof String text){out.append('"');for(char c:text.toCharArray()){if(c=='"'||c=='\\'){out.append('\\').append(c);}else if(c<32||c>=127){out.append(String.format(Locale.ROOT,"\\u%04x",(int)c));}else out.append(c);}out.append('"');return;}
        if(value instanceof Number||value instanceof Boolean){out.append(value);return;}
        if(value instanceof Map<?,?> object){out.append('{');boolean first=true;for(var e:object.entrySet()){if(!first)out.append(',');first=false;json(e.getKey().toString(),out);out.append(':');json(e.getValue(),out);}out.append('}');return;}
        if(value instanceof Iterable<?> list){out.append('[');boolean first=true;for(Object e:list){if(!first)out.append(',');first=false;json(e,out);}out.append(']');return;}
        throw new IllegalStateException("JSON type "+value.getClass());
    }
    static Map<String,Object> metadata(Object info) throws Exception {
        Map<String,Object> result=map();String[] names={"seed","spawn_x","spawn_y","spawn_z","time","last_played","size_on_disk","player","player_dimension","level_name","version","raining","rain_time","thundering","thunder_time"};
        for(int i=0;i<15;i++){Object value=get(info,String.valueOf((char)('a'+i)));if(value instanceof Long l)value=hex(l);if(i==7){check(value==null,"empty-player starter scope");value=false;}result.put(names[i],value);}return result;
    }
    Map<String,Object> roots() throws Exception {
        Map<String,Object> result=map();
        for(String name:new String[]{"level.dat","level.dat_old","level.dat_new","session.lock"}) {
            Path path=parent.resolve("world").resolve(name);result.put(name,Files.exists(path)?blob(Files.readAllBytes(path)):null);
        }
        return result;
    }
    Map<String,Object> files() throws Exception {
        Map<String,Object> result=map();long total=0;
        try(var walk=Files.walk(parent.resolve("world"))){
            List<Path> paths=walk.sorted().toList();check(paths.size()<=1024,"bounded owned world paths");
            for(Path path:paths){check(!Files.isSymbolicLink(path),"ordinary owned files");if(!Files.isRegularFile(path))continue;long size=Files.size(path);total+=size;check(size<=8*1024*1024 && total<=64*1024*1024,"bounded owned world bytes");byte[] raw=Files.readAllBytes(path);result.put(parent.resolve("world").relativize(path).toString().replace('\\','/'),map("bytes",raw.length,"sha256",sha(raw)));}
        }return result;
    }
    WorldStorageObservation(Path archive,boolean client,boolean observed,Path parent,Path output) throws Exception {
        this.client=client;this.observed=observed;this.parent=parent;this.output=output;this.jar=archive.toUri().toURL();
        check(sha(Files.readAllBytes(archive)).equals(client?CLIENT:SERVER),"pinned original");
        loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader());
        worldType=type(client?"fd":"dj");providerType=type(client?"xa":"os");chunkType=type(client?"lm":"hi");infoType=type(client?"ei":"ct");storageType=type(client?"wt":"om");compoundType=type(client?"nu":"iq");
        for(String name:client?new String[]{"rh","wd","mx","fm","ld","bf","ok","yf","qn","to","as","rj","qj","wi","vi","uu"}:new String[]{"kp","nz","ie","dq","hb","an","jb","pk","ka","mg","ag","kq","jx","ob","nn","na"})type(name);
        long start=System.currentTimeMillis();
        if(client){format=construct(type("hi"),new Class<?>[]{File.class},parent.toFile());storage=invoke(method(format.getClass(),"a",storageType,String.class,boolean.class),format,"world",false);check(storage.getClass()==type("mx"),"actual mx handler");}
        else{format=null;storage=construct(type("ie"),new Class<?>[]{File.class,String.class,boolean.class},parent.toFile(),"world",false);}
        long end=System.currentTimeMillis();id(storage);
        events.add(map("kind","handler","interval_ms",List.of(start,end),"session_token",hex((long)inherited(storage,type(client?"fm":"dq"),"e")),"roots",roots()));
    }
    final class Context {
        final int dimension;
        final Object world,provider,cache,chunkLoader,info,generator;
        final Map<String,Object> direct=new LinkedHashMap<>(),cached=new LinkedHashMap<>();
        Context(int dimension,boolean loaded) throws Exception {
            this.dimension=dimension;
            Object preview=invoke(method(type(client?"fm":"dq"),"c",infoType),storage);
            check(!loaded||preview!=null,"expected loadable existing metadata");
            String title=preview==null?"world-storage-reference":(String)get(preview,"j");
            Object selected=construct(type(dimension==0?(client?"rh":"kp"):(client?"wd":"nz")),new Class<?>[]{});
            long start=System.currentTimeMillis();
            world=construct(worldType,new Class<?>[]{storageType,String.class,long.class,providerType},storage,title,23L,selected);
            long end=System.currentTimeMillis();
            provider=field(worldType,"t").get(world);cache=field(worldType,"v").get(world);info=field(worldType,"x").get(world);chunkLoader=get(cache,"d");generator=get(cache,"c");
            id(world);id(info);id(provider);id(cache);id(chunkLoader);id(generator);
            check(provider==selected && field(worldType,"w").get(world)==storage && get(cache,"g")==world,"real world ownership");
            check(chunkLoader.getClass()==type(client?"ld":"hb") && (int)field(providerType,"g").get(provider)==dimension,"actual dimension loader");
            Path expected=parent.resolve("world");if(dimension==-1)expected=expected.resolve("DIM-1");
            check(((File)get(chunkLoader,"a")).toPath().toRealPath().equals(expected.toRealPath()),"actual base/DIM-1 loader root");
            if(loaded)check(!(boolean)field(worldType,"s").get(world) && ((Map<?,?>)get(cache,"e")).isEmpty(),"genuine load-aware empty-cache constructor");
            events.add(map("kind","world","dimension",dimension,"interval_ms",List.of(start,end),"explicit_provider",selected.getClass().getName(),"snapshot",state()));
        }
        Object load(int x,int z) throws Exception {
            Object value=invoke(method(chunkLoader.getClass(),"a",chunkType,worldType,int.class,int.class),chunkLoader,world,x,z);
            check(value!=null && value.getClass()==chunkType && (int)get(value,"j")==x && (int)get(value,"k")==z,"existing requested slot loads directly");id(value);return value;
        }
        Map<String,Object> chunk(Object value) throws Exception {
            Map<String,Object> data=map();String[] roles={"blocks","metadata","skylight","block_light","heightmap"};
            Object[] arrays={get(value,"b"),get(get(value,"e"),"a"),get(get(value,"f"),"a"),get(get(value,"g"),"a"),get(value,"h")};
            int[] lengths={32768,16384,16384,16384,256};for(int i=0;i<5;i++){check(((byte[])arrays[i]).length==lengths[i],"complete loaded arrays");data.put(roles[i],blob((byte[])arrays[i]));}
            Map<String,Object> flags=map();for(String name:new String[]{"c","n","o","p","q"})flags.put(name,get(value,name));
            check(((Map<?,?>)get(value,"l")).isEmpty(),"empty tile starter");Object[] slices=(Object[])get(value,"m");check(slices.length==8,"8 entity slices");for(Object slice:slices)check(((List<?>)slice).isEmpty(),"empty entity starter");
            return map("id",id(value),"dimension",dimension,"coordinates",List.of(get(value,"j"),get(value,"k")),"world_matches",get(value,"d")==world,"data",data,"minimum",get(value,"i"),"flags",flags,"last_save",hex((long)get(value,"r")),"tiles",0,"entities",List.of(0,0,0,0,0,0,0,0));
        }
        Map<String,Object> state() throws Exception {
            Map<?,?> actual=(Map<?,?>)get(cache,"e");List<?> list=(List<?>)get(cache,"f");List<Object> entries=new ArrayList<>(),insertions=new ArrayList<>(),headers=new ArrayList<>();
            check(actual.size()<=64 && list.size()<=64 && ((List<?>)field(worldType,"C").get(world)).size()<=131072,"bounded cache/light setup");
            for(var entry:actual.entrySet())entries.add(List.of(entry.getKey(),id(entry.getValue())));for(Object value:list){insertions.add(id(value));Map<String,Object> flags=map();for(String name:new String[]{"c","n","o","p","q"})flags.put(name,get(value,name));headers.add(map("id",id(value),"coordinates",List.of(get(value,"j"),get(value,"k")),"flags",flags,"minimum",get(value,"i"),"last_save",hex((long)get(value,"r"))));}
            return map("dimension",dimension,"world_id",id(world),"info_id",id(info),"storage_id",id(storage),"metadata",metadata(info),"map",entries,"insertions",insertions,"cached_headers",headers,"pending",new ArrayList<>((Set<?>)get(cache,"a")),"queue_count",((List<?>)field(worldType,"C").get(world)).size(),"loader_root",((File)get(chunkLoader,"a")).toPath().toString(),"no_sky",field(providerType,"e").get(provider));
        }
        Map<String,Object> cacheDataFingerprints() throws Exception {
            Map<String,Object> result=map();for(Object value:(List<?>)get(cache,"f")){Map<String,Object> fingerprint=map();Map<?,?> data=(Map<?,?>)chunk(value).get("data");for(var entry:data.entrySet()){Map<?,?> bytes=(Map<?,?>)entry.getValue();fingerprint.put((String)entry.getKey(),map("bytes",bytes.get("bytes"),"sha256",bytes.get("sha256")));}result.put(Integer.toString(id(value)),fingerprint);}return result;
        }
        void time(long time) throws Exception {invoke(method(worldType,"a",void.class,long.class),world,time);check((long)get(info,"e")==time,"real world-time API");events.add(map("kind","time","dimension",dimension,"value",hex(time),"descriptor","(J)V"));}
        void fullSave(String label) throws Exception {
            Object before=state(),beforeRoot=observed?roots():null;Map<String,Object> arrays=cacheDataFingerprints();long start=System.currentTimeMillis();
            invoke(method(worldType,"a",void.class,boolean.class,type(client?"yb":"pj")),world,true,null);
            long end=System.currentTimeMillis();check(arrays.equals(cacheDataFingerprints()),"full save retains every cached array byte");
            events.add(map("kind","full_save","label",label,"dimension",dimension,"interval_ms",List.of(start,end),"before",before,"after",state(),"cached_arrays_retained",arrays,"before_roots",beforeRoot,"after_roots",observed?roots():null));
        }
        void write(Object value,String label) throws Exception {
            int x=(int)get(value,"j"),z=(int)get(value,"k");Path path=((File)get(chunkLoader,"a")).toPath().resolve("region/r."+(x>>5)+"."+(z>>5)+".mcr");
            byte[] beforeImage=Files.exists(path)?Files.readAllBytes(path):new byte[0];
            Object before=chunk(value);long start=System.currentTimeMillis();
            invoke(method(chunkLoader.getClass(),"a",void.class,worldType,chunkType),chunkLoader,world,value);
            long end=System.currentTimeMillis();Object after=chunk(value);check(before.equals(after),"direct writer retains chunk arrays/flags/transients");
            byte[] image=Files.readAllBytes(path);check(beforeImage.length<=8*1024*1024&&image.length<=8*1024*1024,"bounded actual region images");
            int offset=4096+4*((x&31)+32*(z&31));long timestamp=Integer.toUnsignedLong(java.nio.ByteBuffer.wrap(image,offset,4).getInt());
            check(timestamp>=(start/1000 & 0xffffffffL) && timestamp<=(end/1000 & 0xffffffffL),"actual raw region clock interval");
            events.add(map("kind","direct_write","dimension",dimension,"label",label,"coordinates",List.of(x,z),"interval_ms",List.of(start,end),"timestamp",timestamp,"chunk",observed?after:null,"metadata_after",metadata(info),"region_before_bytes",beforeImage.length,"region_after_bytes",image.length,"region_before",observed?blob(beforeImage):null,"region_after",observed?blob(image):null));
        }
        Map<String,Object> monitor() throws Exception {
            Map<String,Object> result=map();for(Field value:generator.getClass().getDeclaredFields()){value.setAccessible(true);if(value.getType()==Random.class){ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(ObjectOutputStream stream=new ObjectOutputStream(bytes)){stream.writeObject(value.get(generator));}result.put(value.getName(),blob(bytes.toByteArray()));}else if(value.getType()==double[].class){Object array=value.get(generator);result.put(value.getName(),array==null?null:Array.getLength(array));}}return result;
        }
        void inspect(boolean rewrite,String label) throws Exception {
            check(((Map<?,?>)get(cache,"e")).isEmpty(),"load phase begins with empty cache");
            for(int[] xy:ORDINARY){Object value=load(xy[0],xy[1]);direct.put(xy[0]+","+xy[1],value);Object snap=chunk(value);check(!(boolean)get(value,"c") && (long)get(value,"r")==0 && (int)get(value,"i")==0,"direct loader transient defaults");events.add(map("kind","direct_load","label",label,"dimension",dimension,"coordinates",List.of(xy[0],xy[1]),"chunk",observed?snap:null));}
            for(int[] xy:EXTREME){Object value=load(xy[0],xy[1]);direct.put(xy[0]+","+xy[1],value);events.add(map("kind","direct_load","label",label,"dimension",dimension,"coordinates",List.of(xy[0],xy[1]),"chunk",observed?chunk(value):null));}
            Map<String,Object> before=monitor();time(dimension==0?1234L:5678L);
            for(int[] xy:ORDINARY){Object value=invoke(method(cache.getClass(),"c",chunkType,int.class,int.class),cache,xy[0],xy[1]);id(value);check(value!=get(cache,"b") && (boolean)get(value,"c") && (long)get(value,"r")==((long)get(info,"e")),"outer loaded stamp/activation");check(!((boolean)get(value,"n")),"finite isolated slots cause no population");Object expected=chunk(direct.get(xy[0]+","+xy[1])).get("data");check(expected.equals(chunk(value).get("data")),"acquired payload equals known direct disk load");cached.put(xy[0]+","+xy[1],value);events.add(map("kind","acquire","label",label,"dimension",dimension,"coordinates",List.of(xy[0],xy[1]),"chunk",observed?chunk(value):null));}
            check(before.equals(monitor()) && ((List<?>)field(worldType,"C").get(world)).isEmpty(),"provider primary Random/scratch unchanged and loaded queue empty");
            events.add(map("kind","load_controls","label",label,"dimension",dimension,"primary_provider_state_unchanged",true,"provider_state",before,"snapshot",state()));
            if(rewrite){for(var entry:direct.entrySet())write(cached.getOrDefault(entry.getKey(),entry.getValue()),label);fullSave(label);}
        }
    }
    void closeRegions() throws Exception {
        if(client)invoke(method(format.getClass(),"c",void.class),format);else invoke(method(storage.getClass(),"e",void.class),storage);
        Class<?> cache=type(client?"rj":"kq");check(((Map<?,?>)field(cache,"a").get(null)).isEmpty(),"real region close empties global cache");events.add(map("kind","close_regions","global_map_count",0));
    }
    Object run(String phase) throws Exception {
        if(phase.equals("prepare")){
            for(int dim:new int[]{0,-1}){Context context=new Context(dim,dim==-1);context.time(dim==0?1234:5678);Object template=null;for(int[] xy:ORDINARY){Object value=invoke(method(context.cache.getClass(),"c",chunkType,int.class,int.class),context.cache,xy[0],xy[1]);id(value);check(!(boolean)get(value,"n"),"isolated genuine generated preparation");if(template==null)template=value;events.add(map("kind","prepare_acquire","dimension",dim,"coordinates",List.of(xy[0],xy[1]),"chunk",observed?context.chunk(value):null));if(dim==-1)context.write(value,"explicit-unconditional-Nether-storage-preparation");}
                Class<?> schema=type(client?"to":"mg");for(int[] xy:EXTREME){Object level=construct(compoundType,new Class<?>[]{});invoke(method(schema,"a",void.class,chunkType,worldType,compoundType),null,template,context.world,level);invoke(method(compoundType,"a",void.class,String.class,int.class),level,"xPos",xy[0]);invoke(method(compoundType,"a",void.class,String.class,int.class),level,"zPos",xy[1]);Object clone=invoke(method(schema,"a",chunkType,worldType,compoundType),null,context.world,level);id(clone);context.write(clone,"constructed-extreme-storage-clone");}
                context.fullSave("prepare-first-metadata-before-cache-save");context.fullSave("prepare-second-explicit-save");}
            closeRegions();return map("final_files",files(),"final_roots",roots());
        }
        Context base=new Context(0,true),nether=new Context(-1,true);check(base.info!=nether.info,"distinct loaded metadata owners");
        events.add(map("kind","info_ownership","shared",false,"base",id(base.info),"nether",id(nether.info)));
        base.inspect(true,"initial");nether.inspect(true,"initial");closeRegions();
        Object saved=map("base",base.state(),"nether",nether.state(),"roots",roots(),"files",files());
        // A fresh real handler overwrites the session token and loads fresh Info owners.
        WorldStorageObservation reload=new WorldStorageObservation(Path.of(jar.toURI()),client,observed,parent,output);
        Context loadedBase=reload.new Context(0,true),loadedNether=reload.new Context(-1,true);loadedBase.inspect(false,"reload");loadedNether.inspect(false,"reload");reload.closeRegions();
        List<Object> chunks=new ArrayList<>();for(Context c:new Context[]{loadedBase,loadedNether})for(Object value:c.direct.values())chunks.add(c.chunk(value));
        events.add(map("kind","genuine_reload","events",reload.events,"base",loadedBase.state(),"nether",loadedNether.state(),"direct_chunks",chunks));
        return map("saved",saved,"final_files",files(),"final_roots",roots());
    }
    public static void main(String[] args) throws Exception {
        System.setOut(System.err);check(args.length==6,"jar side prepare|roundtrip observed|control owned_parent output_json");
        Path archive=Path.of(args[0]).toAbsolutePath().normalize(),parent=Path.of(args[4]).toAbsolutePath().normalize(),output=Path.of(args[5]).toAbsolutePath().normalize();
        check(args[1].equals("client")||args[1].equals("server"),"side");check(args[2].equals("prepare")||args[2].equals("roundtrip"),"phase");check(args[3].equals("observed")||args[3].equals("control"),"mode");
        check(!Files.isSymbolicLink(parent) && Files.isDirectory(parent) && !Files.exists(output),"fresh owned result context");
        check(args[2].equals("prepare")?!Files.exists(parent.resolve("world")):Files.isRegularFile(parent.resolve("world/level.dat")),"new preparation or existing copied/C-created world");
        Set<String> originalNames=originalClassNames(archive);List<Object> threadsBefore=threadInventory(originalNames);
        WorldStorageObservation probe=new WorldStorageObservation(archive,args[1].equals("client"),args[3].equals("observed"),parent,output);
        Object result=probe.run(args[2]);Map<String,Object> record=map("schema",1,"phase",args[2],"side",args[1],"mode",args[3],"jar_sha256",sha(Files.readAllBytes(archive)),"class_origins",probe.origins,"block_map",blob((byte[])field(probe.type(probe.client?"vi":"nn"),"a").get(null)),"events",probe.events,"result",result,"bindings",BINDINGS,"fields",FIELDS,"threads_before",threadsBefore,"threads_after",threadInventory(originalNames),"scope","Explicit-provider base/Nether storage; no gameplay dimension transition, player/entity/tile/scheduling or C parity claim.","control_scope","Same original calls; intermediate payload/root observations omitted only in control.");
        StringBuilder text=new StringBuilder();json(record,text);text.append('\n');check(text.length()<=32*1024*1024,"bounded complete JSON");Files.writeString(output,text,StandardOpenOption.CREATE_NEW);
    }
}
