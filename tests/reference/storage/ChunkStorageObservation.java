import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.jar.*;

/** Paired chunk schemas and direct region observations; no game-field assignments. */
public final class ChunkStorageObservation {
    static final int MAX_BYTES = 32 * 1024 * 1024;
    static final String CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final String SERVER = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Map<String,Object> object(Object... pairs) {
        Map<String,Object> out = new LinkedHashMap<>();
        for (int i=0;i<pairs.length;i+=2) out.put((String)pairs[i],pairs[i+1]);
        return out;
    }
    static String hash(byte[] value)throws Exception {return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));}
    static String hex(long value) {return String.format(Locale.ROOT,"%016x",value);}
    static Object blob(byte[] value) {return value==null?null:object("length",value.length,"values",HexFormat.of().formatHex(value));}
    static String descriptor(Class<?> value) {
        if(value.isArray())return value.getName().replace('.','/');
        if(!value.isPrimitive())return "L"+value.getName().replace('.','/')+";";
        return value==void.class?"V":value==int.class?"I":value==long.class?"J":value==boolean.class?"Z":value==byte.class?"B":value==short.class?"S":value==double.class?"D":value==float.class?"F":"C";
    }
    static final class BoundedOutput extends ByteArrayOutputStream {
        @Override public synchronized void write(int value) {
            require(count < 1024*1024,"bounded raw NBT stream"); super.write(value);
        }
        @Override public synchronized void write(byte[] value,int offset,int length) {
            require(length>=0 && length<=1024*1024-count,"bounded raw NBT stream"); super.write(value,offset,length);
        }
    }
    static final class OriginalFailure extends Exception {
        private static final long serialVersionUID = 1L;
        OriginalFailure(Throwable cause) {super(cause);}
    }
    static Object invoke(Method method,Object receiver,Object... args)throws Exception {
        try{return method.invoke(receiver,args);}catch(InvocationTargetException failure){
            if(failure.getCause() instanceof Error)throw (Error)failure.getCause();
            if(!(method.getDeclaringClass().getClassLoader() instanceof URLClassLoader)){
                if(failure.getCause() instanceof Exception)throw (Exception)failure.getCause();
                throw failure;
            }
            throw new OriginalFailure(failure.getCause());
        }
    }
    static Object construct(Constructor<?> method,Object... args)throws Exception {
        try{return method.newInstance(args);}catch(InvocationTargetException failure){
            if(failure.getCause() instanceof Error)throw (Error)failure.getCause();
            throw new OriginalFailure(failure.getCause());
        }
    }
    static Map<String,Object> failure(OriginalFailure failure) {
        Throwable cause=failure.getCause();
        cause.printStackTrace(System.err);
        return object("status","exception","class",cause.getClass().getName(),"message",cause.getMessage());
    }
    static final class Observer {
        final boolean client;
        final URL jar;
        final ClassLoader loader;
        final Path root,artifacts;
        final Map<String,Class<?>> classes=new LinkedHashMap<>();
        final Map<String,Object> origins=new LinkedHashMap<>(),bindings=new LinkedHashMap<>(),fields=new LinkedHashMap<>();
        final Object model,world,provider,storage,cache,saveFormat,regionLoader;
        final Class<?> modelType,worldType,chunkType,compound,tag,listType;
        final Method modelGet,modelRef,worldSnapshot,json,read,write,decode,encode,load,save,regionRead,regionWrite,close,time,setTime,setTag,getCompound;
        int artifactCount;
        long artifactBytes;
        Object savedBaseline;
        Class<?> type(String name)throws Exception {
            if(classes.containsKey(name))return classes.get(name);
            Class<?> value=Class.forName(name,false,loader);
            require(value.getClassLoader()==loader&&value.getProtectionDomain().getCodeSource()!=null&&jar.equals(value.getProtectionDomain().getCodeSource().getLocation()),"pinned original class "+name);
            byte[] bytes;
            try(InputStream stream=value.getResourceAsStream("/"+name.replace('.','/')+".class")){require(stream!=null,"class resource");bytes=stream.readAllBytes();}
            origins.put(name,object("origin",jar.toString(),"loader","isolated","bytes",bytes.length,"sha256",hash(bytes)));
            classes.put(name,value);return value;
        }
        Method bind(String role,Class<?> owner,String name,Class<?> result,int modifiers,Class<?>... args)throws Exception {
            Method method=owner.getDeclaredMethod(name,args);
            require(method.getReturnType()==result&&method.getModifiers()==modifiers,"exact method "+role);
            method.setAccessible(true);
            StringBuilder sig=new StringBuilder("(");for(Class<?> a:args)sig.append(descriptor(a));sig.append(')').append(descriptor(result));
            bindings.put(role,object("class",owner.getName(),"method",name,"signature",sig.toString(),"modifiers",modifiers));return method;
        }
        Constructor<?> ctor(Class<?> owner,Class<?>... args)throws Exception {
            Constructor<?> value=owner.getDeclaredConstructor(args);require(value.getModifiers()==1,"public original constructor");
            StringBuilder sig=new StringBuilder("(");for(Class<?> a:args)sig.append(descriptor(a));sig.append(")V");
            bindings.put(owner.getName()+sig,object("class",owner.getName(),"method","<init>","signature",sig.toString(),"modifiers",1));return value;
        }
        Object field(Class<?> owner,String name,Object receiver)throws Exception {
            Field value=owner.getDeclaredField(name);value.setAccessible(true);
            fields.put(owner.getName()+"."+name,object("class",owner.getName(),"name",name,"signature",descriptor(value.getType()),"modifiers",value.getModifiers()));
            return value.get(receiver);
        }
        Object get(String name,Object receiver)throws Exception{return invoke(modelGet,model,name,receiver);}
        Object ref(Object value)throws Exception{return invoke(modelRef,model,value);}
        void observerReference(String name,Object value)throws Exception {
            require(Arrays.asList("world","provider","storage","cache").contains(name)&&modelType.getClassLoader()!=loader,"project observer references only");
            Field f=modelType.getDeclaredField(name);require(f.getType()==Object.class&&f.getModifiers()==0,"project observer field");f.setAccessible(true);f.set(model,value);
        }
        Observer(URL jar,ClassLoader loader,Path root,Path artifacts,boolean client)throws Exception {
            this.jar=jar;this.loader=loader;this.root=root;this.artifacts=artifacts;this.client=client;
            modelType=Class.forName("ChunkObservation$Model");
            Constructor<?> own=modelType.getDeclaredConstructor(ClassLoader.class,boolean.class);own.setAccessible(true);model=own.newInstance(loader,client);
            modelGet=modelType.getDeclaredMethod("get",String.class,Object.class);modelGet.setAccessible(true);
            modelRef=modelType.getDeclaredMethod("reference",Object.class);modelRef.setAccessible(true);
            worldSnapshot=modelType.getDeclaredMethod("worldSnapshot",int.class,int.class,int.class);worldSnapshot.setAccessible(true);
            json=ChunkObservation.class.getDeclaredMethod("json",Object.class);json.setAccessible(true);
            worldType=type(client?"fd":"dj");chunkType=type(client?"lm":"hi");compound=type(client?"nu":"iq");tag=type(client?"ij":"fo");listType=type(client?"sp":"lr");
            Class<?> storeType=type(client?"wt":"om"),providerType=type(client?"xa":"os"),io=type(client?"as":"ag"),schema=type(client?"to":"mg"),regions=type(client?"rj":"kq");
            read=bind("nbt_read",io,"a",compound,9,DataInput.class);
            write=bind("nbt_write",io,"a",void.class,9,compound,DataOutput.class);
            decode=bind("schema_decode",schema,"a",chunkType,9,worldType,compound);
            encode=bind("schema_encode",schema,"a",void.class,9,chunkType,worldType,compound);
            bind("entity_factory",type(client?"jc":"ga"),"a",type(client?"sn":"lq"),9,compound,worldType);
            bind("tile_factory",type(client?"ow":"jh"),"c",type(client?"ow":"jh"),9,compound);
            bind("post_load",chunkType,client?"i":"h",void.class,1);
            bind("normalize_blocks",type(client?"vi":"nn"),"a",void.class,9,byte[].class);
            setTag=bind("nbt_set_tag",compound,"a",void.class,1,String.class,tag);
            getCompound=bind("nbt_get_compound",compound,"k",compound,1,String.class);
            regionRead=bind("region_read",regions,"c",DataInputStream.class,9,File.class,int.class,int.class);
            regionWrite=bind("region_write",regions,"d",DataOutputStream.class,9,File.class,int.class,int.class);
            time=bind("world_time",worldType,client?"t":"m",long.class,1);
            setTime=bind("world_set_time",worldType,"a",void.class,1,long.class);
            require(Files.isDirectory(root,LinkOption.NOFOLLOW_LINKS)&&Files.isRegularFile(root.resolve("world/level.dat"),LinkOption.NOFOLLOW_LINKS),"copied complete saved world");
            if(client){
                Class<?> format=type("hi");saveFormat=construct(ctor(format,File.class),root.toFile());
                storage=invoke(bind("save_handler",format,"a",storeType,1,String.class,boolean.class),saveFormat,"world",false);
                require(storage.getClass()==type("mx"),"actual client region handler");
                world=construct(ctor(worldType,storeType,String.class,long.class),storage,"cache-callback-reference",23L);
                close=bind("close_regions",format,"c",void.class,1);
            }else{
                saveFormat=null;storage=construct(ctor(type("ie"),File.class,String.class,boolean.class),root.toFile(),"world",false);
                Object normal=construct(ctor(type("kp")));
                world=construct(ctor(worldType,storeType,String.class,long.class,providerType),storage,"cache-callback-reference",23L,normal);
                close=bind("close_regions",type("ie"),"e",void.class,1);
            }
            provider=get("world.t",world);cache=get("world.v",world);regionLoader=get("cache.d",cache);
            type(provider.getClass().getName());type(storage.getClass().getName());
            require(regionLoader.getClass()==type(client?"ld":"hb")&&cache.getClass()==type(client?"ok":"jb")&&get("world.w",world)==storage&&get("cache.g",cache)==world&&get("provider.a",provider)==world,"genuine base-world retained loader ownership");
            for(var pair:object("world",world,"provider",provider,"storage",storage,"cache",cache).entrySet())observerReference(pair.getKey(),pair.getValue());
            require(((Map<?,?>)get("cache.e",cache)).isEmpty()&&((List<?>)get("cache.f",cache)).isEmpty(),"load-aware constructor empty cache");
            load=bind("actual_loader",regionLoader.getClass(),"a",chunkType,1,worldType,int.class,int.class);
            save=bind("actual_writer",regionLoader.getClass(),"a",void.class,1,worldType,chunkType);
            invoke(setTime,world,1234L);require((long)invoke(time,world)==1234L,"genuine time setter");
        }
        Object fresh()throws Exception{return construct(ctor(compound));}
        byte[] bytes(Object nbt)throws Exception {
            ByteArrayOutputStream out=new BoundedOutput();invoke(write,null,nbt,(DataOutput)new DataOutputStream(out));
            require(out.size()<=1024*1024,"bounded raw NBT");return out.toByteArray();
        }
        Object cloneNBT(Object nbt)throws Exception{return invoke(read,null,(DataInput)new DataInputStream(new ByteArrayInputStream(bytes(nbt))));}
        @SuppressWarnings("unchecked") Map<String,Object> members(Object nbt)throws Exception {
            return (Map<String,Object>)field(compound,"a",nbt);
        }
        Object filtered(Object nbt,String... omitted)throws Exception {
            Set<String> names=new HashSet<>(Arrays.asList(omitted));Object out=fresh();
            for(var pair:members(nbt).entrySet())if(!names.contains(pair.getKey()))invoke(setTag,out,pair.getKey(),pair.getValue());
            return out;
        }
        void put(Object nbt,String key,Class<?> valueType,Object value)throws Exception {
            invoke(bind("nbt_put_"+descriptor(valueType),compound,"a",void.class,1,String.class,valueType),nbt,key,value);
        }
        Object list(Object... values)throws Exception {
            Object out=construct(ctor(listType));Method add=bind("list_append",listType,"a",void.class,1,tag);
            for(Object value:values)invoke(add,out,value);return out;
        }
        Object number(Class<?> valueType,Object value)throws Exception {
            Object temp=fresh();put(temp,"v",valueType,value);return members(temp).get("v");
        }
        Object actor(boolean valid,boolean tile)throws Exception {
            Object out=fresh();put(out,"id",String.class,valid?(tile?"Sign":"Boat"):"UnregisteredChunkResearchRecord");
            if(tile){put(out,"x",int.class,16);put(out,"y",int.class,64);put(out,"z",int.class,0);
                for(int i=1;i<=4;i++)put(out,"Text"+i,String.class,i==1?"private proof":i==2?"NUL\u0000":"");
            }else if(valid){
                invoke(setTag,out,"Pos",list(number(double.class,16.5),number(double.class,64.0),number(double.class,0.5)));
                invoke(setTag,out,"Motion",list(number(double.class,0.0),number(double.class,0.0),number(double.class,0.0)));
                invoke(setTag,out,"Rotation",list(number(float.class,0.0f),number(float.class,0.0f)));
                put(out,"FallDistance",float.class,0.0f);put(out,"Fire",short.class,(short)0);put(out,"Air",short.class,(short)300);put(out,"OnGround",byte.class,(byte)1);
            }return out;
        }
        Object artifact(String label,Object nbt)throws Exception {
            byte[] value=bytes(nbt);artifactBytes+=value.length;require(++artifactCount<=256&&artifactBytes<=MAX_BYTES,"bounded NBT artifacts");
            Path path=artifacts.resolve(label+".nbt");Files.write(path,value,StandardOpenOption.CREATE_NEW);
            return object("artifact",path.getFileName().toString(),"bytes",value.length,"sha256",hash(value));
        }
        Object chunk(Object value)throws Exception {
            if(value==null)return null;require(value.getClass()==chunkType,"ordinary schema result");
            Map<String,Object> data=new LinkedHashMap<>();List<Object> owners=new ArrayList<>();
            byte[] blocks=(byte[])get("chunk.b",value);data.put("blocks",blob(blocks));owners.add(ref(blocks));
            String[] roles={"metadata","skylight","block_light"},names={"e","f","g"};
            for(int i=0;i<3;i++){Object wrapper=get("chunk."+names[i],value);byte[] backing=wrapper==null?null:(byte[])get("nibble.a",wrapper);
                data.put(roles[i],blob(backing));owners.add(ref(wrapper));owners.add(ref(backing));}
            byte[] heights=(byte[])get("chunk.h",value);data.put("heightmap",blob(heights));data.put("minimum_height",get("chunk.i",value));owners.add(ref(heights));
            Map<String,Object> flags=new LinkedHashMap<>();for(String name:new String[]{"c","n","o","p","q"})flags.put(name,get("chunk."+name,value));
            List<Object> slices=new ArrayList<>();Object[] lists=(Object[])get("chunk.m",value);require(lists.length==8,"eight entity slices");
            for(Object row:lists){List<?> entries=(List<?>)row;require(entries.size()<=16,"entity bound");List<Object> entities=new ArrayList<>();for(Object entry:entries){type(entry.getClass().getName());entities.add(ref(entry));}slices.add(object("object",ref(row),"entries",entities));}
            Map<?,?> tiles=(Map<?,?>)get("chunk.l",value);require(tiles.size()<=16,"tile bound");List<Object> tileRows=new ArrayList<>();
            for(var entry:tiles.entrySet()){
                type(entry.getValue().getClass().getName());Class<?> key=type(entry.getKey().getClass().getName());
                Map<String,Object> position=new TreeMap<>();for(Field f:key.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){
                    require(f.getType()==int.class,"tile position integer fields");f.setAccessible(true);position.put(f.getName(),f.getInt(entry.getKey()));}
                tileRows.add(object("key_class",key.getName(),"key_fields",position,"object",ref(entry.getValue())));
            }
            return object("object",ref(value),"world",ref(get("chunk.d",value)),"coordinates",Arrays.asList(get("chunk.j",value),get("chunk.k",value)),"data",data,"flags",flags,
                    "last_save_time",hex((long)get("chunk.r",value)),"array_references",owners,"entity_slices",slices,"tile_map",object("object",ref(tiles),"entries",tileRows));
        }
        Object context()throws Exception {
            Object info=field(worldType,"x",world);
            return object("world",invoke(worldSnapshot,model,1,0,32768),"world_time",hex((long)invoke(time,world)),
                    "size_on_disk",hex((long)field(type(client?"ei":"ct"),"g",info)));
        }
        Object registry()throws Exception {
            Class<?> normalize=type(client?"vi":"nn"),registry=type(client?"uu":"na");
            byte[] table=(byte[])field(normalize,"a",null);Object[] entries=(Object[])field(registry,"m",null);
            require(table.length==256&&entries.length==256,"actual normalization registry sizes");List<Object> rows=new ArrayList<>();
            for(int i=0;i<256;i++)rows.add(entries[i]==null?null:entries[i].getClass().getName());
            return object("map",blob(table),"map_identity",ref(table),"registry_identity",ref(entries),"registered_classes",rows);
        }
        Object regionNBT()throws Exception {
            try(DataInputStream input=(DataInputStream)invoke(regionRead,null,root.resolve("world").toFile(),1,0)){
                require(input!=null,"actual region stream");return invoke(read,null,(DataInput)input);
            }
        }
        Object runCase(String name,Object input,int index)throws Exception {
            String prefix=String.format(Locale.ROOT,"%02d-%s",index,name);Map<String,Object> row=object("name",name,"input",artifact(prefix+"-input",input));
            Map<String,Object> direct=new LinkedHashMap<>();row.put("static",direct);
            direct.put("stage","decode");try{
                Object level=invoke(getCompound,input,"Level");Object value=invoke(decode,null,world,level);
                direct.put("status","ok");direct.put("chunk_before_write",chunk(value));Object output=fresh();
                direct.put("stage","encode");invoke(encode,null,value,world,output);direct.put("output_level",artifact(prefix+"-static-output",output));direct.put("chunk_after_write",chunk(value));direct.put("stage","complete");
            }catch(OriginalFailure failure){direct.putAll(failure(failure));}
            direct.put("input_after",artifact(prefix+"-input-after",input));
            try(DataOutputStream output=(DataOutputStream)invoke(regionWrite,null,root.resolve("world").toFile(),1,0)){invoke(write,null,input,(DataOutput)output);}
            Map<String,Object> region=new LinkedHashMap<>();row.put("region",region);region.put("request",Arrays.asList(1,0));
            region.put("stage","load");try{
                Object value=invoke(load,regionLoader,world,1,0);region.put("status",value==null?"null":"ok");region.put("chunk_before_write",chunk(value));
                if(value!=null){region.put("stage","save");invoke(save,regionLoader,world,value);region.put("chunk_after_write",chunk(value));
                    // The genuine writer addresses the returned chunk coordinates.
                    int x=(int)get("chunk.j",value),z=(int)get("chunk.k",value);
                    region.put("stage","readback");try(DataInputStream stream=(DataInputStream)invoke(regionRead,null,root.resolve("world").toFile(),x,z)){
                        require(stream!=null,"genuine region writer output");region.put("output_root",artifact(prefix+"-region-output",invoke(read,null,(DataInput)stream)));}
                }
                region.put("stage","complete");
            }catch(OriginalFailure failure){region.putAll(failure(failure));}
            row.put("context_after",context());return row;
        }
        Object cases()throws Exception {
            Object baseline=regionNBT();savedBaseline=artifact("saved-baseline",baseline);List<Object> result=new ArrayList<>();
            String[] arrayNames={"Blocks","Data","SkyLight","BlockLight","HeightMap"};List<String> cases=new ArrayList<>(Arrays.asList("valid_saved","normalization_all_ids","missing_scalars","wrong_x","wrong_z","wrong_populated","ignored_wrong_last_update"));
            for(String name:arrayNames)cases.add("wrong_"+name);
            cases.addAll(Arrays.asList("wrong_Entities","wrong_TileEntities"));for(String name:arrayNames)cases.add("missing_"+name);
            cases.addAll(Arrays.asList("missing_all_arrays","short_all_arrays","missing_level","wrong_level","wrong_location","nonzero_populated","unknown_entity","unknown_tile","valid_boat_and_sign"));
            int index=0;
            for(String name:cases){Object input=cloneNBT(baseline);Object level=invoke(getCompound,input,"Level");
                put(input,"OpaqueRoot",String.class,"retained input only");put(level,"OpaqueLevel",String.class,"retained input only");
                if(name.equals("normalization_all_ids")){byte[] b=new byte[32768];for(int i=0;i<b.length;i++)b[i]=(byte)i;put(level,"Blocks",byte[].class,b);}
                else if(name.equals("missing_scalars"))level=filtered(level,"xPos","zPos","TerrainPopulated","LastUpdate","Entities","TileEntities");
                else if(name.equals("wrong_x"))put(level,"xPos",long.class,1L);
                else if(name.equals("wrong_z"))put(level,"zPos",String.class,"wrong");
                else if(name.equals("wrong_populated"))put(level,"TerrainPopulated",int.class,1);
                else if(name.equals("ignored_wrong_last_update"))put(level,"LastUpdate",String.class,"ignored");
                else if(name.startsWith("wrong_")&&!name.equals("wrong_level")&&!name.equals("wrong_location"))put(level,name.substring(6),String.class,"wrong");
                else if(name.equals("missing_all_arrays"))level=filtered(level,arrayNames);
                else if(name.startsWith("missing_")&&!name.equals("missing_level"))level=filtered(level,name.substring(8));
                else if(name.equals("short_all_arrays")){for(int i=0;i<arrayNames.length;i++)put(level,arrayNames[i],byte[].class,i==0?new byte[]{0,(byte)255,1}:new byte[i]);}
                else if(name.equals("wrong_location")){put(level,"xPos",int.class,-7);put(level,"zPos",int.class,19);}
                else if(name.equals("nonzero_populated"))put(level,"TerrainPopulated",byte.class,(byte)-128);
                else if(name.equals("unknown_entity"))invoke(setTag,level,"Entities",list(actor(false,false)));
                else if(name.equals("unknown_tile"))invoke(setTag,level,"TileEntities",list(actor(false,true)));
                else if(name.equals("valid_boat_and_sign")){
                    byte[] blocks=((byte[])invoke(bind("get_array",compound,"j",byte[].class,1,String.class),level,"Blocks")).clone();blocks[64]=63;put(level,"Blocks",byte[].class,blocks);
                    invoke(setTag,level,"Entities",list(actor(true,false)));invoke(setTag,level,"TileEntities",list(actor(true,true)));
                }
                put(input,"Level",compound,level);
                if(name.equals("missing_level"))input=filtered(input,"Level");else if(name.equals("wrong_level"))put(input,"Level",int.class,1);
                result.add(runCase(name,input,index++));
            }return result;
        }
        void finish()throws Exception {invoke(close,client?saveFormat:storage);}
    }
    static void threads(Path output,Set<String> originalNames)throws Exception {
        List<Object> rows=new ArrayList<>();Map<Thread,StackTraceElement[]> inventory=Thread.getAllStackTraces();require(inventory.size()<=64,"bounded Java thread inventory");
        for(var entry:inventory.entrySet()){
            Thread thread=entry.getKey();StackTraceElement[] frames=entry.getValue();require(frames.length<=64,"bounded Java stacks");List<Object> stack=new ArrayList<>();boolean original=false;
            for(StackTraceElement frame:frames){original|=originalNames.contains(frame.getClassName());stack.add(frame.toString());}
            require(!original&&(thread==Thread.currentThread()||thread.isDaemon()),"no concurrent original game thread");
            rows.add(object("id",thread.getId(),"name",thread.getName(),"daemon",thread.isDaemon(),"state",thread.getState().name(),"original_frame",original,"frames",stack));
        }
        Method json=ChunkObservation.class.getDeclaredMethod("json",Object.class);json.setAccessible(true);
        Files.writeString(output,(String)invoke(json,null,rows)+"\n",StandardOpenOption.CREATE_NEW);
    }
    public static void main(String[] args){System.setOut(System.err);try{
        require(args.length==4,"jar side copied_owned_world_root snapshots_json");boolean client=args[1].equals("client");require(client||args[1].equals("server"),"side");
        Path jarArgument=Path.of(args[0]).toAbsolutePath().normalize(),rootArgument=Path.of(args[2]).toAbsolutePath().normalize();
        Path jarPath=jarArgument.toRealPath(),root=rootArgument.toRealPath(),output=Path.of(args[3]).toAbsolutePath().normalize();
        require(jarArgument.equals(jarPath)&&rootArgument.equals(root),"canonical nonlinked archive and owned root");
        Path marker=root.resolve(".chunk-storage-owned");
        require(Files.isRegularFile(marker,LinkOption.NOFOLLOW_LINKS)&&Files.size(marker)==30&&Files.readString(marker,StandardCharsets.US_ASCII).equals("fresh chunk storage reference\n"),"owned copied baseline");
        try(var paths=Files.walk(root)){List<Path> entries=paths.limit(257).toList();require(entries.size()<=256,"bounded copied world inventory");long total=0;for(Path p:entries){require(!Files.isSymbolicLink(p),"no linked copied world entries");if(!Files.isDirectory(p,LinkOption.NOFOLLOW_LINKS)){require(Files.isRegularFile(p,LinkOption.NOFOLLOW_LINKS)&&Files.size(p)<=MAX_BYTES,"bounded copied file");total+=Files.size(p);}}require(total<=MAX_BYTES,"bounded copied world bytes");}
        require(!Files.isSymbolicLink(root)&&Files.isDirectory(root.getParent())&&!Files.exists(output,LinkOption.NOFOLLOW_LINKS)&&Files.isDirectory(output.getParent())&&output.getParent().equals(root.getParent())&&!output.startsWith(root),"fresh helper-owned copy and output");
        String expected=client?CLIENT:SERVER;require(Files.size(jarPath)<2*1024*1024&&hash(Files.readAllBytes(jarPath)).equals(expected),"pinned supplied original JAR");
        Path artifacts=Path.of(output+".nbt");Files.createDirectory(artifacts);
        Set<String> originalNames=new HashSet<>();try(JarFile file=new JarFile(jarPath.toFile())){var entries=file.entries();while(entries.hasMoreElements()){String name=entries.nextElement().getName();if(name.endsWith(".class"))originalNames.add(name.substring(0,name.length()-6).replace('/','.'));}}
        URL jar=jarPath.toUri().toURL();try(URLClassLoader loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader())){
            Observer observer=new Observer(jar,loader,root,artifacts,client);Object before=observer.context(),registry=observer.registry();threads(Path.of(output+".threads-before.json"),originalNames);
            Object cases=observer.cases();Object after=observer.context(),registryAfter=observer.registry();observer.finish();threads(Path.of(output+".threads-after.json"),originalNames);
            require(hash(Files.readAllBytes(jarPath)).equals(expected),"original archive preserved");
            Map<String,Object> record=object("schema",1,"scope","untouched original static chunk schemas and direct persistent base-world region loader/writer APIs; no cache coordinator or running game claim","side",args[1],"seed_hex","0000000000000017","jar_sha256",expected,
                    "platform_parent",loader.getParent()==ClassLoader.getPlatformClassLoader(),"class_origins",observer.origins,"bindings",observer.bindings,"fields",observer.fields,"time_api",object("method","a","signature","(J)V","value",1234),
                    "setup",before,"registry_before",registry,"saved_baseline",observer.savedBaseline,"cases",cases,"after",after,"registry_after",registryAfter);
            byte[] encoded=(((String)invoke(observer.json,null,record))+"\n").getBytes(StandardCharsets.UTF_8);require(encoded.length<=MAX_BYTES,"bounded complete output");Files.write(output,encoded,StandardOpenOption.CREATE_NEW);
        }
    }catch(Throwable failure){failure.printStackTrace(System.err);System.exit(1);}}
}
