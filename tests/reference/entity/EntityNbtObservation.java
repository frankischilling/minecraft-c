import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Finite actual original calls. Never writes static/final/ID/Random fields. */
public final class EntityNbtObservation {
    private static final String[] CASES = {"save_narrow", "save_dead", "factory", "factory_missing_item",
        "read_good", "read_missing_item", "motion_nan_ten", "motion_infinities", "motion_neighbors",
        "motion_short", "motion_wrong", "pos_wrong", "rotation_short", "fire_wrong", "count_wrong",
        "getter_wrong", "null_compound", "empty_vectors", "angle_infinity", "missing_scalars"};
    private static final String[] KEYS = {"x", "y", "z", "previous_x", "previous_y", "previous_z",
        "interpolation_x", "interpolation_y", "interpolation_z", "velocity_x", "velocity_y", "velocity_z",
        "yaw", "pitch", "previous_yaw", "previous_pitch", "fall", "y_size", "offset", "width", "height",
        "fire", "air", "ground", "health", "age"};
    private static final String[] CLIENT = {"aM","aN","aO","aJ","aK","aL","bl","bm","bn","aP","aQ","aR",
        "aS","aT","aU","aV","bk","bo","bf","bg","bh","bv","bz","aX","f","b"};
    private static final String[] SERVER = {"aP","aQ","aR","aM","aN","aO","bo","bp","bq","aS","aT","aU",
        "aV","aW","aX","aY","bn","br","bi","bj","bk","by","bC","ba","f","b"};
    private static final String[] TAG_NAMES = {"id","Pos","Motion","Rotation","FallDistance","Fire","Air",
        "OnGround","Health","Age","Item"};
    private final boolean client;
    private final Class<?> base, item, world, stack, compound, list, tag, box, registry;
    private final Constructor<?> worldConstructor, fullConstructor, stackConstructor;
    private final StringBuilder output = new StringBuilder();

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private EntityNbtObservation(boolean side, URLClassLoader loader, URL jar) throws Exception {
        client = side;
        base = load(loader, side ? "sn" : "lq", jar);
        item = load(loader, side ? "hl" : "ez", jar);
        world = load(loader, side ? "fd" : "dj", jar);
        stack = load(loader, side ? "iz" : "fy", jar);
        compound = load(loader, side ? "nu" : "iq", jar);
        list = load(loader, side ? "sp" : "lr", jar);
        tag = load(loader, side ? "ij" : "fo", jar);
        box = load(loader, side ? "eq" : "cz", jar);
        registry = load(loader, side ? "jc" : "ga", jar);
        worldConstructor = item.getDeclaredConstructor(world);
        fullConstructor = item.getDeclaredConstructor(world,double.class,double.class,double.class,stack);
        stackConstructor = stack.getDeclaredConstructor(int.class,int.class,int.class);
        worldConstructor.setAccessible(true);
        fullConstructor.setAccessible(true);
        stackConstructor.setAccessible(true);
    }

