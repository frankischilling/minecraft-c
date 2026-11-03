import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.FloatBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.Pbuffer;
import org.lwjgl.opengl.PixelFormat;
import net.minecraft.client.Minecraft;

/** Declared inputs and observations around unchanged original CV inventory calls. */
public final class InventoryGeometryTarget {
    public static int active = -1;
    static int network;
    static final List<String> links = new ArrayList<>();
    static final int[] IDS = {81,53,67,85,81,53,67,85,81,53,67,85,81,53,67,85,53};
    static final int[] META = {0,1,3,15,-1,Integer.MIN_VALUE,Integer.MAX_VALUE,3,8,2,4,0,5,6,-7,9,0};
    static final float[] BRIGHTNESS = {1,.375f,.75f,.5f,.625f,1.25f,0,-.25f,.5f,.875f,.125f,.75f,1,-0.0f,.375f,1,1};
    static final boolean[] COLOR = {true,false,true,false,true,true,false,true,false,true,false,true,false,false,true,true,false};
    static final String[] BOUNDS = {"bs","bt","bu","bv","bw","bx"};
    static final String[] ROTATIONS = {"l","k","g","h","j","i"};
    static final String[] COLORS = {"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final float[] AO = {.125f,.25f,.5f,.75f,.875f,.625f,.375f,.0625f,1,.50000006f,-.25f,.3125f};

    static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name); result.setAccessible(true); return result;
    }
    static String bits(float value) { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value)); }
    static Object value(Object value) {
        if (value instanceof Float) return bits((Float)value);
        if (value instanceof Double) return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)value));
        return value;
    }
    static Map<String,Object> primitive(Class<?> type, Object receiver, boolean statics) throws Exception {
        Map<String,Object> out = new TreeMap<>();
        for (Field f : type.getDeclaredFields()) if (f.getType().isPrimitive() && Modifier.isStatic(f.getModifiers()) == statics) {
            f.setAccessible(true); out.put(f.getName(),value(f.get(receiver)));
        }
        return out;
    }
    static List<String> words() throws Exception {
        int[] raw = (int[])field(nw.class,"g").get(nw.a);
        List<String> result = new ArrayList<>();
        for (int i=0;i<48;++i) result.add(String.format(Locale.ROOT,"%08x",raw[i]));
        return result;
    }
    static List<Object> bounds(uu block) throws Exception {
        List<Object> result = new ArrayList<>();
        for (String name : BOUNDS) result.add(value(field(uu.class,name).get(block)));
        return result;
    }
    static List<String> gl(int parameter,int count) {
        FloatBuffer data = BufferUtils.createFloatBuffer(16); GL11.glGetFloat(parameter,data);
        List<String> result = new ArrayList<>();
        for (int i=0;i<count;++i) result.add(bits(data.get(i)));
        return result;
    }
    static Map<String,Object> deviceState() {
        return ConstructorJson.map("modelview",gl(GL11.GL_MODELVIEW_MATRIX,16),"projection",gl(GL11.GL_PROJECTION_MATRIX,16),"color",gl(GL11.GL_CURRENT_COLOR,4));
    }
    static Map<String,Object> sourceBlock(uu block) throws Exception {
        if (!(block instanceof ss)) return null;
        uu source = (uu)field(ss.class,"a").get(block);
        return source == null ? ConstructorJson.map("id",null,"class",null,"texture",null) :
            ConstructorJson.map("id",source.bn,"class",source.getClass().getName(),"texture",source.bm);
    }
    static void emit(String tag,Object data) { System.out.println("INVENTORY_GEOMETRY_"+tag+" "+ConstructorJson.json(data)); }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toAbsolutePath(), natives = Path.of(args[2]).toRealPath();
        String mode=args[3]; require(mode.equals("immediate") || mode.equals("compile"),"declared observer mode");
        Files.createDirectories(directory);
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host,int port) { ++network; throw new SecurityException("network"); }
            public void checkListen(int port) { ++network; throw new SecurityException("listener"); }
            public void checkLink(String path) {
                links.add(path);
                boolean lwjgl = Path.of(path).isAbsolute() && Path.of(path).normalize().equals(natives.resolve("lwjgl64.dll"));
                boolean jdk = Set.of("awt","awt_headless","fontmanager","freetype","lcms","zip","nio","net").contains(path);
                require(lwjgl || jdk,"unexpected native link "+path);
            }
        });
        for (Class<?> type : new Class<?>[]{kq.class,Minecraft.class,kv.class,ik.class,ji.class,sj.class,cv.class,nw.class,uu.class,ss.class,or.class,jw.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original JAR "+type.getName());
        kq app = new kq(null,null,null,854,480,false,null);
        Pbuffer context = null;
        try {
            require(field(Minecraft.class,"a").get(null)==app,"actual canonical App constructor");
            field(Minecraft.class,"Z").set(app,directory.toFile());
            context = new Pbuffer(32,32,new PixelFormat(),null,null); context.makeCurrent();
            emit("DEVICE",ConstructorJson.map("vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"hidden_pbuffer",true,"mode",mode));
            app.z = new kv();
            ik packs = new ik(app,directory.toFile()); ji engine = new ji(packs,app.z); app.p=engine;
            app.q = new sj(app.z,"/font/default.png",engine);
            int atlas = engine.b("/terrain.png"); GL11.glBindTexture(GL11.GL_TEXTURE_2D,atlas);
            require(ji.class.getResource("/terrain.png")!=null,"original terrain atlas");
            emit("OWNERS",ConstructorJson.map("app",app.getClass().getName(),"settings",app.z.getClass().getName(),"packs",packs.getClass().getName(),"engine",engine.getClass().getName(),"font",app.q.getClass().getName(),"nw",nw.a.getClass().getName(),"canonical_app",true,"raw_allocations",0,"world_constructed",false));
            emit("INITIAL",ConstructorJson.map("static",primitive(nw.class,null,true),"instance",primitive(nw.class,nw.a,false),"words",words()));
            for (int profile=0;profile<IDS.length;++profile) {
                uu block = uu.m[IDS[profile]]; require(block!=null,"actual registered Block");
                cv renderer = new cv(); renderer.b=COLOR[profile];
                if (profile>=4) block.a(0,0,0,1,1,1);
                if (profile==4 || profile==12) block.a(.125f,.25f,-.125f,.75f,.875f,1.125f);
                if (profile==5) uu.m[5].bm=83;
                if (profile==6) field(ss.class,"a").set(block,uu.m[81]);
                if (profile==7) block.bm=302;
                if (profile==10) field(ss.class,"a").set(block,uu.m[17]);
                if (profile==15) block.bm=-1;
                if (profile==16) field(ss.class,"a").set(block,null);
                if (profile==9 || profile==12 || profile==14) {
                    int[] rotations = {1,2,3,1,2,3};
                    for (int i=0;i<6;++i) field(cv.class,ROTATIONS[i]).setInt(renderer,rotations[i]);
                    field(cv.class,"e").setBoolean(renderer,true);
                }
                if (profile==10 || profile==12) field(cv.class,"d").setInt(renderer,profile==10?302:16);
                if (profile==11 || profile==12 || profile==13) {
                    field(cv.class,"m").setBoolean(renderer,true);
                    for (int i=0;i<COLORS.length;++i) field(cv.class,COLORS[i]).setFloat(renderer,AO[i]);
                }
                double x=0,y=0,z=0;
                if (profile==8) { x=16; y=-48; z=-32; }
                if (profile==12) { x=.5; y=.25; z=-.75; }
                if (profile==14) { x=16777216.0; y=-64; z=-256; }
                nw.a.b(x,y,z);
                GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
                GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity(); GL11.glTranslatef(.25f,-.125f,.5f);
                GL11.glColor4f(.125f,.25f,.5f,.75f);
                emit("INPUT",ConstructorJson.map("profile",profile,"id",IDS[profile],"class",block.getClass().getName(),"metadata",META[profile],"brightness",bits(BRIGHTNESS[profile]),"cv",primitive(cv.class,renderer,false),"bounds",bounds(block),"texture",block.bm,"source_block",sourceBlock(block),"nw_before",primitive(nw.class,nw.a,false),"prior_words",words(),"gl_before",deviceState()));
                Throwable failure = null;
                int list=0;
                if (mode.equals("compile")) { list=GL11.glGenLists(1);require(list!=0,"actual list allocation");GL11.glNewList(list,GL11.GL_COMPILE); }
                active=profile;
                try { renderer.a(block,META[profile],BRIGHTNESS[profile]); }
                catch (NullPointerException exception) { failure=exception; }
                finally { active=-1; }
                if (mode.equals("compile")) { GL11.glEndList();GL11.glDeleteLists(list,1); }
                require((profile==16)==(failure!=null),"actual expected complete/failure path "+profile);
                int glError=GL11.glGetError(); require(glError==0,"actual GL error "+glError);
                List<String> trace=new ArrayList<>();
                if (failure!=null) for (StackTraceElement frame:failure.getStackTrace()) trace.add(frame.getClassName()+"."+frame.getMethodName());
                emit("RETURN",ConstructorJson.map("profile",profile,"returned",failure==null,"exception",failure==null?null:failure.getClass().getName(),"exception_stack",trace,"cv",primitive(cv.class,renderer,false),"bounds",bounds(block),"source_block",sourceBlock(block),"nw_after",primitive(nw.class,nw.a,false),"last_words",words(),"gl_after",deviceState(),"gl_error",glError));
            }
            require(network==0,"no target network attempts");
            emit("COMPLETE",ConstructorJson.map("profiles",17,"returned",16,"failed",1,"network",network,"links",links,"game_substitution",false,"random_rebinding",false,"raw_allocations",0,"nw_backing_replaced",false,"nw_backing_cleared",false,"source_failure_retried",false));
        } finally {
            app.J=false;
            for (Thread thread:Thread.getAllStackTraces().keySet()) if (thread.getName().equals("Timer hack thread")) {
                thread.interrupt(); thread.join(2000); require(!thread.isAlive(),"Timer retired");
            }
            if (context!=null) context.destroy();
        }
    }
}
