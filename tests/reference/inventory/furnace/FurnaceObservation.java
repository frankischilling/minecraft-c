import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Base64;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Declared inputs to genuine unchanged SK/IZ/IX/BM and NBT owner objects. */
public final class FurnaceObservation {
    static final String[] CORE = {"sk","ow","lw","iz","gm","uu","ln","ey","nu","sp","ij","bm","vq","gp","dw","ix","ec","tc","fd","gs"};
    static final IdentityHashMap<iz,String> identities = new IdentityHashMap<iz,String>();
    static final LinkedHashMap<String,iz> stacks = new LinkedHashMap<String,iz>();
    static int network, nativeLinks, execs, writes;
    static int boundary;
    static sk activeTile;
    static ix activePlayer;
    static iz[] suppliedStacks;
    static String[] suppliedLabels;

    static void need(boolean condition, String text) { if (!condition) throw new AssertionError(text); }
    static Object field(Object value, Class<?> type, String name) throws Exception {
        Field f=type.getDeclaredField(name); f.setAccessible(true); return f.get(value);
    }
    static void store(Object value, Class<?> type, String name, Object input) throws Exception {
        Field f=type.getDeclaredField(name); f.setAccessible(true); f.set(value,input);
    }
    static Object invoke(sk tile, String name, Class<?>[] types, Object... arguments) throws Throwable {
        Method method=sk.class.getDeclaredMethod(name,types);method.setAccessible(true);
        try { return method.invoke(tile,arguments); }
        catch (InvocationTargetException failure) { throw failure.getCause(); }
    }
    public static Map<String,Object> map(Object... values) {
        Map<String,Object> out=new LinkedHashMap<String,Object>();
        for(int i=0;i<values.length;i+=2)out.put((String)values[i],values[i+1]);return out;
    }
    public static String json(Object value) {
        if(value==null)return "null";
        if(value instanceof String) {
            StringBuilder out=new StringBuilder("\"");
            for(char c:((String)value).toCharArray()) {
                if(c=='"'||c=='\\')out.append('\\').append(c);
                else if(c<32||c>=127)out.append(String.format("\\u%04x",(int)c));else out.append(c);
            }
            return out.append('"').toString();
        }
        if(value instanceof Map) {
            StringBuilder out=new StringBuilder("{");
            for(Object raw:((Map<?,?>)value).entrySet()) {
                Map.Entry<?,?> entry=(Map.Entry<?,?>)raw;
                if(out.length()>1)out.append(',');out.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return out.append('}').toString();
        }
        if(value instanceof Iterable) {
            StringBuilder out=new StringBuilder("[");
            for(Object item:(Iterable<?>)value) {if(out.length()>1)out.append(',');out.append(json(item));}
            return out.append(']').toString();
        }
        if(value instanceof Number||value instanceof Boolean)return value.toString();
        throw new AssertionError("closed JSON input "+value.getClass());
    }
    static String digest(byte[] bytes) throws Exception {
        StringBuilder out=new StringBuilder();
        for(byte value:MessageDigest.getInstance("SHA-256").digest(bytes))out.append(String.format("%02x",value&255));
        return out.toString();
    }
    static Object origins() throws Exception {
        Map<String,Object> classes=map();Path jar=null;
        for(String name:CORE) {
            Class<?> type=Class.forName(name,false,FurnaceObservation.class.getClassLoader());
            Path source=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(jar==null)jar=source;need(Files.isRegularFile(source)&&source.equals(jar),"one original source archive");
            try(InputStream input=type.getResourceAsStream("/"+name+".class")) {
                need(input!=null,"original loaded resource");classes.put(name,digest(input.readAllBytes()));
            }
        }
        return map("jar_sha256",digest(Files.readAllBytes(jar)),"classes",classes);
    }
    static String label(iz value) {
        if(value==null)return null;
        String name=identities.get(value);
        if(name==null) {name="N"+(stacks.size()+1);identities.put(value,name);stacks.put(name,value);}
        return name;
    }
    static List<Object> labels(iz[] values) {
        if(values==null)return null;
        List<Object> out=new ArrayList<Object>();for(iz value:values)out.add(label(value));return out;
    }
    static Object snapshot(sk tile,ix player) throws Exception {
        List<Object> slots=tile==null?null:labels((iz[])field(tile,sk.class,"i"));
        List<Object> main=player==null?null:labels(player.a);
        Map<String,Object> values=map();
        for(Map.Entry<String,iz> entry:stacks.entrySet()) {
            iz s=entry.getValue();values.put(entry.getKey(),map("id",s.c,"count",s.a,"damage",field(s,iz.class,"d"),"animation",s.b));
        }
        return map("fields",tile==null?null:List.of(tile.a,tile.b,tile.c),
            "xyz",tile==null?null:List.of(tile.e,tile.f,tile.g),"world_null",tile==null?null:tile.d==null,
            "slots",slots,"player",main,"player_changed",player==null?null:player.e,"stacks",values);
    }
    static void locations(iz[] out,String input) {
        if(input.equals("-"))return;
        for(String word:input.split(";")) {String[] item=word.split(":");out[Integer.parseInt(item[0])]=stacks.get(item[1]);}
    }
    static int[] ints(String value) {String[] words=value.split(",");int[] out=new int[words.length];for(int i=0;i<out.length;i++)out[i]=Integer.parseInt(words[i]);return out;}
    static nu nbt(String[] v) {
        nu out=new nu();int[] xyz=ints(v[15]);out.a("x",xyz[0]);out.a("y",xyz[1]);out.a("z",xyz[2]);
        out.a("BurnTime",(short)Integer.parseInt(v[13]));out.a("CookTime",(short)Integer.parseInt(v[14]));
        sp items=new sp();
        if(!v[12].equals("-"))for(String word:v[12].split(";")) {
            int[] row=ints(word);nu item=new nu();item.a("Slot",(byte)row[0]);item.a("id",(short)row[1]);
            item.a("Count",(byte)row[2]);item.a("Damage",(short)row[3]);items.a(item);
        }
        if(!v[16].equals("missing-items"))out.a("Items",items);
        if(v[16].equals("wrong-items"))out.a("Items",1);
        if(v[16].equals("wrong-burn"))out.a("BurnTime",1);
        return out;
    }
    static String wire(nu value) throws Exception {
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();DataOutputStream output=new DataOutputStream(bytes);
        ij.a(value,output);output.flush();return Base64.getEncoder().encodeToString(bytes.toByteArray());
    }
    static Object recipes() throws Exception {
        Map<String,Object> out=map();
        Map<?,?> values=ey.a().b();
        java.util.TreeSet<Integer> keys=new java.util.TreeSet<Integer>();
        for(Object key:values.keySet())keys.add((Integer)key);
        for(Integer key:keys) {
            iz value=(iz)values.get(key);
            out.put(key.toString(),List.of(value.c,value.a,field(value,iz.class,"d"),value.b));
        }
        return out;
    }
    static Object layout(bm container,sk tile,ix player) throws Exception {
        if(container==null)return null;List<Object> out=new ArrayList<Object>();
        for(Object raw:container.e) {
            gp slot=(gp)raw;Object owner=field(slot,gp.class,"e");
            out.add(List.of(slot.a,owner==tile?"SK":owner==player?"IX":"unknown",field(slot,gp.class,"d"),slot.b,slot.c,slot.getClass().getName()));
        }
        return out;
    }
    public static void begin(int index) { boundary=index; }
    public static void end(int index) { boundary=index; }
    static void observe(int index,String declaration) throws Throwable {
        String[] v=declaration.split("\t",-1);need(v.length==20,"closed declaration");
        identities.clear();stacks.clear();
        if(!v[8].equals("-"))for(String word:v[8].split(";")) {
            String[] row=word.split(":");iz value=new iz(Integer.parseInt(row[1]),Integer.parseInt(row[2]),Integer.parseInt(row[3]));
            value.b=Integer.parseInt(row[4]);stacks.put(row[0],value);identities.put(value,row[0]);
        }
        sk tile=new sk();tile.a=Integer.parseInt(v[4]);tile.b=Integer.parseInt(v[5]);tile.c=Integer.parseInt(v[6]);
        int[] xyz=ints(v[7]);tile.e=xyz[0];tile.f=xyz[1];tile.g=xyz[2];
        iz[] array=(iz[])field(tile,sk.class,"i");locations(array,v[9]);
        if(Boolean.parseBoolean(v[17]))store(tile,sk.class,"i",null);
        ix player=Boolean.parseBoolean(v[18])?null:new ix(null);if(player!=null)locations(player.a,v[10]);
        if(Boolean.parseBoolean(v[19]))tile=null;
        bm container=null;if(v[1].startsWith("bm-")&&!v[1].equals("bm-layout"))container=new bm(player,tile);
        Object before=snapshot(tile,player);sk previous=tile;Object previousArray=tile==null?null:field(tile,sk.class,"i");
        String output=null,error=null,message=null;Object result=null;boolean returned=false;
        int slot=Integer.parseInt(v[2]),amount=Integer.parseInt(v[3]);
        nu input=nbt(v),written=null;iz argument=stacks.get(v[11]);
        activeTile=tile;activePlayer=player;
        suppliedStacks=stacks.values().toArray(new iz[0]);suppliedLabels=stacks.keySet().toArray(new String[0]);
        begin(index);
        try {
            switch(v[1]) {
                case "construct": tile=new sk();break;
                case "size":result=tile.a();break;
                case "name":result=tile.c();break;
                case "limit":result=tile.d();break;
                case "get":result=label(tile.f_(slot));break;
                case "set":tile.a(slot,argument);break;
                case "remove":result=label(tile.a(slot,amount));break;
                case "cook-scale":result=tile.b(amount);break;
                case "burn-scale":result=tile.c(amount);break;
                case "burning":result=tile.b();break;
                case "fuel":result=invoke(tile,"a",new Class<?>[]{iz.class},argument);break;
                case "can-smelt":result=invoke(tile,"l",new Class<?>[]{});break;
                case "smelt":tile.k();break;
                case "read":tile.a(input);break;
                case "write":written=new nu();tile.b(written);break;
                case "roundtrip":written=new nu();tile.b(written);tile=new sk();tile.a(written);break;
                case "tick-null":tile.n_();break;
                case "usable-null":result=tile.a_((gs)null);break;
                case "bm-layout":container=new bm(player,tile);break;
                case "bm-shift":result=label(container.a(slot));break;
                case "bm-update":container.a();break;
                case "bm-progress":container.a(slot,amount);break;
                case "bm-usable":result=container.b((gs)null);break;
                default:throw new AssertionError("declared operation "+v[1]);
            }
            returned=true;
        } catch(RuntimeException failure) {error=failure.getClass().getName();message=failure.getMessage();}
          catch(Throwable failure) {if(failure instanceof Error)throw failure;error=failure.getClass().getName();message=failure.getMessage();}
        end(index);
        if(written!=null)output=wire(written);
        Object after=snapshot(tile,player);
        Object caches=container==null?null:List.of(field(container,bm.class,"b"),field(container,bm.class,"c"),field(container,bm.class,"h"));
        System.out.println("FURNACE_ROW "+json(map("index",index,"profile",v[0],"operation",v[1],"returned",returned,
            "result",result,"exception",error,"exception_message",message,"before",before,"after",after,
            "tile_same",tile==previous,"array_same",tile!=null&&field(tile,sk.class,"i")==previousArray,
            "layout",layout(container,tile,player),"bm_caches",caches,"nbt_base64",output,
            "input_nbt_base64",v[1].equals("read")?wire(input):null)));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Throwable {
        need(args.length==1,"one declared input file");Object origins=origins();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission value) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listen");}
            public void checkLink(String name) {++nativeLinks;throw new SecurityException("native link");}
            public void checkExec(String command) {++execs;throw new SecurityException("process");}
            public void checkWrite(String path) {++writes;throw new SecurityException("write");}
            public void checkDelete(String path) {++writes;throw new SecurityException("delete");}
        });
        Class.forName("uu");Class.forName("gm");
        System.out.println("FURNACE_ORIGINS "+json(origins));
        System.out.println("FURNACE_RECIPES_BEFORE "+json(recipes()));int rows=0;
        try(BufferedReader reader=Files.newBufferedReader(Path.of(args[0]),StandardCharsets.UTF_8)) {
            String line;while((line=reader.readLine())!=null)observe(rows++,line);
        }
        System.out.println("FURNACE_RECIPES_AFTER "+json(recipes()));
        System.out.println("FURNACE_COMPLETE "+json(map("rows",rows,"network",network,"native",nativeLinks,"execs",execs,"writes",writes)));
        need(network==0&&nativeLinks==0&&execs==0&&writes==0,"no forbidden external operation");
    }
}
