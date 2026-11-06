import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;

/** Enter unchanged crossed-plant geometry on actual Registry/CV/NW owners. */
public final class PlantObservation {
    static int network, nativeLinks, writes;
    static final String[] LIGHT = {"n","o","p","q","r","s","t","u","v","w","x","y","z",
        "A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] FLAGS = {"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final String[] COLORS = {"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] BOUNDS = {"bs","bt","bu","bv","bw","bx"};
    static final int[] CORNERS = {0x3e000000,0x3e800000,0x3f000000,0x3f400000,
        0x3f600000,0x3f200000,0x3ec00000,0x3d800000,0x3f800000,0x3f000001,0xbe800000,0x7fc12345};
    static final double[][] POINTS = {{0,0,0},{-3,50,39},{-16777215.75,65,256.75}};
    static final int[] CAPACITIES = {0,4,8,16,32,47};
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> owner, String name) throws Exception {
        Field result = owner.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }
    static void original(Class<?> type, Path jar) throws Exception {
        require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
            .toRealPath().equals(jar), "unchanged original class: " + type.getName());
    }
    static void word(StringBuilder row, int value) {
        row.append(String.format(Locale.ROOT, " %08x", value));
    }
    static void wide(StringBuilder row, double value) {
        row.append(String.format(Locale.ROOT, " %016x", Double.doubleToRawLongBits(value)));
    }
    static String observe(int index, int[] profile, cv renderer, uu block,
                          nw tessellator, String exception) throws Exception {
        StringBuilder row = new StringBuilder("PLANT " + index);
        for (int value : profile) row.append(' ').append(value);
        row.append(' ').append(field(cv.class,"m").getBoolean(renderer) ? 1 : 0);
        for (String name : LIGHT) word(row, Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        int flags = 0;
        for (int i=0;i<FLAGS.length;++i) if (field(cv.class,FLAGS[i]).getBoolean(renderer)) flags |= 1<<i;
        row.append(' ').append(flags);
        for (String name : COLORS) word(row, Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        for (String name : BOUNDS) wide(row, field(uu.class,name).getDouble(block));
        for (String name : new String[]{"h","p","o"}) row.append(' ').append(field(nw.class,name).getInt(tessellator));
        word(row, field(nw.class,"k").getInt(tessellator));
        for (String name : new String[]{"i","j","s","t","u"}) wide(row, field(nw.class,name).getDouble(tessellator));
        for (String name : new String[]{"l","m","n","q","w"})
            row.append(' ').append(field(nw.class,name).getBoolean(tessellator) ? 1 : 0);
        row.append(' ').append(field(nw.class,"r").getInt(tessellator));
        row.append(' ').append(field(nw.class,"B").getInt(tessellator));
        int[] words = (int[])field(nw.class,"g").get(tessellator);
        int count = words == null ? -1 : Math.min(words.length,192);
        row.append(' ').append(exception).append(' ').append(count);
        if (words != null) for (int i=0;i<count;++i) word(row,words[i]);
        return row.toString();
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2,"original JAR and declared profiles");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath();
        java.util.List<String> profiles = Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) { ++network; throw new SecurityException("network denied"); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException("listen denied"); }
            @Override public void checkLink(String library) { ++nativeLinks; throw new SecurityException("native denied"); }
            @Override public void checkWrite(String path) { ++writes; throw new SecurityException("write denied"); }
            @Override public void checkDelete(String path) { ++writes; throw new SecurityException("delete denied"); }
        });
        for (Class<?> type : new Class<?>[]{cv.class,uu.class,nw.class}) original(type,jar);
        nw tessellator = nw.a;
        int[] originalWords = (int[])field(nw.class,"g").get(tessellator);
        int originalCapacity = field(nw.class,"B").getInt(tessellator);
        Method geometry = cv.class.getMethod("a",uu.class,int.class,double.class,double.class,double.class);
        int index = 0;
        for (String line : profiles) {
            if (line.isBlank()) continue;
            int[] p = Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();
            require(p.length == 5,"profile width");
            uu block = uu.m[p[0]];
            require(block != null,"actual registered Block");
            original(block.getClass(),jar);
            cv renderer = new cv();
            field(cv.class,"d").setInt(renderer,p[2]);
            for (int i=0;i<CORNERS.length;++i) field(cv.class,COLORS[i]).setFloat(renderer,Float.intBitsToFloat(CORNERS[i]));
            int kind = p[4], capacity = kind >= 2 ? CAPACITIES[kind-2] : originalCapacity;
            int[] words = kind == 1 ? null : kind >= 2 ? new int[capacity] : originalWords;
            if (words != null) Arrays.fill(words,0,Math.min(words.length,192),0x5a6b7c8d);
            field(nw.class,"g").set(tessellator,words);
            field(nw.class,"B").setInt(tessellator,capacity);
            field(nw.class,"w").setBoolean(tessellator,false);
            tessellator.b(16.0,-48.0,-32.0);
            tessellator.b();
            tessellator.a(31,127,233,255);
            double[] point = POINTS[p[3]];
            String exception = "NONE";
            try {
                geometry.invoke(renderer,kind == 0 ? null : block,p[1],point[0],point[1],point[2]);
            } catch (InvocationTargetException caught) {
                Throwable failure = caught.getCause();
                if (failure instanceof NullPointerException) exception = "NPE";
                else if (failure instanceof ArrayIndexOutOfBoundsException) exception = "AIOOBE";
                else throw caught;
            }
            require(exception.equals(kind < 0 ? "NONE" : kind < 2 ? "NPE" : "AIOOBE"),"source failure category");
            require(field(cv.class,"c").get(renderer) == null && field(cv.class,"d").getInt(renderer) == p[2]
                && field(cv.class,"b").getBoolean(renderer) && field(cv.class,"O").getInt(renderer) == 1,
                "retained actual CV scalar fields");
            System.out.println(observe(index++,p,renderer,block,tessellator,exception));
            field(nw.class,"g").set(tessellator,originalWords);
            field(nw.class,"B").setInt(tessellator,originalCapacity);
        }
        require(index == 125 && network == 0 && nativeLinks == 0 && writes == 0,"bounded scope and external effects");
        System.out.println("Original crossed plants: 125 rows; network=0; native=0; write=0; draw=0");
    }
}
