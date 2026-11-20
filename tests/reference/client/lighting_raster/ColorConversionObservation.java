import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GLContext;
import org.lwjgl.opengl.PixelFormat;

/** Standalone actual color-attribute observations; no game classes or formulas. */
public final class ColorConversionObservation {
    static final int SENTINEL = 0x7fc12345;
    static int network, nativeAttempts, stateRows, decodeRows;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String bounded(String value) {
        require(value != null,"actual driver/link string");
        value = value.replace('\n',' ').replace('\r',' ');
        return value.substring(0,Math.min(value.length(),240));
    }
    static String words(FloatBuffer values, int count) {
        StringBuilder text = new StringBuilder();
        for (int index = 0; index < count; ++index) {
            if (index != 0) text.append(',');
            text.append(String.format("%08x",Float.floatToRawIntBits(values.get(index))));
        }
        return text.toString();
    }
    static String hash(ByteBuffer bytes) throws Exception {
        ByteBuffer view = bytes.duplicate(); view.clear();
        byte[] input = new byte[view.remaining()]; view.get(input);
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(input);
        StringBuilder text = new StringBuilder();
        for (byte value : digest) text.append(String.format("%02x",value & 255));
        return text.toString();
    }
    static ByteBuffer geometry() {
        ByteBuffer bytes = BufferUtils.createByteBuffer(96).order(ByteOrder.nativeOrder());
        float[][] positions = {{-.75f,-.75f,0},{.75f,-.75f,0},{-.75f,.75f,0}};
        for (int vertex = 0; vertex < 3; ++vertex)
            for (int component = 0; component < 3; ++component)
                bytes.putFloat(vertex*32+component*4,positions[vertex][component]);
        return bytes;
    }
    static void colorBytes(ByteBuffer bytes, int value) {
        for (int vertex = 0; vertex < 3; ++vertex)
            for (int component = 0; component < 4; ++component)
                bytes.put(vertex*32+20+component,(byte)value);
    }
    static void arrays(ByteBuffer bytes, boolean colors) {
        GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,0);
        GL11.glDisableClientState(GL11.GL_TEXTURE_COORD_ARRAY);
        GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY);
        GL11.glEnableClientState(GL11.GL_VERTEX_ARRAY);
        bytes.position(0); GL11.glVertexPointer(3,32,bytes.asFloatBuffer());
        bytes.position(20); GL11.glColorPointer(4,true,32,bytes); bytes.position(0);
        if (colors) GL11.glEnableClientState(GL11.GL_COLOR_ARRAY);
        else GL11.glDisableClientState(GL11.GL_COLOR_ARRAY);
    }
    static String layout() {
        int type = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_TYPE);
        int size = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_SIZE);
        int stride = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_STRIDE);
        int buffer = GL11.glGetInteger(GL15.GL_COLOR_ARRAY_BUFFER_BINDING);
        int vertexType = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_TYPE);
        int vertexSize = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_SIZE);
        int vertexStride = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_STRIDE);
        int vertexBuffer = GL11.glGetInteger(GL15.GL_VERTEX_ARRAY_BUFFER_BINDING);
        require(type == GL11.GL_UNSIGNED_BYTE && size == 4 && stride == 32 && buffer == 0 &&
                vertexType == GL11.GL_FLOAT && vertexSize == 3 && vertexStride == 32 && vertexBuffer == 0,
                "actual client RGBA unsigned-byte and float-position layout");
        return "COLOR_LAYOUT color_type=" + type + " color_size=" + size + " color_stride=" + stride +
            " color_buffer=" + buffer + " vertex_type=" + vertexType + " vertex_size=" + vertexSize +
            " vertex_stride=" + vertexStride + " vertex_buffer=" + vertexBuffer;
    }
    static String current(int parameter, int count) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(parameter,values); return words(values,count);
    }
    static String material(int face, int parameter) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetMaterial(face,parameter,values); return words(values,4);
    }
    static void state(int value, String label) {
        System.out.println("COLOR_STATE " + value + " " + label + " " + current(GL11.GL_CURRENT_NORMAL,3) +
            " " + current(GL11.GL_CURRENT_COLOR,4) + " " + material(GL11.GL_FRONT,GL11.GL_AMBIENT) +
            " " + material(GL11.GL_FRONT,GL11.GL_DIFFUSE) + " " + material(GL11.GL_BACK,GL11.GL_AMBIENT) +
            " " + material(GL11.GL_BACK,GL11.GL_DIFFUSE) + " " + (GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0) +
            " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE) + " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER));
        ++stateRows;
    }
    static int shader(int type, String source) {
        int result = GL20.glCreateShader(type); GL20.glShaderSource(result,source); GL20.glCompileShader(result);
        require(GL20.glGetShaderi(result,GL20.GL_COMPILE_STATUS) != 0,GL20.glGetShaderInfoLog(result,4096));
        return result;
    }
    static String feedback(int program, int buffer, int query, int list) {
        FloatBuffer initial = BufferUtils.createFloatBuffer(16);
        for (int index = 0; index < 16; ++index) initial.put(Float.intBitsToFloat(SENTINEL));
        initial.flip();
        GL15.glBindBuffer(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
        GL15.glBufferData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,initial,GL15.GL_STREAM_READ);
        GL30.glBindBufferBase(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
        GL20.glUseProgram(program); GL11.glEnable(GL30.GL_RASTERIZER_DISCARD);
        GL15.glBeginQuery(GL30.GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,query);
        GL30.glBeginTransformFeedback(GL11.GL_TRIANGLES);
        if (list == 0) GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3); else GL11.glCallList(list);
        GL30.glEndTransformFeedback(); GL15.glEndQuery(GL30.GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        GL11.glDisable(GL30.GL_RASTERIZER_DISCARD); GL20.glUseProgram(0);
        require(GL15.glGetQueryObjecti(query,GL15.GL_QUERY_RESULT) == 1,"one actual feedback triangle");
        FloatBuffer result = BufferUtils.createFloatBuffer(16);
        GL15.glGetBufferSubData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,result);
        for (int index = 0; index < 12; ++index)
            require(Float.isFinite(result.get(index)),"all twelve color components written");
        for (int index = 12; index < 16; ++index)
            require(Float.floatToRawIntBits(result.get(index)) == SENTINEL,"four feedback suffix words unchanged");
        return words(result,12);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2,"native directory and exact native filename");
        Locale.setDefault(Locale.ROOT); Path natives = Path.of(args[0]).toRealPath();
        require(args[1].equals("lwjgl64.dll") || args[1].equals("liblwjgl64.so"),"declared native platform");
        Path allowed = natives.resolve(args[1]);
        Map<String,Path> jdkNatives = new LinkedHashMap<>();
        if (args[1].equals("liblwjgl64.so")) {
            Path root = Path.of(System.getProperty("java.home"),"lib").toRealPath();
            for (String name : new String[]{"awt","awt_xawt","awt_headless","jawt","fontmanager"}) {
                Path path = root.resolve(System.mapLibraryName(name)).toRealPath();
                require(path.getParent().equals(root),"exact JDK native remains within java.home/lib");
                jdkNatives.put(name,path);
            }
        }
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                System.out.println("NATIVE_LINK_ATTEMPT " + bounded(library));
                require(++nativeAttempts <= 32,"bounded native-link attempt inventory");
                Path requested = Path.of(library);
                if (requested.isAbsolute() && requested.normalize().equals(allowed)) { links.add(library); return; }
                for (Map.Entry<String,Path> entry : jdkNatives.entrySet())
                    if (library.equals(entry.getKey()) || requested.isAbsolute() && requested.normalize().equals(entry.getValue())) {
                        System.out.println("JDK_NATIVE_LINK " + entry.getKey() + " " + entry.getValue()); return;
                    }
                throw new SecurityException("native outside exact LWJGL/JDK allowlist: " + bounded(library));
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Actual color attribute conversion");
        int vertex = 0, fragment = 0, program = 0, buffer = 0, query = 0, lists = 0;
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            require(GLContext.getCapabilities().OpenGL30,"actual transform-feedback support");
            System.out.println("COLOR_DRIVER " + bounded(GL11.glGetString(GL11.GL_VENDOR)) + " | " +
                bounded(GL11.glGetString(GL11.GL_RENDERER)) + " | " + bounded(GL11.glGetString(GL11.GL_VERSION)));
            for (int channel : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8,"actual RGBA8 context format");
            System.out.println("COLOR_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                GL11.glGetInteger(GL11.GL_STENCIL_BITS));
            GL11.glViewport(0,0,64,64);
            GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
            GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
            GL11.glEnable(GL11.GL_LIGHTING); GL11.glEnable(GL11.GL_LIGHT0); GL11.glEnable(GL11.GL_LIGHT1);
            GL11.glDisable(GL11.GL_NORMALIZE); GL11.glDisable(32826);
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_AMBIENT_AND_DIFFUSE);
            GL11.glEnable(GL11.GL_COLOR_MATERIAL);
            System.out.println("COLOR_CLAMP vertex=" + GL11.glGetInteger(GL30.GL_CLAMP_VERTEX_COLOR) +
                " fragment=" + GL11.glGetInteger(GL30.GL_CLAMP_FRAGMENT_COLOR) +
                " read=" + GL11.glGetInteger(GL30.GL_CLAMP_READ_COLOR));
            System.out.println("COLOR_STATE_POLICY after_direct=unspecified_observation no_state_TF_parity_assumed=1");
            vertex = shader(GL20.GL_VERTEX_SHADER,
                "#version 120\nvarying vec4 capturedColor;void main(){capturedColor=gl_Color;gl_Position=gl_Vertex;}\n");
            fragment = shader(GL20.GL_FRAGMENT_SHADER,"#version 120\nvoid main(){gl_FragColor=vec4(1.0);}\n");
            program = GL20.glCreateProgram(); GL20.glAttachShader(program,vertex); GL20.glAttachShader(program,fragment);
            GL30.glTransformFeedbackVaryings(program,new CharSequence[]{"capturedColor"},GL30.GL_INTERLEAVED_ATTRIBS);
            GL20.glLinkProgram(program);
            require(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS) != 0,GL20.glGetProgramInfoLog(program,4096));
            require(GL20.glGetProgrami(program,GL30.GL_TRANSFORM_FEEDBACK_VARYINGS) == 1 &&
                    GL20.glGetProgrami(program,GL30.GL_TRANSFORM_FEEDBACK_BUFFER_MODE) == GL30.GL_INTERLEAVED_ATTRIBS,
                    "actual one interleaved vec4 transform-feedback varying");
            buffer = GL15.glGenBuffers(); query = GL15.glGenQueries(); lists = GL11.glGenLists(256);
            require(buffer != 0 && query != 0 && lists != 0,"actual feedback/query/list allocations");
            ByteBuffer bytes = geometry();
            for (int value = 0; value < 256; ++value) {
                int list = lists+value; colorBytes(bytes,value); arrays(bytes,true);
                if (value == 0) System.out.println(layout()); else layout();
                String sourceHash = hash(bytes);
                GL11.glColor4f(.75f,.5f,.25f,.625f); GL11.glNormal3f(.25f,-.5f,.75f);
                state(value,"before_direct"); String direct = feedback(program,buffer,query,0);
                state(value,"after_direct");
                require(hash(bytes).equals(sourceHash),"direct feedback preserves source storage");
                GL11.glColor4f(.75f,.5f,.25f,.625f); GL11.glNormal3f(.25f,-.5f,.75f);
                state(value,"before_compile"); GL20.glUseProgram(0);
                GL11.glNewList(list,GL11.GL_COMPILE);
                int mode = GL11.glGetInteger(GL11.GL_LIST_MODE), compilingProgram = GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM);
                require(mode == GL11.GL_COMPILE && compilingProgram == 0,"actual fixed-function compile-only list");
                GL11.glColor4f(.25f,.5f,.75f,.375f); GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3); GL11.glEndList();
                require(GL11.glIsList(list) && hash(bytes).equals(sourceHash),"defined list preserves source storage");
                if (value == 0) System.out.println("COLOR_CAPTURE mode=" + mode + " compile_program=" + compilingProgram +
                    " draw_mode=4 vertices=3 feedback_components=12 sentinel_components=4 lighting=" +
                    (GL11.glIsEnabled(GL11.GL_LIGHTING) ? 1 : 0) + " color_material=" +
                    (GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0) + " normalize=" +
                    (GL11.glIsEnabled(GL11.GL_NORMALIZE) ? 1 : 0) + " rescale=" + (GL11.glIsEnabled(32826) ? 1 : 0));
                state(value,"after_compile");
                colorBytes(bytes,value ^ 255); String poisonA = hash(bytes); arrays(bytes,false);
                GL11.glColor4f(.125f,.25f,.5f,.875f); GL11.glNormal3f(123,456,789);
                state(value,"before_call"); String compiled = feedback(program,buffer,query,list); state(value,"after_call");
                colorBytes(bytes,value ^ 0x55); String poisonB = hash(bytes); arrays(bytes,true);
                GL11.glColor4f(.875f,.625f,.375f,.125f); GL11.glNormal3f(-123,-456,-789);
                state(value,"before_repeat"); String repeated = feedback(program,buffer,query,list); state(value,"after_repeat");
                System.out.println("COLOR_STORAGE " + value + " " + sourceHash + " " + poisonA + " " + poisonB);
                System.out.println("COLOR_DECODE " + value + " " + direct + " " + compiled + " " + repeated); ++decodeRows;
                require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual current/array/list color observations completed");
            }
            require(decodeRows == 256 && stateRows == 2048,"complete conversion/current-state inventory");
            require(network == 0,"no network/listener attempts");
        } finally {
            GL20.glUseProgram(0);
            if (lists != 0) GL11.glDeleteLists(lists,256);
            if (query != 0) GL15.glDeleteQueries(query);
            if (buffer != 0) GL15.glDeleteBuffers(buffer);
            if (program != 0) GL20.glDeleteProgram(program);
            if (vertex != 0) GL20.glDeleteShader(vertex); if (fragment != 0) GL20.glDeleteShader(fragment);
            require(GL11.glGetError() == GL11.GL_NO_ERROR,"checked native cleanup"); Display.destroy();
        }
        System.out.println("Actual color conversion: 256 decode; 256 storage; 2048 state rows; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
