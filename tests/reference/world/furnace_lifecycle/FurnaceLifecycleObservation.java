import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.StringJoiner;
import java.util.concurrent.atomic.AtomicLong;

/** Genuine saved Furnace and actual World.g variant lifecycle. */
public final class FurnaceLifecycleObservation {
    static fd world;
    static lm[] chunks;
    static int network, natives, execs, deniedWrites, worldWrites;
    static sk original;
    static final java.util.IdentityHashMap<ow,String> tileNames = new java.util.IdentityHashMap<ow,String>();
    static final java.util.LinkedHashMap<String,ow> tiles = new java.util.LinkedHashMap<String,ow>();
    static final java.util.IdentityHashMap<iz,String> stackNames = new java.util.IdentityHashMap<iz,String>();
    static final java.util.LinkedHashMap<String,iz> stacks = new java.util.LinkedHashMap<String,iz>();
    static final String[] CORE = {"sk", "tc", "rw", "ow", "fd", "lm", "mx", "uu", "xp", "pe", "st", "eb", "pm", "ok", "wi", "xa", "iz", "gm", "ey", "nu", "sp", "ei"};

    static void need(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static Map<String,Object> map(Object... pairs) {
        Map<String,Object> value = new LinkedHashMap<String,Object>();
        for (int i = 0; i < pairs.length; i += 2) value.put((String)pairs[i], pairs[i + 1]);
        return value;
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) return "\"" + ((String)value).replace("\\", "\\\\")
            .replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
        if (value instanceof Map) {
            StringJoiner out = new StringJoiner(",", "{", "}");
            for (Map.Entry<?,?> entry : ((Map<?,?>)value).entrySet()) out.add(json(entry.getKey()) + ":" + json(entry.getValue()));
            return out.toString();
        }
        if (value instanceof Iterable) {
            StringJoiner out = new StringJoiner(",", "[", "]");
            for (Object item : (Iterable<?>)value) out.add(json(item));
            return out.toString();
        }
        return value.toString();
    }
    static String sha(byte[] bytes) throws Exception {
        StringBuilder out = new StringBuilder();
        for (byte value : MessageDigest.getInstance("SHA-256").digest(bytes)) out.append(String.format(Locale.ROOT, "%02x", value & 255));
        return out.toString();
    }
    static Object primitive(Field field, Object owner) throws Exception {
        if (field.getType() == float.class) return Float.floatToRawIntBits(field.getFloat(owner));
        if (field.getType() == double.class) return Double.doubleToRawLongBits(field.getDouble(owner));
        return field.get(owner);
    }
    static final class Snapshot {
        final Object owner;
        final Field[] fields;
        final Object[] values;
        Snapshot(Object owner) throws Exception {
            this.owner = owner;
            fields = Arrays.stream(owner.getClass().getDeclaredFields()).filter(f -> !Modifier.isStatic(f.getModifiers()))
                .sorted((a,b) -> a.getName().compareTo(b.getName())).toArray(Field[]::new);
            values = new Object[fields.length];
            for (int i = 0; i < fields.length; ++i) { fields[i].setAccessible(true); values[i] = primitive(fields[i], owner); }
        }
        boolean same() throws Exception {
            for (int i = 0; i < fields.length; ++i) {
                Object value = primitive(fields[i], owner);
                if (fields[i].getType().isPrimitive() ? !values[i].equals(value) : values[i] != value) return false;
            }
            return true;
        }
    }
    static List<Object> random(Random value) throws Exception {
        return List.of(String.format(Locale.ROOT, "%012x", ((AtomicLong)field(Random.class, "seed").get(value)).get()),
            field(Random.class, "haveNextNextGaussian").getBoolean(value),
            String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(field(Random.class, "nextNextGaussian").getDouble(value))));
    }
    static List<Object> queues() throws Exception {
        List<Object> result = new ArrayList<Object>();
        for (Object raw : (List<?>)field(fd.class, "C").get(world)) {
            st item = (st)raw; need(item.a == eb.a || item.a == eb.b, "actual light kind");
            result.add(List.of(item.a == eb.a ? 0 : 1, item.b, item.c, item.d, item.e, item.f, item.g));
        }
        return result;
    }
    static byte[][] arrays() {
        byte[][] result = new byte[chunks.length * 5][];
        for (int i = 0; i < chunks.length; ++i) {
            result[i * 5] = chunks[i].b; result[i * 5 + 1] = chunks[i].e.a;
            result[i * 5 + 2] = chunks[i].f.a; result[i * 5 + 3] = chunks[i].g.a; result[i * 5 + 4] = chunks[i].h;
        }
        return result;
    }
    static String tile(ow value) {
        if(value==null)return null;String name=tileNames.get(value);
        if(name==null){name="T"+tiles.size();tileNames.put(value,name);tiles.put(name,value);}return name;
    }
    static String stack(iz value) {
        if(value==null)return null;String name=stackNames.get(value);
        if(name==null){name="S"+stacks.size();stackNames.put(value,name);stacks.put(name,value);}return name;
    }
    static List<Object> list(List<?> input) {
        List<Object> out=new ArrayList<Object>();for(Object value:input)out.add(tile((ow)value));return out;
    }
    static Object snapshot() throws Exception {
        List<Object> active=list(world.c),pending=list((List<?>)field(fd.class,"G").get(world));
        List<Object> maps=new ArrayList<Object>();
        for(lm chunk:chunks) {
            List<Object> values=new ArrayList<Object>();for(Object value:chunk.l.values())values.add(tile((ow)value));
            maps.add(map("chunk",List.of(chunk.j,chunk.k),"values",values));
        }
        Map<String,Object> observed=map();
        for(Map.Entry<String,ow> entry:tiles.entrySet()) {
            ow value=entry.getValue();need(value.getClass()==sk.class,"actual SK owner");sk furnace=(sk)value;
            List<Object> slots=new ArrayList<Object>();for(iz item:(iz[])field(sk.class,"i").get(value))slots.add(stack(item));
            observed.put(entry.getKey(),map("fields",List.of(furnace.a,furnace.b,furnace.c),"invalid",field(ow.class,"h").getBoolean(value),
                "world_same",value.d==world,"xyz",List.of(value.e,value.f,value.g),"slots",slots));
        }
        Map<String,Object> values=map();
        for(Map.Entry<String,iz> entry:stacks.entrySet()) {
            iz value=entry.getValue();values.put(entry.getKey(),List.of(value.c,value.a,field(iz.class,"d").getInt(value),value.b));
        }
        List<Object> hashes=new ArrayList<Object>();for(byte[] array:arrays())hashes.add(sha(array));
        int at=(8<<11)|(8<<7)|64;
        return map("block",chunks[0].b[at]&255,"metadata",(chunks[0].e.a[at/2]>>((at&1)*4))&15,
            "iterating",field(fd.class,"L").getBoolean(world),"suppression",field(tc.class,"c").getBoolean(null),
            "active",active,"pending",pending,"maps",maps,"tiles",observed,"stacks",values,"array_sha256",hashes,
            "light_queues",queues(),"entities",world.b.size());
    }
    static void begin(int index) {}
    static void end(int index) {}
    static Object primitives(Object owner,Class<?> declared) throws Exception {
        Map<String,Object> out=map();
        Field[] fields=declared.getDeclaredFields();Arrays.sort(fields,(a,b)->a.getName().compareTo(b.getName()));
        for(Field f:fields)if(!Modifier.isStatic(f.getModifiers())&&f.getType().isPrimitive()) {
            f.setAccessible(true);out.put(f.getName(),map("type",f.getType().getName(),"value",primitive(f,owner)));
        }
        return out;
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);need(args.length==3,"jar private World declared steps");
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();
        List<String> profiles=Files.readAllLines(Path.of(args[2]));Map<String,Object> origins=map();
        for(String name:CORE) {
            Class<?> type=Class.forName(name,false,FurnaceLifecycleObservation.class.getClassLoader());
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original class origin");
            try(InputStream input=type.getResourceAsStream("/"+name+".class")){origins.put(name,sha(input.readAllBytes()));}
        }
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
            public void checkListen(int port){++network;throw new SecurityException("listen");}
            public void checkLink(String name){++natives;throw new SecurityException("native");}
            public void checkExec(String name){++execs;throw new SecurityException("exec");}
            public void checkWrite(String name){if(Path.of(name).toAbsolutePath().normalize().startsWith(directory)){++worldWrites;return;}++deniedWrites;throw new SecurityException("outside World");}
            public void checkDelete(String name){checkWrite(name);}
        });
        // Explicit original startup precondition: Blocks before Items. Loading
        // a saved fueled SK before this exposes the genuine circular-init failure.
        Class.forName("uu");Class.forName("gm");
        world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        chunks=new lm[]{world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
        original=(sk)world.b(8,64,8);need(original!=null&&original.getClass()==sk.class,"saved original SK");tile(original);
        need(!world.B,"authoritative World");
        System.out.println("FURNACE_WORLD_ORIGINS "+json(map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins)));
        List<Object> chunkSetup=new ArrayList<Object>();
        for(lm chunk:chunks)chunkSetup.add(map("position",List.of(chunk.j,chunk.k),"fields",primitives(chunk,lm.class)));
        System.out.println("FURNACE_WORLD_SETUP "+json(map("world_class",world.getClass().getName(),
            "provider_class",world.t.getClass().getName(),"provider_fields",primitives(world.t,xa.class),
            "chunk_provider_class",field(fd.class,"v").get(world).getClass().getName(),
            "info_fields",primitives(field(fd.class,"x").get(world),ei.class),"remote",world.B,
            "chunks",chunkSetup,"open_calls",List.of(List.of(0,0),List.of(0,1),List.of(1,0),List.of(1,1)))));
        int index=0;
        for(String profile:profiles) {
            String[] v=profile.split("\t",-1);need(v.length==5,"five declared step fields");
            if(!v[2].equals("-"))original.c=Integer.parseInt(v[2]);
            if(!v[3].equals("-"))original.a=Integer.parseInt(v[3]);
            if(Boolean.parseBoolean(v[4]))((iz[])field(sk.class,"i").get(original))[1]=null;
            Object before=snapshot();List<Object> randomBefore=random(world.r);String error=null,message=null;boolean returned=false;
            begin(index);
            try {if(v[1].equals("world-g"))world.g();else need(v[1].equals("snapshot"),"declared operation");returned=true;}
            catch(RuntimeException failure){error=failure.getClass().getName();message=failure.getMessage();}
            end(index);
            List<Object> randomAfter=random(world.r);
            System.out.println("FURNACE_WORLD_ROW "+json(map("index",index,"name",v[0],"operation",v[1],"returned",returned,"exception",error,"exception_message",message,"before",before,"after",snapshot(),"rng_unchanged",randomBefore.equals(randomAfter))));
            System.out.println("FURNACE_WORLD_RANDOM "+json(map("index",index,"name",v[0],"before",randomBefore,"after",randomAfter)));index++;
        }
        for(int i=0;i<chunks.length;i++)need(world.c(i/2,i%2)==chunks[i],"same canonical loaded Chunk");
        System.out.println("FURNACE_WORLD_COMPLETE "+json(map("rows",index,"network",network,"native",natives,"execs",execs,"denied_writes",deniedWrites,"world_writes",worldWrites,"canonical_chunks",chunks.length)));
        need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no external attempts");
    }
}
