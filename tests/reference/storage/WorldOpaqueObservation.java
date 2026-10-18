import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Generic storage transport observation; no activation or gameplay. */
public final class WorldOpaqueObservation {
    static void check(boolean value,String text) { WorldStorageObservation.check(value,text); }
    static Map<String,Object> map(Object... args) { return WorldStorageObservation.map(args); }
    static Object get(Object object,String name) throws Exception { return WorldStorageObservation.get(object,name); }
    static Object call(Method method,Object owner,Object...args) throws Exception { return WorldStorageObservation.invoke(method,owner,args); }
    static Map<String,Object> chunk(WorldStorageObservation observer,WorldStorageObservation.Context context,Object value) throws Exception {
        Map<String,Object> data=map();String[] names={"blocks","metadata","skylight","block_light","heightmap"};
        Object[] arrays={get(value,"b"),get(get(value,"e"),"a"),get(get(value,"f"),"a"),get(get(value,"g"),"a"),get(value,"h")};
        int[] sizes={32768,16384,16384,16384,256};
        for(int i=0;i<5;i++) {check(((byte[])arrays[i]).length==sizes[i],"complete arrays");data.put(names[i],WorldStorageObservation.blob((byte[])arrays[i]));}
        Map<String,Object> flags=map();for(String name:new String[]{"c","n","o","p","q"})flags.put(name,get(value,name));
        Object[] slices=(Object[])get(value,"m");check(slices.length==8,"eight slices");List<Object> entities=new ArrayList<>();
        for(int i=0;i<8;i++){List<?> list=(List<?>)slices[i];check(list.size()<=4,"finite entity inventory");for(Object entity:list){observer.type(entity.getClass().getName());entities.add(map("slice",i,"class",entity.getClass().getName(),"id",observer.id(entity)));}}
        Map<?,?> tiles=(Map<?,?>)get(value,"l");check(tiles.size()<=4,"finite tile inventory");List<Object> tileRows=new ArrayList<>();
        for(var entry:tiles.entrySet()) {Object tile=entry.getValue();observer.type(tile.getClass().getName());observer.type(entry.getKey().getClass().getName());Map<String,Object> fields=new TreeMap<>();for(Field field:entry.getKey().getClass().getDeclaredFields())if(!Modifier.isStatic(field.getModifiers())){check(field.getType()==int.class,"position integer field");field.setAccessible(true);fields.put(field.getName(),field.getInt(entry.getKey()));}tileRows.add(map("class",tile.getClass().getName(),"id",observer.id(tile),"key_class",entry.getKey().getClass().getName(),"position",fields));}
        check(entities.size()==1&&tileRows.size()==1,"one genuine loaded Boat and Sign");
        return map("id",observer.id(value),"coordinates",List.of(get(value,"j"),get(value,"k")),"world_matches",get(value,"d")==context.world,
                   "data",data,"flags",flags,"minimum",get(value,"i"),"last_save",WorldStorageObservation.hex((long)get(value,"r")),"entities",entities,"tiles",tileRows);
    }
    static List<Object> threads(Set<String> originals) {
        List<Object> rows=new ArrayList<>();Map<Thread,StackTraceElement[]> inventory=Thread.getAllStackTraces();check(inventory.size()<=64,"bounded threads");
        for(var entry:inventory.entrySet()){Thread thread=entry.getKey();List<String> frames=new ArrayList<>();boolean original=false;check(entry.getValue().length<=128,"bounded frames");for(StackTraceElement frame:entry.getValue()){frames.add(frame.toString());original|=originals.contains(frame.getClassName());}check(!original&&(thread==Thread.currentThread()||thread.isDaemon()),"no concurrent original frame");rows.add(map("name",thread.getName(),"daemon",thread.isDaemon(),"original_frame",original,"frames",frames));}return rows;
    }
    public static void main(String[] args) {
        System.setOut(System.err);
        try {
            check(args.length==5||args.length==6,"jar side owned_parent observed|control|prepare|metadata output [raw_input]");boolean client=args[1].equals("client");check(client||args[1].equals("server"),"side");boolean metadata=args[3].equals("metadata"),prepare=args[3].equals("prepare")||metadata;boolean observed=args[3].equals("observed")||prepare;check(observed||args[3].equals("control"),"mode");check(prepare==(args.length==6),"prepare owns explicit input only");
            Path archive=Path.of(args[0]).toAbsolutePath().normalize(),parent=Path.of(args[2]).toAbsolutePath().normalize(),output=Path.of(args[4]).toAbsolutePath().normalize();
            check(archive.equals(archive.toRealPath())&&parent.equals(parent.toRealPath()),"canonical owned paths");
            check(Files.readString(parent.resolve(".opaque-world-owned")).equals("private copied opaque world\n"),"owned copied world marker");
            check(!Files.exists(output)&&Files.isRegularFile(parent.resolve("world/level.dat")),"new output and existing metadata");
            List<Object> passes=new ArrayList<>();Map<String,Object> allOrigins=map();
            if(prepare){
                WorldStorageObservation observer=new WorldStorageObservation(archive,client,true,parent,output);
                if(!metadata)observer.new Context(0,true);Path input=Path.of(args[5]).toRealPath();check(Files.size(input)<=1024*1024,"bounded original raw input");
                Class<?> utility=observer.type(client?"as":"ag"),regions=observer.type(client?"rj":"kq");
                Object tree;try(DataInputStream stream=new DataInputStream(Files.newInputStream(input))){tree=call(WorldStorageObservation.method(utility,"a",observer.compoundType,DataInput.class),null,stream);check(stream.read()==-1,"complete input NBT");}
                long start=System.currentTimeMillis();if(metadata){try(OutputStream stream=Files.newOutputStream(parent.resolve("world/level.dat"),StandardOpenOption.TRUNCATE_EXISTING)){call(WorldStorageObservation.method(utility,"a",void.class,observer.compoundType,OutputStream.class),null,tree,stream);}}
                else try(DataOutputStream stream=(DataOutputStream)call(WorldStorageObservation.method(regions,"d",DataOutputStream.class,File.class,int.class,int.class),null,parent.resolve("world").toFile(),1,0)){
                    check(stream!=null,"actual global region output");call(WorldStorageObservation.method(utility,"a",void.class,observer.compoundType,DataOutput.class),null,tree,stream);
                }long end=System.currentTimeMillis();observer.closeRegions();
                Map<String,Object> result=map("schema",1,"scope",metadata?"original generic NBT gzip writer input; no metadata Info serializer retention claim":"original raw fixture installed through actual original global region/NBT writer", "side",args[1],"mode",args[3],"jar_sha256",WorldStorageObservation.sha(Files.readAllBytes(archive)),"class_origins",observer.origins,"bindings",WorldStorageObservation.BINDINGS,"fields",WorldStorageObservation.FIELDS,"threads",WorldStorageObservation.threadInventory(WorldStorageObservation.originalClassNames(archive)),"input",WorldStorageObservation.blob(Files.readAllBytes(input)),"write_interval_ms",List.of(start,end),"events",observer.events,"files",observer.files(),"roots",observer.roots());
                StringBuilder json=new StringBuilder();WorldStorageObservation.json(result,json);Files.writeString(output,json+"\n",StandardOpenOption.CREATE_NEW);return;
            }
            for(int pass=0;pass<2;pass++) {
                WorldStorageObservation observer=new WorldStorageObservation(archive,client,observed,parent,output);
                WorldStorageObservation.Context context=observer.new Context(0,true);context.time(1234L);
                Object value=context.load(1,0);Map<String,Object> before=chunk(observer,context,value);
                check(!(boolean)get(value,"c")&&!(boolean)get(value,"o")&&(boolean)get(value,"q")&&(long)get(value,"r")==0,"generic loader defaults and hasEntities only");
                Object after=before;List<Long> interval=null;
                if(pass==0){long start=System.currentTimeMillis();call(WorldStorageObservation.method(context.chunkLoader.getClass(),"a",void.class,observer.worldType,observer.chunkType),context.chunkLoader,context.world,value);long end=System.currentTimeMillis();interval=List.of(start,end);after=chunk(observer,context,value);check(before.equals(after),"generic writer retains complete loaded object state");context.fullSave("opaque-direct-load-no-cache-activation");}
                observer.closeRegions();allOrigins.putAll(observer.origins);
                passes.add(map("pass",pass,"before",before,"after",after,"write_interval_ms",interval,"events",observer.events,"files",observer.files(),"roots",observer.roots(),"threads",WorldStorageObservation.threadInventory(WorldStorageObservation.originalClassNames(archive))));
            }
            Map<String,Object> result=map("schema",1,"scope","generic copied-world Boat and Sign loader/factory/writer; no activation or feature simulation", "side",args[1],"mode",args[3],"jar_sha256",WorldStorageObservation.sha(Files.readAllBytes(archive)),"class_origins",allOrigins,"bindings",WorldStorageObservation.BINDINGS,"fields",WorldStorageObservation.FIELDS,"passes",passes);
            StringBuilder json=new StringBuilder();WorldStorageObservation.json(result,json);check(json.length()<=4*1024*1024,"bounded record");Files.writeString(output,json+"\n",StandardOpenOption.CREATE_NEW);
        } catch(Throwable failure) {failure.printStackTrace(System.err);System.exit(1);}
    }
}
