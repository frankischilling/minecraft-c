import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Genuine NW methods; separately authored DK matrix-call sequence on real GL. */
public final class CompiledMatrixObservation {
    static final List<String> nativeLinks = new ArrayList<>();
    static int network;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(String name) throws Exception {
        Field result = nw.class.getDeclaredField(name); result.setAccessible(true); return result;
    }
    static void offsets() throws Exception {
        nw tessellator = nw.a;
        long[][] vectors = {
            {0L,0x8000000000000000L,1L},
            {0x8000000000000001L,0x0010000000000000L,0x8010000000000000L},
            {0x7ff0000000000000L,0xfff0000000000000L,0x7ff8000000001234L},
            {0xfff8000000005678L,0x7ff0000000001234L,0xfff0000000005678L},
            {0x4170000000000000L,0xc050200000000000L,0xc070000000000000L}};
        int row = 0;
        for (long[] vector : vectors) {
            tessellator.b(Double.longBitsToDouble(vector[0]),Double.longBitsToDouble(vector[1]),
                          Double.longBitsToDouble(vector[2]));
            System.out.printf("NW_TRANSLATION %d",row++);
            for (String name : new String[]{"s","t","u"})
                System.out.printf(" %016x",Double.doubleToRawLongBits(field(name).getDouble(tessellator)));
            System.out.printf(" %d %d %d %d %d %d %d %d %d %d%n",
                field("w").getBoolean(tessellator)?1:0,field("h").getInt(tessellator),
                field("o").getInt(tessellator),field("p").getInt(tessellator),
                field("l").getBoolean(tessellator)?1:0,field("m").getBoolean(tessellator)?1:0,
                field("n").getBoolean(tessellator)?1:0,
                ((ByteBuffer)field("d").get(tessellator)).position(),
                ((IntBuffer)field("e").get(tessellator)).position(),
                ((FloatBuffer)field("f").get(tessellator)).position());
        }
        tessellator.b();
        System.out.printf("NW_BEGIN %016x %016x %016x%n",
            Double.doubleToRawLongBits(field("s").getDouble(tessellator)),
            Double.doubleToRawLongBits(field("t").getDouble(tessellator)),
            Double.doubleToRawLongBits(field("u").getDouble(tessellator)));
        tessellator.a(-16777215.75,65.0,256.75);
        int[] packed=(int[])field("g").get(tessellator);
        System.out.printf("NW_VERTEX %08x %08x %08x %d %d %d%n",packed[0],packed[1],packed[2],
            field("h").getInt(tessellator),field("o").getInt(tessellator),field("p").getInt(tessellator));
        tessellator.b(0.0,0.0,0.0);
        // No nonempty draw, native list, prepared source field or fake GL call.
    }
    static int[] matrix(int mode) {
        FloatBuffer values=BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(mode==GL11.GL_MODELVIEW?GL11.GL_MODELVIEW_MATRIX:GL11.GL_PROJECTION_MATRIX,values);
        int[] bits=new int[16];
        for(int i=0;i<16;++i) bits[i]=Float.floatToRawIntBits(values.get(i));
        return bits;
    }
    static void prefix() {
        // Authored operands from DK.a264..332, with DK.g's local origin zero.
        GL11.glPushMatrix(); GL11.glTranslatef(0,0,0);
        GL11.glTranslatef(-8,-8,-8); GL11.glScalef(1.000001f,1.000001f,1.000001f);
        GL11.glTranslatef(8,8,8);
    }
    static void matrices() {
        int base=GL11.glGenLists(2);
        require(base!=0,"original GL list allocation failed");
        for(int mode : new int[]{GL11.GL_MODELVIEW,GL11.GL_PROJECTION}) {
            GL11.glMatrixMode(mode); GL11.glLoadIdentity(); GL11.glTranslatef(3,-5,7);
            int[] before=matrix(mode);
            prefix(); int[] immediate=matrix(mode); GL11.glPopMatrix();
            GL11.glNewList(base,GL11.GL_COMPILE); prefix(); GL11.glEndList();
            require(java.util.Arrays.equals(before,matrix(mode)),"GL_COMPILE changed current matrix");
            GL11.glNewList(base+1,GL11.GL_COMPILE); GL11.glPopMatrix(); GL11.glEndList();
            GL11.glCallList(base); int[] compiled=matrix(mode);
            require(java.util.Arrays.equals(immediate,compiled),"compiled/immediate GL matrix differs");
            System.out.printf("DK_SEQUENCE %d",mode);
            for(int value:compiled) System.out.printf(" %08x",value);
            System.out.println();
            GL11.glCallList(base+1);
            require(java.util.Arrays.equals(before,matrix(mode)),"compiled Pop did not restore matrix");
        }
        GL11.glDeleteLists(base,2);
        require(GL11.glGetError()==GL11.GL_NO_ERROR,"matrix sequence GL error");
    }
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(arguments[0]).toRealPath(),natives=Path.of(arguments[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) {
                ++network; throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("observer denies listeners");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
            @Override public void checkLink(String library) {
                nativeLinks.add(library);
                if(!Path.of(library).isAbsolute() ||
                   !Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native link "+library);
            }
        });
        for(Class<?> type:new Class<?>[]{nw.class,ge.class,dk.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar),"foreign original class");
        offsets();
        require(nativeLinks.isEmpty(),"CPU NW methods linked a native library");
        Display.setDisplayMode(new DisplayMode(64,64));
        Display.setTitle("Original compiled matrix controls");
        try {
            Display.create();
            System.out.println("MATRIX_DRIVER "+GL11.glGetString(GL11.GL_VENDOR)+" | "+
                GL11.glGetString(GL11.GL_RENDERER)+" | "+GL11.glGetString(GL11.GL_VERSION));
            matrices();
        } finally { Display.destroy(); }
        for(String nativeLink:nativeLinks) System.out.println("NATIVE_LINK "+nativeLink);
        require(network==0,"observer attempted network");
        System.out.println("Original NW translation: 5 rows; matrix sequence: 2 profiles; network=0; display=1");
    }
}
