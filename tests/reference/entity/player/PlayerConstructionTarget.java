import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Observe unchanged original concrete constructors; never provide a replacement game class. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class PlayerConstructionTarget {
    public static int activeCase=-1;
    public static fd world;
    public static fd argumentWorld;
    public static gr session;
    public static dc result;
    private static final String[] CREATIVE={"u","x","am","w","y","K","L","ar","al","N","ap","z","ae","af","ag","ah","F","G","M","ac","J","I","H","aj","ai","ao","an","aq"};
    private static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    private static Object ref(Object value) {
        if(value==null) return null;
        Integer id=identities.get(value);
        if(id==null) { id=identities.size()+1; identities.put(value,id); }
        Map<String,Object> out=ConstructorJson.map("class",value.getClass().getName(),"identity",id);
        if(value instanceof String) {
            List<Integer> units=new ArrayList<>();
            for(char c:((String)value).toCharArray()) units.add((int)c);
            out.put("utf16",units);
        }
        if(value.getClass().isArray()) out.put("length",Array.getLength(value));
        return out;
    }
    private static Object get(Object object,Class<?> owner,String name) throws Exception {
        Field field=owner.getDeclaredField(name); field.setAccessible(true); return field.get(object);
    }
    private static Map<String,Object> fields(Object object) throws Exception {
        if(object==null) return null;
        Map<String,Object> out=new TreeMap<>();
        for(Class<?> owner=object.getClass();owner!=Object.class;owner=owner.getSuperclass())
            for(Field field:owner.getDeclaredFields()) {
                if(Modifier.isStatic(field.getModifiers())) continue;
                field.setAccessible(true);
                Object value=field.get(object);
                if(field.getType()==float.class) value=ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits((Float)value)));
                else if(field.getType()==double.class) value=ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)value)));
                else if(field.getType()==long.class) value=ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)value));
                else if(!field.getType().isPrimitive()) value=ref(value);
                out.put(owner.getName()+"."+field.getName(),value);
            }
        return out;
    }
    private static long seed(Random random) throws Exception {
        return ((AtomicLong)get(random,Random.class,"seed")).get();
    }
    private static int nextEntityId() throws Exception {
        return (Integer)get(null,sn.class,"a");
    }
    private static Map<String,Object> watcher(ud watcher) throws Exception {
        List<Object> rows=new ArrayList<>();
        Map<?,?> entries=(Map<?,?>)get(watcher,ud.class,"b");
        for(int index:new int[]{0,16}) {
            ma record=(ma)entries.get(index);
            rows.add(ConstructorJson.map("index",record.a(),"type",record.c(),"value",record.b(),"dirty",get(record,ma.class,"d")));
        }
        return ConstructorJson.map("size",entries.size(),"dirty",get(watcher,ud.class,"c"),"records",rows);
    }
    private static List<Object> creative() throws Exception {
        List<Object> rows=new ArrayList<>();
        if(gr.a.size()!=CREATIVE.length) throw new AssertionError("creative list size");
        for(int i=0;i<CREATIVE.length;++i) {
            uu block=(uu)gr.a.get(i);
            rows.add(ConstructorJson.map("field",CREATIVE[i],"id",block.bn,"same_static",block==uu.class.getField(CREATIVE[i]).get(null)));
        }
        return rows;
    }
    private static Map<String,Object> container(dc player) throws Exception {
        aa container=(aa)player.d;
        mq grid=container.a; wl result=(wl)container.b;
        List<Object> slots=new ArrayList<>();
        for(Object entry:container.e) {
            gp slot=(gp)entry;
            Object inventory=get(slot,gp.class,"e");
            Map<String,Object> row=ConstructorJson.map("class",slot.getClass().getName(),"index",slot.a,"inventory_index",get(slot,gp.class,"d"),"x",slot.b,"y",slot.c,
                "inventory",inventory==player.c?"player":inventory==grid?"crafting":inventory==result?"result":"other");
            if(slot instanceof yv) {
                row.put("same_player",get(slot,yv.class,"e")==player);
                row.put("same_grid",get(slot,yv.class,"d")==grid);
            }
            if(slot instanceof pv) {
                row.put("armor_type",get(slot,pv.class,"d")); row.put("same_container",get(slot,pv.class,"e")==container);
            }
            slots.add(row);
        }
        Object listeners=get(container,dw.class,"g"), blocked=get(container,dw.class,"b");
        Set<Object> collections=Collections.newSetFromMap(new IdentityHashMap<>());
        collections.add(container.d); collections.add(container.e); collections.add(listeners); collections.add(blocked);
        return ConstructorJson.map("fields",fields(container),"slots",slots,"snapshot_count",container.d.size(),"snapshot_nonnull",container.d.stream().filter(Objects::nonNull).count(),
            "listener_count",((List<?>)listeners).size(),"blocked_count",((Set<?>)blocked).size(),"distinct_collection_identities",collections.size()==4,
            "grid_fields",fields(grid),"grid_length",grid.a(),"grid_owner_same",get(grid,mq.class,"c")==container,
            "grid_nonnull",Arrays.stream((Object[])get(grid,mq.class,"a")).filter(Objects::nonNull).count(),
            "result_fields",fields(result),"result_length",result.a(),"result_nonnull",Arrays.stream((Object[])get(result,wl.class,"a")).filter(Objects::nonNull).count());
    }
    private static Map<String,Object> player(dc player) throws Exception {
        List<Object> filters=new ArrayList<>(); Set<Object> distinct=Collections.newSetFromMap(new IdentityHashMap<>());
        for(String name:new String[]{"bN","bO","bP"}) { Object filter=get(player,dc.class,name); filters.add(fields(filter)); distinct.add(filter); }
        return ConstructorJson.map("fields",fields(player),"bounds",fields(player.aW),"watcher",watcher(player.ad()),"inventory",fields(player.c),
            "main_count",player.c.a.length,"armor_count",player.c.b.length,"nonnull_main",Arrays.stream(player.c.a).filter(Objects::nonNull).count(),
            "nonnull_armor",Arrays.stream(player.c.b).filter(Objects::nonNull).count(),"same_player_backref",player.c.d==player,
            "containers_same",player.d==player.e,"container",container(player),"filters",filters,"three_distinct_filters",distinct.size()==3&&!distinct.contains(null),
            "same_world",player.aI==argumentWorld,"minecraft_null",get(player,dc.class,"b")==null,"same_username",session!=null&&player.l==session.b,
            "private_random_seed",String.format(Locale.ROOT,"%012x",seed(player.bs)),"private_random_cached",get(player.bs,Random.class,"haveNextNextGaussian"),
            "private_random_gaussian",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)get(player.bs,Random.class,"nextNextGaussian"))));
    }
    private static String username(int which) {
        if(which==1) return "";
        if(which==2) return null;
        if(which==3) return new String(new char[]{'A',0,'\u03a9','\ud800','Z'});
        if(which==4) return new String("SharedName");
        return "ConstructorProbe";
    }
    private static void bindOrigins() throws Exception {
        Path classes=Path.of(System.getProperty("player.probe.classes"));
        Path binding=Path.of(System.getProperty("player.probe.binding"));
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(binding,StandardCharsets.UTF_8))) {
            out.println("PLAYER_BINDING_V1");
            for(String name:Files.readAllLines(classes,StandardCharsets.US_ASCII)) {
                Class<?> type=Class.forName(name,false,PlayerConstructionTarget.class.getClassLoader());
                byte[] bytes;
                try(InputStream source=type.getResourceAsStream("/"+name.replace('.','/')+".class")) {
                    if(source==null) throw new IllegalStateException("class resource "+name);
                    bytes=source.readAllBytes();
                }
                StringJoiner digest=new StringJoiner("");
                for(byte item:java.security.MessageDigest.getInstance("SHA-256").digest(bytes))
                    digest.add(String.format(Locale.ROOT,"%02x",item&255));
                out.println(name+"\t"+bytes.length+"\t"+digest+"\t"+type.getProtectionDomain().getCodeSource().getLocation().toURI());
            }
            if(out.checkError()) throw new IOException("binding output");
        }
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=1) throw new IllegalArgumentException("output path");
        world=new fd(new wy(),"PlayerConstructorReference",xa.a(0),1234L);
        session=new gr("Setup",""); hk.a();
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);
        Random math=(Random)get(null,holder,"randomNumberGenerator");
        int[][] cases={{11,65,-7,0,0,0},{0,0,0,0,1,0},{-31,-1,48,0,2,-1},
            {2147483647,2147483647,-2147483648,0,3,2147483647},
            {-2147483648,-2147483648,2147483647,1,4,-2147483648},{0,127,0,1,0,7},{4,64,8,0,5,2},{0,0,0,2,0,3}};
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))) {
            out.println(ConstructorJson.json(ConstructorJson.map("event","setup","creative",creative(),"session",fields(session),"world_class",world.getClass().getName(),"save_handler_class","wy","dimension_class",world.t.getClass().getName())));
            for(int index=0;index<cases.length;++index) {
                activeCase=-1; int[] input=cases[index];
                world.a(new br(input[0],input[1],input[2])); world.B=input[3]==1;
                argumentWorld=input[3]==2?null:world;
                session=input[4]==5?null:new gr(username(input[4]),"");
                result=null; long before=seed(math); int entityBefore=nextEntityId();
                Random oracle=new Random(0L); oracle.setSeed(before^0x5deece66dL);
                List<String> draws=new ArrayList<>(); for(int draw=0;draw<4;++draw) draws.add(String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(oracle.nextDouble())));
                activeCase=index; String outcome="success", failure=null;
                try { result=new dc(null,argumentWorld,session,input[5]); }
                catch(NullPointerException exception) { outcome="NullPointerException"; failure=exception.getStackTrace()[0].toString(); }
                activeCase=-1; long after=seed(math);
                if(seed(oracle)!=after) throw new AssertionError("constructor did not consume exactly four Math nextDouble calls");
                if(nextEntityId()!=entityBefore+1) throw new AssertionError("entity counter advance");
                identities.clear(); ref(result); ref(argumentWorld); ref(session);
                Map<String,Object> row=ConstructorJson.map("event","case","case",index,"spawn",Arrays.asList(input[0],input[1],input[2]),"remote",world.B,"null_world",argumentWorld==null,"null_session",session==null,
                    "username",session==null?null:ref(session.b),"dimension",input[5],"outcome",outcome,"failure",failure,
                    "entity_counter_before",entityBefore,"entity_counter_after",nextEntityId(),
                    "math_seed_before",String.format(Locale.ROOT,"%012x",before),"math_seed_after",String.format(Locale.ROOT,"%012x",after),"draws",draws,
                    "session",fields(session),"player",result==null?null:player(result));
                out.println(ConstructorJson.json(row)); out.flush(); System.out.println("CASE "+index+" "+outcome);
            }
            if(out.checkError()) throw new IOException("observation output");
        }
        bindOrigins();
    }
}
