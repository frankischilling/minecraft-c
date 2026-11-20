import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.PixelFormat;

/** Separate driver diagnostic: current/array/list normal conversion, without game execution. */
public final class NormalListConversionReceiptObservation {
    static final int SENTINEL = 0x7fc12345;
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String words(FloatBuffer values, int count) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < count; ++i) {
            if (i != 0) text.append(',');
            text.append(String.format("%08x", Float.floatToRawIntBits(values.get(i))));
        }
        return text.toString();
    }
    static String hash(ByteBuffer bytes) throws Exception {
        ByteBuffer view = bytes.duplicate(); view.clear();
        byte[] input = new byte[view.remaining()]; view.get(input);
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(input);
        StringBuilder text = new StringBuilder();
        for (byte value : digest) text.append(String.format("%02x", value & 255));
        return text.toString();
    }
    static int shader(int type, String source) {
        int value = GL20.glCreateShader(type); GL20.glShaderSource(value,source); GL20.glCompileShader(value);
        require(GL20.glGetShaderi(value,GL20.GL_COMPILE_STATUS) != 0,
                GL20.glGetShaderInfoLog(value,4096));
        return value;
    }
    static ByteBuffer geometry() {
        ByteBuffer bytes = BufferUtils.createByteBuffer(96).order(ByteOrder.nativeOrder());
        float[][] positions = {{-.75f,-.75f,0},{.75f,-.75f,0},{-.75f,.75f,0}};
        for (int vertex = 0; vertex < 3; ++vertex)
            for (int component = 0; component < 3; ++component)
                bytes.putFloat(vertex * 32 + component * 4,positions[vertex][component]);
        return bytes;
    }
    static void normalBytes(ByteBuffer bytes, int number) {
        for (int vertex = 0; vertex < 3; ++vertex)
            for (int component = 0; component < 3; ++component)
                bytes.put(vertex * 32 + 24 + component,(byte)number);
    }
    static void arrays(ByteBuffer bytes, boolean normals) {
        GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,0);
        GL11.glDisableClientState(GL11.GL_COLOR_ARRAY);
        GL11.glDisableClientState(GL11.GL_TEXTURE_COORD_ARRAY);
        GL11.glEnableClientState(GL11.GL_VERTEX_ARRAY);
        bytes.position(0); GL11.glVertexPointer(3,32,bytes.asFloatBuffer());
        if (normals) {
            bytes.position(24); GL11.glNormalPointer(32,bytes);
            GL11.glEnableClientState(GL11.GL_NORMAL_ARRAY);
        } else GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY);
        bytes.position(0);
    }
    static String layout() {
        int normalType = GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_TYPE);
        int normalStride = GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_STRIDE);
        int normalBuffer = GL11.glGetInteger(GL15.GL_NORMAL_ARRAY_BUFFER_BINDING);
        int vertexType = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_TYPE);
        int vertexSize = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_SIZE);
        int vertexStride = GL11.glGetInteger(GL11.GL_VERTEX_ARRAY_STRIDE);
        int vertexBuffer = GL11.glGetInteger(GL15.GL_VERTEX_ARRAY_BUFFER_BINDING);
        require(normalType == GL11.GL_BYTE && normalStride == 32 && normalBuffer == 0 &&
                vertexType == GL11.GL_FLOAT && vertexSize == 3 && vertexStride == 32 && vertexBuffer == 0,
                "actual client GL_BYTE/GL_FLOAT source array layout");
        return "NORMAL_LIST_LAYOUT normal_type=" + normalType + " normal_stride=" + normalStride +
            " normal_buffer=" + normalBuffer + " vertex_type=" + vertexType + " vertex_size=" + vertexSize +
            " vertex_stride=" + vertexStride + " vertex_buffer=" + vertexBuffer;
    }
    static FloatBuffer feedback(int program, int buffer, int query, int list) {
        FloatBuffer initial = BufferUtils.createFloatBuffer(16);
        for (int i = 0; i < 16; ++i) initial.put(i,Float.intBitsToFloat(SENTINEL));
        GL15.glBindBuffer(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
        GL15.glBufferData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,initial,GL15.GL_STREAM_READ);
        GL30.glBindBufferBase(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
        GL20.glUseProgram(program); GL11.glEnable(GL30.GL_RASTERIZER_DISCARD);
        GL15.glBeginQuery(GL30.GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,query);
        GL30.glBeginTransformFeedback(GL11.GL_TRIANGLES);
        if (list == 0) GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3); else GL11.glCallList(list);
        GL30.glEndTransformFeedback();
        GL15.glEndQuery(GL30.GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        GL11.glDisable(GL30.GL_RASTERIZER_DISCARD); GL20.glUseProgram(0);
        require(GL15.glGetQueryObjecti(query,GL15.GL_QUERY_RESULT) == 1,
                "one actual triangle written by transform feedback");
        FloatBuffer result = BufferUtils.createFloatBuffer(16);
        GL15.glGetBufferSubData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,result);
        for (int i = 0; i < 9; ++i)
            require(Float.isFinite(result.get(i)),"all nine normal components written");
        for (int i = 9; i < 16; ++i)
            require(Float.floatToRawIntBits(result.get(i)) == SENTINEL,
                    "all seven feedback suffix components preserved");
        return result;
    }
    static String current(int parameter, int count) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(parameter,values); return words(values,count);
    }
    static String material(int face, int parameter) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetMaterial(face,parameter,values); return words(values,4);
    }
    static void state(String label) {
        System.out.println("NORMAL_LIST_STATE " + label + " " + current(GL11.GL_CURRENT_NORMAL,3) + " " +
            current(GL11.GL_CURRENT_COLOR,4) + " " + material(GL11.GL_FRONT,GL11.GL_AMBIENT) + " " +
            material(GL11.GL_FRONT,GL11.GL_DIFFUSE) + " " + material(GL11.GL_BACK,GL11.GL_AMBIENT) + " " +
            material(GL11.GL_BACK,GL11.GL_DIFFUSE) + " " + (GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0) +
            " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE) + " " +
            GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER));
    }
    static void stateControl(int list) throws Exception {
        GL20.glUseProgram(0);
        ByteBuffer bytes = geometry();
        int[][] normals = {{0,0,127},{64,0,64},{0,127,0}};
        int[][] colors = {{128,64,32,255},{32,192,64,160},{64,32,224,96}};
        for (int vertex = 0; vertex < 3; ++vertex) {
            for (int component = 0; component < 3; ++component)
                bytes.put(vertex * 32 + 24 + component,(byte)normals[vertex][component]);
            for (int component = 0; component < 4; ++component)
                bytes.put(vertex * 32 + 20 + component,(byte)colors[vertex][component]);
        }
        arrays(bytes,true);
        bytes.position(20); GL11.glColorPointer(4,true,32,bytes); bytes.position(0);
        GL11.glEnableClientState(GL11.GL_COLOR_ARRAY);
        int colorType = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_TYPE);
        int colorSize = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_SIZE);
        int colorStride = GL11.glGetInteger(GL11.GL_COLOR_ARRAY_STRIDE);
        int colorBuffer = GL11.glGetInteger(GL15.GL_COLOR_ARRAY_BUFFER_BINDING);
        require(colorType == GL11.GL_UNSIGNED_BYTE && colorSize == 4 && colorStride == 32 && colorBuffer == 0,
                "actual client RGBA byte array layout");
        System.out.println("NORMAL_LIST_STATE_LAYOUT color_type=" + colorType + " color_size=" + colorSize +
            " color_stride=" + colorStride + " color_buffer=" + colorBuffer + " " +
            layout().substring("NORMAL_LIST_LAYOUT ".length()));
        GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_AMBIENT_AND_DIFFUSE);
        GL11.glEnable(GL11.GL_COLOR_MATERIAL);
        GL11.glColor4f(.75f,.5f,.25f,.625f); GL11.glNormal3f(.25f,-.5f,.75f);
        String sourceHash = hash(bytes);
        state("before_compile");
        GL11.glNewList(list,GL11.GL_COMPILE);
        GL11.glColor4f(.75f,.5f,.25f,.625f);
        GL11.glPushMatrix(); GL11.glScaled(1,1,.5); GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
        GL11.glPopMatrix(); GL11.glEndList();
        require(GL11.glIsList(list),"actual defined state-control list");
        require(hash(bytes).equals(sourceHash),"compilation does not modify client input storage");
        state("after_compile");
        for (int index = 0; index < bytes.capacity(); ++index) bytes.put(index,(byte)0);
        String poisonHash = hash(bytes);
        System.out.println("NORMAL_LIST_STATE_STORAGE " + sourceHash + " " + poisonHash);
        GL11.glColor4f(.125f,.25f,.5f,.875f); GL11.glNormal3f(-.25f,.75f,.5f);
        state("before_call"); GL11.glCallList(list); state("after_call");
        GL11.glColor4f(.875f,.625f,.375f,.125f); GL11.glNormal3f(.5f,.25f,-.75f);
        state("before_repeat"); GL11.glCallList(list); state("after_repeat");
        require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual fixed-function list/current/material controls completed");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 1,"native directory argument");
        Locale.setDefault(Locale.ROOT);
        Path natives = Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Legacy normal list conversion");
        int vertex = 0, fragment = 0, program = 0, buffer = 0, query = 0, lists = 0;
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("NORMAL_LIST_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            for (int channel : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8,"actual RGBA8 context format");
            System.out.println("NORMAL_LIST_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                               GL11.glGetInteger(GL11.GL_STENCIL_BITS));
            GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
            GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
            GL11.glEnable(GL11.GL_LIGHTING); GL11.glEnable(GL11.GL_LIGHT0); GL11.glEnable(GL11.GL_LIGHT1);
            GL11.glDisable(GL11.GL_NORMALIZE);
            GL11.glDisable(32826); GL11.glDisable(GL11.GL_COLOR_MATERIAL);
            vertex = shader(GL20.GL_VERTEX_SHADER,
                "#version 120\nvarying vec3 capturedNormal; void main(){capturedNormal=gl_Normal;gl_Position=gl_Vertex;}\n");
            fragment = shader(GL20.GL_FRAGMENT_SHADER,
                "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}\n");
            program = GL20.glCreateProgram(); GL20.glAttachShader(program,vertex); GL20.glAttachShader(program,fragment);
            GL30.glTransformFeedbackVaryings(program,new CharSequence[]{"capturedNormal"},GL30.GL_INTERLEAVED_ATTRIBS);
            GL20.glLinkProgram(program);
            require(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS) != 0,GL20.glGetProgramInfoLog(program,4096));
            require(GL20.glGetProgrami(program,GL30.GL_TRANSFORM_FEEDBACK_VARYINGS) == 1 &&
                    GL20.glGetProgrami(program,GL30.GL_TRANSFORM_FEEDBACK_BUFFER_MODE) == GL30.GL_INTERLEAVED_ATTRIBS,
                    "actual single interleaved vec3 feedback varying");
            buffer = GL15.glGenBuffers(); query = GL15.glGenQueries(); lists = GL11.glGenLists(257);
            require(buffer != 0 && query != 0 && lists != 0,"actual feedback/query/list names");
            ByteBuffer bytes = geometry();
            for (int number = -128; number <= 127; ++number) {
                int list = lists + number + 128;
                normalBytes(bytes,number); String sourceHash = hash(bytes);
                arrays(bytes,true); String actualLayout = layout();
                if (number == -128) System.out.println(actualLayout);
                FloatBuffer array = feedback(program,buffer,query,0);
                GL11.glNormal3b((byte)number,(byte)number,(byte)number);
                FloatBuffer current = BufferUtils.createFloatBuffer(16);
                GL11.glGetFloat(GL11.GL_CURRENT_NORMAL,current);
                arrays(bytes,false);
                FloatBuffer currentFeedback = feedback(program,buffer,query,0);
                arrays(bytes,true); GL20.glUseProgram(0);
                GL11.glNewList(list,GL11.GL_COMPILE);
                int compileMode = GL11.glGetInteger(GL11.GL_LIST_MODE);
                int compileProgram = GL11.glGetInteger(GL20.GL_CURRENT_PROGRAM);
                require(compileMode == GL11.GL_COMPILE && compileProgram == 0,
                        "actual GL_COMPILE DrawArrays under fixed-function program");
                GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3); GL11.glEndList();
                require(GL11.glIsList(list),"actual defined compiled array list");
                if (number == -128) System.out.println("NORMAL_LIST_CAPTURE mode=" + compileMode +
                    " compile_program=" + compileProgram + " draw_mode=" + GL11.GL_TRIANGLES +
                    " vertices=3 feedback_components=9 sentinel_components=7 lighting=" +
                    (GL11.glIsEnabled(GL11.GL_LIGHTING) ? 1 : 0) + " normalize=" +
                    (GL11.glIsEnabled(GL11.GL_NORMALIZE) ? 1 : 0) + " rescale=" +
                    (GL11.glIsEnabled(32826) ? 1 : 0));
                normalBytes(bytes,~number); String poisonAHash = hash(bytes);
                GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY); GL11.glNormal3f(123,456,789);
                FloatBuffer compiled = feedback(program,buffer,query,list);
                normalBytes(bytes,number ^ 0x55); String poisonBHash = hash(bytes);
                arrays(bytes,true); GL11.glNormal3f(-123,-456,-789);
                FloatBuffer compiledAgain = feedback(program,buffer,query,list);
                require(words(compiled,9).equals(words(compiledAgain,9)),
                        "compiled normal attributes survive both changed client storage/current normal/array state");
                System.out.println("NORMAL_LIST_STORAGE " + number + " " + sourceHash + " " + poisonAHash + " " + poisonBHash);
                System.out.println("NORMAL_LIST_DECODE " + number + " " + words(current,3) + " " +
                    words(currentFeedback,9) + " " + words(array,9) + " " + words(compiled,9) + " " + words(compiledAgain,9));
                require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual current/array/list feedback completed");
            }
            stateControl(lists + 256);
            require(network == 0,"no network/listener attempts");
        } finally {
            GL20.glUseProgram(0);
            if (lists != 0) GL11.glDeleteLists(lists,257);
            if (query != 0) GL15.glDeleteQueries(query);
            if (buffer != 0) GL15.glDeleteBuffers(buffer);
            if (program != 0) GL20.glDeleteProgram(program);
            if (vertex != 0) GL20.glDeleteShader(vertex);
            if (fragment != 0) GL20.glDeleteShader(fragment);
            require(GL11.glGetError() == GL11.GL_NO_ERROR,"checked native cleanup");
            Display.destroy();
        }
        System.out.println("Original normal list conversion: 256 values; 6 state receipts; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