    private static Class<?> load(URLClassLoader loader, String name, URL jar) throws Exception {
        Class<?> type = Class.forName(name, false, loader);
        require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null &&
                type.getProtectionDomain().getCodeSource().getLocation().equals(jar), "actual pinned original class: " + name);
        return type;
    }

    private static Field field(Class<?> owner, String name) throws Exception {
        Field value = owner.getDeclaredField(name);
        require(!Modifier.isStatic(value.getModifiers()), "instance field only: " + name);
        value.setAccessible(true);
        return value;
    }

    private Object get(Class<?> owner, String name, Object receiver) throws Exception {
        return field(owner, name).get(receiver);
    }

    private void prepare(Object receiver, boolean saving) throws Exception {
        /* Explicit finite scalar fixture values, never a constructor/ID/phase
         * substitute. No reference field, static, final or Random is written. */
        double[] doubles = {-101, saving ? 16777217 : -102, -103, -201,-202,-203,-301,-302,-303,-1,-2,-3};
        float[] floats = {saving ? 725 : 11,saving ? -725 : 12,13,14,2.25f,0.5f};
        for (int i = 0; i < 12; ++i) setFixture(base, receiver, i, doubles[i]);
        for (int i = 0; i < 6; ++i) setFixture(base, receiver, i + 12, floats[i]);
        setFixture(base, receiver, 21, saving ? 65535 : 7);
        setFixture(base, receiver, 22, saving ? 65537 : 8);
        setFixture(base, receiver, 23, false);
        setFixture(item, receiver, 24, saving ? 384 : 37);
        setFixture(item, receiver, 25, saving ? 98305 : 41);
    }

    private void setFixture(Class<?> owner, Object receiver, int index, Object value) throws Exception {
        require(index >= 0 && index < KEYS.length && !(index >= 18 && index <= 20), "allowed scalar fixture index");
        Field member = field(owner, (client ? CLIENT : SERVER)[index]);
        require(!Modifier.isFinal(member.getModifiers()) && member.getType().isPrimitive(), "mutable scalar fixture");
        member.set(receiver, value);
    }

    private static Method method(Class<?> owner, String name, Class<?>... parameters) throws Exception {
        Method value = owner.getDeclaredMethod(name, parameters);
        value.setAccessible(true);
        return value;
    }

    private Object fresh(Class<?> type) throws Exception {
        Constructor<?> constructor = type.getDeclaredConstructor();
        constructor.setAccessible(true);
        return constructor.newInstance();
    }

    @SuppressWarnings("unchecked")
    private Map<String,Object> map(Object value) throws Exception {
        return (Map<String,Object>) get(compound, "a", value);
    }

    private void put(Object root, String name, Class<?> type, Object value) throws Exception {
        method(compound,"a",String.class,type).invoke(root,name,value);
    }

    private Object scalar(Class<?> type, Object value) throws Exception {
        Object temporary = fresh(compound);
        put(temporary,"v",type,value);
        return map(temporary).get("v");
    }

    private Object vector(Object[] values, Class<?>[] types, int length) throws Exception {
        Object result = fresh(list);
        for (int i = 0; i < length; ++i) method(list,"a",tag).invoke(result, scalar(types[i],values[i]));
        return result;
    }

    private Object record(String scenario) throws Exception {
        if (scenario.equals("null_compound")) return null;
        Object root = fresh(compound);
        if (scenario.equals("empty_vectors")) return root;
        put(root,"id",String.class,"Item");
        Object[] positions = {8.0,16.0,32.0}, motion = {20.0,30.0,40.0}, angles = {725.0f,-725.0f};
        Class<?>[] positionTypes = {double.class,double.class,double.class};
        Class<?>[] motionTypes = positionTypes.clone(), angleTypes = {float.class,float.class};
        if (scenario.equals("motion_nan_ten")) {
            motion = new Object[] {Double.longBitsToDouble(0x7ff8000000000042L),10.0,-10.0};
        } else if (scenario.equals("motion_infinities")) {
            motion = new Object[] {Double.POSITIVE_INFINITY,Double.NEGATIVE_INFINITY,-0.0};
        } else if (scenario.equals("motion_neighbors")) {
            motion = new Object[] {Double.longBitsToDouble(0x4024000000000001L),
                Double.longBitsToDouble(0xc024000000000001L),Double.longBitsToDouble(0x4023ffffffffffffL)};
        } else if (scenario.equals("motion_wrong")) { motion[1] = 9; motionTypes[1] = int.class; }
        if (scenario.equals("pos_wrong")) { positions[1] = 9; positionTypes[1] = int.class; }
        if (scenario.equals("angle_infinity")) angles = new Object[] {Float.POSITIVE_INFINITY,-0.0f};
        if (!scenario.equals("getter_wrong"))
            put(root,"Pos",tag,vector(positions,positionTypes,3));
        if (scenario.equals("getter_wrong")) put(root,"Motion",int.class,9);
        else put(root,"Motion",tag,vector(motion,motionTypes,scenario.equals("motion_short") ? 2 : 3));
        put(root,"Rotation",tag,vector(angles,angleTypes,scenario.equals("rotation_short") ? 1 : 2));
        if (scenario.equals("missing_scalars")) return root;
        put(root,"FallDistance",float.class,3.5f);
        if (scenario.equals("fire_wrong")) put(root,"Fire",int.class,9);
        else put(root,"Fire",short.class,(short)-19);
        put(root,"Air",short.class,(short)298);
        put(root,"OnGround",byte.class,(byte)-2);
        put(root,"Health",short.class,(short)(scenario.equals("count_wrong") ? 128 : 511));
        put(root,"Age",short.class,(short)(scenario.equals("count_wrong") ? -123 : -17));
        if (!scenario.equals("factory_missing_item") && !scenario.equals("read_missing_item")) {
            Object itemTag = fresh(compound);
            put(itemTag,"id",short.class,(short)-7);
            if (scenario.equals("count_wrong")) put(itemTag,"Count",int.class,9);
            else put(itemTag,"Count",byte.class,(byte)-3);
            put(itemTag,"Damage",short.class,(short)-11);
            put(root,"Item",compound,itemTag);
        }
        return root;
    }

    private void row(Object... values) {
        for (int i = 0; i < values.length; ++i) {
            if (i != 0) output.append('\t');
            output.append(values[i]);
        }
        output.append('\n');
        require(output.length() <= 1048576, "bounded finite observation output");
    }

    private static String hex(long value, int width) {
        if (width != 16) value &= (1L << (width * 4)) - 1;
        String digits = Long.toHexString(value);
        return "0".repeat(width - digits.length()) + digits;
    }

    private static String primitive(Object value) {
        if (value instanceof Double) return hex(Double.doubleToRawLongBits((Double)value),16);
        if (value instanceof Float) return hex(Float.floatToRawIntBits((Float)value),8);
        if (value instanceof Boolean) return (Boolean)value ? "1" : "0";
        return hex(((Number)value).longValue(),8);
    }

    private void snapshot(String name, String stage, Object entity) throws Exception {
        require(get(base,client ? "aI" : "aL",entity) == null, "actual NULL construction World");
        for (int i = 0; i < KEYS.length; ++i)
            row("FIELD",name,stage,KEYS[i],primitive(get(i >= 24 ? item : base,(client ? CLIENT : SERVER)[i],entity)));
        Object bounds = get(base,client ? "aW" : "aZ",entity);
        String[] boxKeys = {"min_x","min_y","min_z","max_x","max_y","max_z"};
        for (int i = 0; i < 6; ++i) row("FIELD",name,stage,"bounds_" + boxKeys[i],primitive(get(box,"" + (char)('a'+i),bounds)));
        Object value = get(item,"a",entity);
        row("FIELD",name,stage,"stack_present",value == null ? "0" : "1");
        if (value != null) {
            String[] keys = {"stack_id","stack_count","stack_damage","stack_animation"};
            String[] fields = {"c","a","d","b"};
            for (int i = 0; i < 4; ++i) row("FIELD",name,stage,keys[i],primitive(get(stack,fields[i],value)));
        }
        String[] extraKeys = {"id","dead","entity_membership","chunk_x","chunk_y","chunk_z","pickup_delay","update_counter"};
        String[] extraClient = {"aD","be","bF","bG","bH","bI","c","e"};
        String[] extraServer = {"aG","bh","bG","bH","bI","bJ","c","e"};
        for (int i = 0; i < extraKeys.length; ++i)
            row("FIELD",name,stage,extraKeys[i],primitive(get(i >= 6 ? item : base,(client ? extraClient : extraServer)[i],entity)));
        row("FIELD",name,stage,"world_null","1");
        Object watcher = get(base,client ? "bD" : "bE",entity);
        require(watcher != null && watcher.getClass().getName().equals(client ? "ud" : "mo"), "actual watcher owner");
        Map<?,?> watched = (Map<?,?>)get(watcher.getClass(),"b",watcher);
        Object record = watched.get(Integer.valueOf(0));
        require(record != null && record.getClass().getName().equals(client ? "ma" : "ht"), "actual watcher record");
        row("FIELD",name,stage,"watcher_size",primitive(watched.size()));
        row("FIELD",name,stage,"watcher_index",primitive(get(record.getClass(),"b",record)));
        row("FIELD",name,stage,"watcher_type",primitive(get(record.getClass(),"a",record)));
        row("FIELD",name,stage,"watcher_value",primitive(get(record.getClass(),"c",record)));
        row("FIELD",name,stage,"watcher_record_dirty",primitive(get(record.getClass(),"d",record)));
        row("FIELD",name,stage,"watcher_dirty",primitive(get(watcher.getClass(),"c",watcher)));
    }

    @SuppressWarnings("unchecked")
    private void tags(String name, String stage, String path, Object value) throws Exception {
        if (value == null) { row("TAG",name,stage,path,"NULL","-"); return; }
        int kind = ((Byte)method(tag,"a").invoke(value)) & 255;
        String payload;
        if (kind == 10) payload = Integer.toString(map(value).size());
        else if (kind == 9) payload = (((Byte)get(list,"b",value)) & 255) + "," + ((List<?>)get(list,"a",value)).size();
        else if (kind == 8) {
            String text = (String)get(value.getClass(),"a",value);
            StringBuilder units = new StringBuilder();
            for (int i = 0; i < text.length(); ++i) units.append(hex(text.charAt(i),4));
            payload = units.length() == 0 ? "-" : units.toString();
        } else payload = primitive(get(value.getClass(),"a",value));
        row("TAG",name,stage,path,kind,payload);
        if (kind == 10) {
            for (Map.Entry<String,Object> entry : new TreeMap<>(map(value)).entrySet())
                tags(name,stage,path + "/" + entry.getKey(),entry.getValue());
        } else if (kind == 9) {
            List<?> children = (List<?>)get(list,"a",value);
            for (int i = 0; i < children.size(); ++i) tags(name,stage,path + "/" + i,children.get(i));
        }
    }

    private void bytes(String name, Object root) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (DataOutputStream stream = new DataOutputStream(bytes)) {
            method(tag,"a",tag,java.io.DataOutput.class).invoke(null,root,stream);
        }
        require(bytes.size() <= 65536, "bounded original encoded NBT");
        row("BYTES",name,HexFormat.of().formatHex(bytes.toByteArray()));
    }

    private String outcome(String name, Throwable error) {
        require(error.getClass() == ClassCastException.class || error instanceof IndexOutOfBoundsException ||
                error.getClass() == NullPointerException.class, "unexpected original Throwable: " + error);
        StringWriter text = new StringWriter();
        error.printStackTrace(new PrintWriter(text));
        require(text.getBuffer().length() <= 32768, "bounded complete Throwable");
        row("THROW",name,error.getClass().getName(),HexFormat.of().formatHex(text.toString().getBytes(StandardCharsets.UTF_8)));
        if (error instanceof ClassCastException) return "CAST";
        return error instanceof IndexOutOfBoundsException ? "INDEX" : "NULL";
    }

    private void run(String side) throws Exception {
        row("ENTITY_NBT_V1",side);
        row("SCOPE","PREPARED_SCALARS","EXCLUDE_ONLY_RANDOM_INTERNALS_AND_HOVER_FROM_SELECTED_OWNER_FIELDS");
        for (String name : CASES) {
            boolean saving = name.startsWith("save_"), factory = name.startsWith("factory");
            row("CASE",name,saving ? "SAVE" : factory ? "FACTORY" : "READ");
            Object input = saving ? fresh(compound) : record(name);
            if (name.equals("save_dead")) put(input,"Marker",int.class,123);
            if (!saving) tags(name,"input","$",input);
            Object entity = null, bounds = null, oldStack = null;
            if (!factory) {
                if (saving) entity = fullConstructor.newInstance(null,8.0,16.0,32.0,
                    stackConstructor.newInstance(98305,511,65538));
                else {
                    entity = worldConstructor.newInstance(new Object[] {null});
                    method(base,"e",compound).invoke(entity,record("read_good"));
                }
                prepare(entity,saving);
                if (name.equals("save_dead")) {
                    Field dead = field(base,client ? "be" : "bh");
                    require(dead.getType() == boolean.class && !Modifier.isFinal(dead.getModifiers()), "source dead fixture");
                    dead.setBoolean(entity,true);
                }
                snapshot(name,"before",entity);
                bounds = get(base,client ? "aW" : "aZ",entity);
                oldStack = get(item,"a",entity);
            }
            String result;
            try {
                if (saving) result = (Boolean)method(base,"c",compound).invoke(entity,input) ? "TRUE" : "FALSE";
                else if (factory) {
                    entity = method(registry,"a",compound,world).invoke(null,input,null);
                    require(entity != null && entity.getClass() == item, "actual Item factory result");
                    result = "OK";
                } else { method(base,"e",compound).invoke(entity,input); result = "OK"; }
            } catch (InvocationTargetException error) { result = outcome(name,error.getCause()); }
            row("RESULT",name,result);
            require(entity != null, "observable actual Entity required");
            snapshot(name,"after",entity);
            row("REF",name,factory ? "NA" : bounds == get(base,client ? "aW" : "aZ",entity) ? "1" : "0",
                factory ? "NA" : oldStack == get(item,"a",entity) ? "1" : "0");
            if (oldStack != null) row("HELD",name,primitive(get(stack,"c",oldStack)),primitive(get(stack,"a",oldStack)),
                primitive(get(stack,"d",oldStack)),primitive(get(stack,"b",oldStack)));
            if (saving) { tags(name,"output","$",input); bytes(name,input); }
        }
        row("END",CASES.length);
        PrintWriter writer = new PrintWriter(System.out,false,StandardCharsets.US_ASCII);
        writer.print(output); writer.flush();
        require(!writer.checkError(), "complete output transport");
    }

    private static void pin(Path jar, boolean client) throws Exception {
        require(Files.size(jar) == (client ? 1465375 : 503100), "pinned original JAR size");
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
            : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        require(HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar))).equals(expected),
            "untouched original JAR bytes");
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server")))
            throw new IllegalArgumentException("EntityNbtObservation client|server jar");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        pin(jar,client);
        URL url = jar.toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url},ClassLoader.getPlatformClassLoader())) {
            new EntityNbtObservation(client,loader,url).run(args[0]);
        }
        pin(jar,client);
    }
}
