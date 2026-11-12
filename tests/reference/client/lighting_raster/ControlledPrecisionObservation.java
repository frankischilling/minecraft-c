import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.ContextAttribs;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL12;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL20;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GL32;
import org.lwjgl.opengl.GLContext;
import org.lwjgl.opengl.PixelFormat;

/** Authored driver precision controls. No game classes or assets are used. */
public final class ControlledPrecisionObservation {
    static int network, draws, samples, nativeAttempts;
    static final List<String> links = new ArrayList<>();
    static final int[][] POINTS = {{8,8},{16,16},{24,24}};
    static final String[] PIPES = {"clear", "fragment120", "fragment330",
        "fixed_current_smooth", "fixed_current_flat", "fixed_array_smooth", "fixed_array_flat",
        "generic120_smooth", "generic120_flat", "generic330_smooth", "generic330_flat",
        "lit_smooth", "lit_flat"};
    static final int[] programs = new int[9];
    static final int[] shaders = new int[18];
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String bit(float value) {
        return String.format("%08x", Float.floatToRawIntBits(value));
    }
    static String words(FloatBuffer values, int count) {
        StringBuilder text = new StringBuilder();
        for (int index = 0; index < count; ++index) {
            if (index != 0) text.append(',');
            text.append(bit(values.get(index)));
        }
        return text.toString();
    }
    static FloatBuffer vector(float... values) {
        FloatBuffer result = BufferUtils.createFloatBuffer(values.length);
        result.put(values).flip(); return result;
    }
    static String bounded(String text) {
        require(text != null, "actual driver string");
        return text.replace('\n',' ').replace('\r',' ').substring(0,Math.min(text.length(),240));
    }
    static int shader(int type, String source) {
        int result = GL20.glCreateShader(type);
        GL20.glShaderSource(result,source); GL20.glCompileShader(result);
        require(GL20.glGetShaderi(result,GL20.GL_COMPILE_STATUS) != 0,
                "shader compile: " + GL20.glGetShaderInfoLog(result,4096));
        return result;
    }
    static void program(int index, boolean modern, boolean flat, boolean direct) {
        String header = modern ? "#version 330 compatibility\n" : "#version 120\n";
        if (flat && !modern) header += "#extension GL_EXT_gpu_shader4 : require\n";
        String attribute = modern ? "in " : "attribute ";
        String vertexVarying = modern ? "out " : "varying ";
        String fragmentVarying = modern ? "in " : "varying ";
        String qualifier = flat ? "flat " : "";
        String vertex = header + attribute + "vec3 inPosition;\n";
        if (!direct) vertex += attribute + "vec4 inColor;\n" + qualifier + vertexVarying + "vec4 shadeColor;\n";
        vertex += "void main(){gl_Position=vec4(inPosition,1.0);" +
            (direct ? "" : "shadeColor=inColor;") + "}\n";
        boolean mrt = index >= 6;
        String fragment = header + (modern ? (mrt ?
            "layout(location=0) out vec4 pixelColor;\nlayout(location=1) out vec4 floatColor;\n" :
            "out vec4 pixelColor;\n") : "") +
            (direct ? "uniform vec4 constantColor;\n" : qualifier + fragmentVarying + "vec4 shadeColor;\n") +
            "void main(){" + (modern ? "pixelColor" : "gl_FragColor") + "=" +
            (direct ? "constantColor" : "shadeColor") + ";" +
            (mrt ? "floatColor=pixelColor;" : "") + "}\n";
        shaders[index*2] = shader(GL20.GL_VERTEX_SHADER,vertex);
        shaders[index*2+1] = shader(GL20.GL_FRAGMENT_SHADER,fragment);
        int result = GL20.glCreateProgram(); programs[index] = result;
        GL20.glAttachShader(result,shaders[index*2]); GL20.glAttachShader(result,shaders[index*2+1]);
        GL20.glBindAttribLocation(result,0,"inPosition");
        GL20.glBindAttribLocation(result,1,"inColor"); GL20.glLinkProgram(result);
        require(GL20.glGetProgrami(result,GL20.GL_LINK_STATUS) != 0,
                "program link: " + GL20.glGetProgramInfoLog(result,4096));
        System.out.println("PRECISION_PROGRAM " + index + " glsl=" + (modern ? 330 : 120) +
            " flat=" + (flat ? 1 : 0) + " direct_fragment=" + (direct ? 1 : 0) + " mrt=" + (mrt ? 1 : 0));
    }
    static void setup() {
        GL11.glViewport(0,0,64,64);
        GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
        GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
        GL11.glShadeModel(GL11.GL_SMOOTH);
        for (int state : new int[]{GL11.GL_DITHER,GL11.GL_BLEND,GL11.GL_ALPHA_TEST,
                GL11.GL_DEPTH_TEST,GL11.GL_STENCIL_TEST,GL11.GL_CULL_FACE,GL11.GL_SCISSOR_TEST,
                GL11.GL_TEXTURE_2D,GL11.GL_FOG,GL11.GL_LIGHTING,GL11.GL_COLOR_MATERIAL,
                GL11.GL_NORMALIZE,32826,GL30.GL_FRAMEBUFFER_SRGB}) GL11.glDisable(state);
        GL11.glColorMask(true,true,true,true); GL11.glPixelStorei(GL11.GL_PACK_ALIGNMENT,1);
        GL11.glLightModel(GL11.GL_LIGHT_MODEL_AMBIENT,vector(0,0,0,1));
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_TWO_SIDE,0);
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER,0);
        GL11.glLightModeli(GL12.GL_LIGHT_MODEL_COLOR_CONTROL,GL12.GL_SINGLE_COLOR);
        GL11.glEnable(GL11.GL_LIGHT0); GL11.glDisable(GL11.GL_LIGHT1);
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_POSITION,vector(0,0,1,0));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_DIFFUSE,vector(1,1,1,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_AMBIENT,vector(0,0,0,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_SPECULAR,vector(0,0,0,1));
        for (int component : new int[]{GL11.GL_AMBIENT,GL11.GL_SPECULAR,GL11.GL_EMISSION})
            GL11.glMaterial(GL11.GL_FRONT_AND_BACK,component,vector(0,0,0,1));
        GL11.glMaterialf(GL11.GL_FRONT_AND_BACK,GL11.GL_SHININESS,0);
        GL32.glProvokingVertex(GL32.GL_LAST_VERTEX_CONVENTION);
    }
    static int attachment(int attachment, int parameter) {
        return GL30.glGetFramebufferAttachmentParameteri(GL30.GL_FRAMEBUFFER,attachment,
            GL30.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) == GL11.GL_NONE ? 0 :
            GL30.glGetFramebufferAttachmentParameteri(GL30.GL_FRAMEBUFFER,attachment,parameter);
    }
    static void format(String target, boolean floating) {
        int color = floating ? GL30.GL_COLOR_ATTACHMENT0 : GL11.GL_BACK_LEFT;
        int depth = floating ? GL30.GL_DEPTH_ATTACHMENT : GL11.GL_DEPTH;
        int stencil = floating ? GL30.GL_STENCIL_ATTACHMENT : GL11.GL_STENCIL;
        int[] bits = new int[4];
        int[] names = {GL30.GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,GL30.GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
            GL30.GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,GL30.GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE};
        for (int index = 0; index < 4; ++index) {
            bits[index] = attachment(color,names[index]);
            require(bits[index] == (floating ? 32 : 8),"actual requested framebuffer channel precision");
        }
        IntBuffer viewport = BufferUtils.createIntBuffer(16); GL11.glGetInteger(GL11.GL_VIEWPORT,viewport);
        require(viewport.get(0) == 0 && viewport.get(1) == 0 && viewport.get(2) == 64 && viewport.get(3) == 64,
                "actual authored viewport");
        System.out.println("PRECISION_FORMAT " + target + " rgba=" + bits[0] + "," + bits[1] + "," + bits[2] + "," + bits[3] +
            " depth=" + attachment(depth,GL30.GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE) +
            " stencil=" + attachment(stencil,GL30.GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE) +
            " sample_buffers=" + GL11.glGetInteger(GL13_SAMPLE_BUFFERS) + " samples=" + GL11.glGetInteger(GL13_SAMPLES) +
            " double_buffer=" + GL11.glGetInteger(GL11.GL_DOUBLEBUFFER) +
            " component_type=" + attachment(color,GL30.GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) +
            " color_encoding=" + attachment(color,GL30.GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING) +
            " viewport=0,0,64,64 dither=" + (GL11.glIsEnabled(GL11.GL_DITHER) ? 1 : 0));
        require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual framebuffer format queries");
    }
    // Stable GL1.3 enum values; the actual state is queried, never supplied.
    static final int GL13_SAMPLE_BUFFERS = 32936, GL13_SAMPLES = 32937;
    static void clamps(String target, String label, int[] initial, int variant) {
        int[] values = variant == 0 ? initial : new int[]{variant == 1 ? GL11.GL_TRUE : GL11.GL_FALSE,
            GL11.GL_FALSE,GL11.GL_FALSE};
        int[] names = {GL30.GL_CLAMP_VERTEX_COLOR,GL30.GL_CLAMP_FRAGMENT_COLOR,GL30.GL_CLAMP_READ_COLOR};
        for (int index = 0; index < 3; ++index) GL30.glClampColor(names[index],values[index]);
        System.out.println("PRECISION_CLAMP " + target + " " + label + " vertex=" + GL11.glGetInteger(names[0]) +
            " fragment=" + GL11.glGetInteger(names[1]) + " read=" + GL11.glGetInteger(names[2]));
    }
    static FloatBuffer colors(float value) {
        FloatBuffer result = BufferUtils.createFloatBuffer(12);
        for (int index = 0; index < 12; ++index) result.put(value);
        result.flip(); return result;
    }
    static void upload(int vbo, FloatBuffer colors) {
        FloatBuffer vertices = BufferUtils.createFloatBuffer(21);
        float[][] points = {{-1,-1,0},{3,-1,0},{-1,3,0}};
        for (int vertex = 0; vertex < 3; ++vertex) {
            vertices.put(points[vertex]);
            for (int component = 0; component < 4; ++component) vertices.put(colors.get(vertex*4+component));
        }
        vertices.flip(); GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,vbo);
        GL15.glBufferData(GL15.GL_ARRAY_BUFFER,vertices,GL15.GL_STREAM_DRAW);
    }
    static void observe(String target, String clamp, String pipe, String scenario) {
        for (int[] point : POINTS) {
            ByteBuffer bytes = BufferUtils.createByteBuffer(4);
            FloatBuffer floats = BufferUtils.createFloatBuffer(4);
            GL11.glReadPixels(point[0],point[1],1,1,GL11.GL_RGBA,GL11.GL_FLOAT,floats);
            GL11.glReadPixels(point[0],point[1],1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,bytes);
            String rgba = String.format("%02x%02x%02x%02x",bytes.get(0)&255,bytes.get(1)&255,bytes.get(2)&255,bytes.get(3)&255);
            System.out.println("PRECISION_SAMPLE " + target + " " + clamp + " " + pipe + " " + scenario + " " +
                point[0] + " " + point[1] + " " + rgba + " " + words(floats,4));
            ++samples;
        }
        require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual separate float/byte readbacks");
    }
    static void floatLayout(int vbo, int fixedVao, int genericVao) {
        GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,vbo);
        GL30.glBindVertexArray(fixedVao);
        GL11.glVertexPointer(3,GL11.GL_FLOAT,28,0L); GL11.glColorPointer(4,GL11.GL_FLOAT,28,12L);
        GL11.glEnableClientState(GL11.GL_VERTEX_ARRAY); GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY);
        GL30.glBindVertexArray(genericVao);
        GL20.glVertexAttribPointer(0,3,GL11.GL_FLOAT,false,28,0L);
        GL20.glVertexAttribPointer(1,4,GL11.GL_FLOAT,false,28,12L);
        GL20.glEnableVertexAttribArray(0); GL20.glEnableVertexAttribArray(1);
    }
    static void draw(String target, String clamp, int pipe, String scenario, FloatBuffer input,
                     int vbo, int fixedVao, int genericVao) {
        float value = input.get(0); upload(vbo,input);
        GL20.glUseProgram(0); GL11.glDisable(GL11.GL_LIGHTING);
        GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        boolean fixed = pipe >= 3 && pipe <= 6 || pipe >= 11;
        GL30.glBindVertexArray(fixed ? fixedVao : genericVao);
        if (pipe == 0) {
            GL11.glClearColor(value,value,value,value); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
        } else if (fixed) {
            boolean flat = pipe == 4 || pipe == 6 || pipe == 12;
            GL11.glShadeModel(flat ? GL11.GL_FLAT : GL11.GL_SMOOTH);
            if (pipe == 3 || pipe == 4 || pipe >= 11) {
                GL11.glDisableClientState(GL11.GL_COLOR_ARRAY); GL11.glColor4f(value,value,value,value);
            } else GL11.glEnableClientState(GL11.GL_COLOR_ARRAY);
            if (pipe >= 11) {
                GL11.glEnable(GL11.GL_LIGHTING); GL11.glNormal3f(0,0,value);
                GL11.glMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_DIFFUSE,vector(1,1,1,value));
            }
            GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
        } else {
            int index = pipe == 1 ? 0 : pipe == 2 ? 1 : pipe - 5;
            GL20.glUseProgram(programs[index]);
            if (pipe <= 2) {
                int location = GL20.glGetUniformLocation(programs[index],"constantColor");
                require(location >= 0,"actual direct-fragment constant uniform");
                GL20.glUniform4f(location,value,value,value,value);
            }
            GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
        }
        observe(target,clamp,PIPES[pipe],scenario);
        ++draws;
    }
    static final int[][] BYTE_COLORS = {{128,64,32,255},{32,192,64,160},{64,32,224,96}};
    static FloatBuffer byteFloats(boolean gradient, boolean multiply) {
        FloatBuffer result = BufferUtils.createFloatBuffer(12);
        for (int vertex = 0; vertex < 3; ++vertex)
            for (int component : BYTE_COLORS[gradient ? vertex : 2])
                result.put(multiply ? component * (1.0f / 255.0f) : component / 255.0f);
        result.flip(); return result;
    }
    static void byteControls(String target, String clamp, int vbo, int fixedVao, int genericVao) {
        for (int scene = 0; scene < 2; ++scene) {
            boolean gradient = scene == 1; String label = gradient ? "byte_gradient" : "byte_constant";
            ByteBuffer bytes = BufferUtils.createByteBuffer(48);
            float[][] positions = {{-1,-1,0},{3,-1,0},{-1,3,0}};
            for (int vertex = 0; vertex < 3; ++vertex) {
                for (float coordinate : positions[vertex]) bytes.putFloat(coordinate);
                for (int component : BYTE_COLORS[gradient ? vertex : 2]) bytes.put((byte)component);
            }
            bytes.flip(); GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,vbo);
            GL15.glBufferData(GL15.GL_ARRAY_BUFFER,bytes,GL15.GL_STREAM_DRAW);
            GL30.glBindVertexArray(fixedVao); GL11.glVertexPointer(3,GL11.GL_FLOAT,16,0L);
            GL11.glColorPointer(4,GL11.GL_UNSIGNED_BYTE,16,12L); GL11.glEnableClientState(GL11.GL_COLOR_ARRAY);
            GL30.glBindVertexArray(genericVao);
            GL20.glVertexAttribPointer(0,3,GL11.GL_FLOAT,false,16,0L);
            GL20.glVertexAttribPointer(1,4,GL11.GL_UNSIGNED_BYTE,true,16,12L);
            GL11.glDisable(GL11.GL_LIGHTING);
            for (int pipe = 0; pipe < 6; ++pipe) {
                boolean flat = (pipe & 1) != 0;
                GL30.glBindVertexArray(pipe < 2 ? fixedVao : genericVao);
                GL20.glUseProgram(pipe < 2 ? 0 : programs[pipe]);
                GL11.glShadeModel(flat ? GL11.GL_FLAT : GL11.GL_SMOOTH);
                GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
                GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
                String name = (pipe < 2 ? "fixed" : pipe < 4 ? "generic120" : "generic330") +
                    "_byte_" + (flat ? "flat" : "smooth");
                observe(target,clamp,name,label); ++draws;
            }
        }
    }
    static void normalizedControls(String target, String clamp, int vbo, int fixedVao) {
        ByteBuffer bytes = BufferUtils.createByteBuffer(96);
        float[][] positions = {{-.75f,-.75f,0},{.75f,-.75f,0},{-.75f,.75f,0}};
        for (int vertex = 0; vertex < 3; ++vertex) {
            bytes.putFloat(vertex*32,positions[vertex][0]/2);
            bytes.putFloat(vertex*32+4,positions[vertex][1]/3);
            bytes.putFloat(vertex*32+8,0);
            bytes.put(vertex*32+24,(byte)16); bytes.put(vertex*32+25,(byte)64); bytes.put(vertex*32+26,(byte)64);
        }
        GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,vbo); GL15.glBufferData(GL15.GL_ARRAY_BUFFER,bytes,GL15.GL_STREAM_DRAW);
        GL30.glBindVertexArray(fixedVao); GL11.glVertexPointer(3,GL11.GL_FLOAT,32,0L);
        GL11.glNormalPointer(GL11.GL_BYTE,32,24L); GL11.glEnableClientState(GL11.GL_NORMAL_ARRAY);
        GL11.glDisableClientState(GL11.GL_COLOR_ARRAY); GL11.glColor4f(1,1,1,1);
        GL20.glUseProgram(0); GL11.glEnable(GL11.GL_LIGHTING); GL11.glEnable(GL11.GL_NORMALIZE);
        GL11.glShadeModel(GL11.GL_SMOOTH);
        GL11.glMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_DIFFUSE,vector(1,1,1,1));
        GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity(); GL11.glScaled(2,3,4);
        require(GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_TYPE) == GL11.GL_BYTE &&
                GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_STRIDE) == 32,"actual normalized-case byte array layout");
        for (int rescale = 0; rescale < 2; ++rescale) {
            if (rescale == 0) GL11.glDisable(32826); else GL11.glEnable(32826);
            FloatBuffer matrix = BufferUtils.createFloatBuffer(16);
            GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,matrix);
            System.out.println("PRECISION_NORMAL_STATE " + target + " " + clamp + " " +
                (rescale == 0 ? "normalized_A18" : "normalized_A20") + " " + words(matrix,16) +
                " type=" + GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_TYPE) +
                " stride=" + GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_STRIDE) +
                " normalize=" + (GL11.glIsEnabled(GL11.GL_NORMALIZE) ? 1 : 0) +
                " rescale=" + (GL11.glIsEnabled(32826) ? 1 : 0));
            GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
            GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
            observe(target,clamp,"fixed_lit_smooth",rescale == 0 ? "normalized_A18" : "normalized_A20"); ++draws;
        }
        GL11.glLoadIdentity(); GL11.glDisable(GL11.GL_NORMALIZE); GL11.glDisable(32826);
        GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY); GL11.glDisable(GL11.GL_LIGHTING);
    }
    static void mrtControls(int framebuffer, int vbo, int fixedVao, int genericVao, int[] initial) {
        GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,framebuffer);
        IntBuffer buffers = BufferUtils.createIntBuffer(2);
        buffers.put(GL30.GL_COLOR_ATTACHMENT0).put(GL30.GL_COLOR_ATTACHMENT1).flip(); GL20.glDrawBuffers(buffers);
        require(GL30.glCheckFramebufferStatus(GL30.GL_FRAMEBUFFER) == GL30.GL_FRAMEBUFFER_COMPLETE,
                "actual complete simultaneous RGBA8/RGBA32F framebuffer");
        int[] channelNames = {GL30.GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,GL30.GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
            GL30.GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,GL30.GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE};
        for (int name : channelNames)
            require(attachment(GL30.GL_COLOR_ATTACHMENT0,name) == 8 &&
                    attachment(GL30.GL_COLOR_ATTACHMENT1,name) == 32,"actual simultaneous attachment precision");
        System.out.println("PRECISION_MRT_FORMAT rgba8=8,8,8,8 rgba32f=32,32,32,32 component_types=" +
            attachment(GL30.GL_COLOR_ATTACHMENT0,GL30.GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) + "," +
            attachment(GL30.GL_COLOR_ATTACHMENT1,GL30.GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) +
            " draw_buffers=" + GL11.glGetInteger(GL20.GL_DRAW_BUFFER0) + "," + GL11.glGetInteger(GL20.GL_DRAW_BUFFER1));
        clamps("mrt","initial",initial,0); floatLayout(vbo,fixedVao,genericVao);
        float[] values = {0,.25f,.5f,.5746957659721375f,.625f,.75f,1,Math.nextDown(.5f)};
        for (int scene = 0; scene < values.length; ++scene) {
            FloatBuffer input = colors(values[scene]); String label = "mrt"+scene;
            System.out.println("PRECISION_INPUT " + label + " " + words(input,12));
            for (int pipe = 0; pipe < 3; ++pipe) {
                int program = programs[pipe+6]; upload(vbo,input); GL30.glBindVertexArray(genericVao);
                GL11.glDisable(GL11.GL_LIGHTING); GL20.glUseProgram(program);
                require(GL30.glGetFragDataLocation(program,"pixelColor") == 0 &&
                        GL30.glGetFragDataLocation(program,"floatColor") == 1,"actual distinct shader output locations");
                if (pipe == 0) GL20.glUniform4f(GL20.glGetUniformLocation(program,"constantColor"),
                    values[scene],values[scene],values[scene],values[scene]);
                GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
                GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
                String name = new String[]{"mrt_uniform","mrt_smooth","mrt_flat"}[pipe];
                GL11.glReadBuffer(GL30.GL_COLOR_ATTACHMENT0); observe("mrt8","initial",name,label);
                GL11.glReadBuffer(GL30.GL_COLOR_ATTACHMENT1); observe("mrt32f","initial",name,label); ++draws;
            }
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 3,"native directory, pinned library filename and context profile arguments");
        Locale.setDefault(Locale.ROOT); Path natives = Path.of(args[0]).toRealPath();
        require(args[1].equals("lwjgl64.dll") || args[1].equals("liblwjgl64.so"),"declared native platform");
        require(args[2].equals("legacy") || args[2].equals("compat33"),"declared actual context creator");
        Path allowed = natives.resolve(args[1]);
        Map<String,Path> jdkNatives = new LinkedHashMap<>();
        if (args[1].equals("liblwjgl64.so")) {
            Path jdkLib = Path.of(System.getProperty("java.home"),"lib").toRealPath();
            for (String name : new String[]{"awt","awt_xawt","awt_headless","jawt","fontmanager"}) {
                Path library = jdkLib.resolve(System.mapLibraryName(name)).toRealPath();
                require(library.getParent().equals(jdkLib),"declared JDK native remains in actual java.home/lib");
                jdkNatives.put(name,library);
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
                if (requested.isAbsolute() && requested.normalize().equals(allowed)) {
                    links.add(library); return;
                }
                for (Map.Entry<String,Path> entry : jdkNatives.entrySet()) {
                    if (library.equals(entry.getKey()) ||
                            requested.isAbsolute() && requested.normalize().equals(entry.getValue())) {
                        System.out.println("JDK_NATIVE_LINK " + entry.getKey() + " " + entry.getValue());
                        return;
                    }
                }
                throw new SecurityException("native outside exact LWJGL/JDK allowlist: " + bounded(library));
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Controlled driver precision");
        int framebuffer = 0, texture = 0, mrtFramebuffer = 0, byteTexture = 0,
            vbo = 0, fixedVao = 0, genericVao = 0;
        try {
            PixelFormat requested = new PixelFormat().withAlphaBits(8).withDepthBits(16);
            if (args[2].equals("legacy")) Display.create(requested);
            else Display.create(requested,new ContextAttribs(3,3).withProfileCompatibility(true));
            int beforeProfileQuery = GL11.glGetError();
            int profile = GL11.glGetInteger(GL32.GL_CONTEXT_PROFILE_MASK);
            int profileQueryError = GL11.glGetError();
            System.out.println("PRECISION_DRIVER " + bounded(GL11.glGetString(GL11.GL_VENDOR)) + " | " +
                bounded(GL11.glGetString(GL11.GL_RENDERER)) + " | " + bounded(GL11.glGetString(GL11.GL_VERSION)));
            System.out.println("PRECISION_CONTEXT creator=" + args[2] + " profile_mask=" + profile +
                " profile_query_error=" + profileQueryError + " prior_error=" + beforeProfileQuery + " glsl=" +
                bounded(GL11.glGetString(GL20.GL_SHADING_LANGUAGE_VERSION)));
            require(beforeProfileQuery == GL11.GL_NO_ERROR,"actual context creation has no pending GL error");
            if (args[2].equals("compat33"))
                require(profileQueryError == GL11.GL_NO_ERROR &&
                        (profile & GL32.GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0,"explicit actual compatibility context");
            require(GLContext.getCapabilities().OpenGL33 && GLContext.getCapabilities().GL_EXT_gpu_shader4,
                    "actual GL3.3 and GLSL120 flat-varying extension");
            int[] initial = {GL11.glGetInteger(GL30.GL_CLAMP_VERTEX_COLOR),
                GL11.glGetInteger(GL30.GL_CLAMP_FRAGMENT_COLOR),GL11.glGetInteger(GL30.GL_CLAMP_READ_COLOR)};
            System.out.println("PRECISION_INITIAL_CLAMP " + initial[0] + " " + initial[1] + " " + initial[2]);
            setup();
            int fixedError = GL11.glGetError();
            System.out.println("PRECISION_FIXED_SETUP error=" + fixedError);
            require(fixedError == GL11.GL_NO_ERROR,"actual legacy fixed-function setup accepted");
            for (int index = 0; index < 6; ++index)
                program(index,index == 1 || index >= 4,index == 3 || index == 5,index <= 1);
            for (int index = 6; index < 9; ++index) program(index,true,index == 8,index == 6);
            vbo = GL15.glGenBuffers(); fixedVao = GL30.glGenVertexArrays(); genericVao = GL30.glGenVertexArrays();
            require(vbo != 0 && fixedVao != 0 && genericVao != 0,"actual VBO and two VAO names");
            floatLayout(vbo,fixedVao,genericVao);
            texture = GL11.glGenTextures(); GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL30.GL_RGBA32F,64,64,0,GL11.GL_RGBA,GL11.GL_FLOAT,(FloatBuffer)null);
            framebuffer = GL30.glGenFramebuffers(); GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,framebuffer);
            GL30.glFramebufferTexture2D(GL30.GL_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT0,GL11.GL_TEXTURE_2D,texture,0);
            require(GL30.glCheckFramebufferStatus(GL30.GL_FRAMEBUFFER) == GL30.GL_FRAMEBUFFER_COMPLETE,
                    "actual complete RGBA32F framebuffer");
            byteTexture = GL11.glGenTextures(); GL11.glBindTexture(GL11.GL_TEXTURE_2D,byteTexture);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA8,64,64,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,(ByteBuffer)null);
            mrtFramebuffer = GL30.glGenFramebuffers(); GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,mrtFramebuffer);
            GL30.glFramebufferTexture2D(GL30.GL_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT0,GL11.GL_TEXTURE_2D,byteTexture,0);
            GL30.glFramebufferTexture2D(GL30.GL_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT1,GL11.GL_TEXTURE_2D,texture,0);
            List<String> labels = new ArrayList<>(); List<FloatBuffer> inputs = new ArrayList<>();
            float[] base = {0,.25f,.5f,.5746957659721375f,.625f,.75f,1};
            for (int index = 0; index < base.length; ++index) {
                labels.add("base"+index); inputs.add(colors(base[index]));
            }
            for (int threshold : new int[]{0,1,63,127,146,159,191,254}) {
                float center = (float)((threshold + .5) / 255.0);
                for (int step = -4; step <= 4; ++step) {
                    float value = center;
                    for (int count = 0; count < Math.abs(step); ++count)
                        value = step < 0 ? Math.nextDown(value) : Math.nextUp(value);
                    labels.add("t"+threshold+"u"+step); inputs.add(colors(value));
                }
            }
            labels.add("gradient_rgba"); inputs.add(vector(0,.25f,.75f,.125f, 1,.75f,.25f,.875f, .5f,.625f,0,.375f));
            labels.add("gradient_alpha"); inputs.add(vector(.25f,.5f,.75f,0, .25f,.5f,.75f,1, .25f,.5f,.75f,.375f));
            for (boolean gradient : new boolean[]{false,true})
                for (boolean multiply : new boolean[]{false,true}) {
                    labels.add("byte_float_" + (gradient ? "gradient_" : "constant_") + (multiply ? "multiply" : "divide"));
                    inputs.add(byteFloats(gradient,multiply));
                }
            for (int index = 0; index < labels.size(); ++index)
                System.out.println("PRECISION_INPUT " + labels.get(index) + " " + words(inputs.get(index),12));
            System.out.println("PRECISION_BYTE_INPUT byte_constant 4020e060,4020e060,4020e060");
            System.out.println("PRECISION_BYTE_INPUT byte_gradient 804020ff,20c040a0,4020e060");
            System.out.println("PRECISION_NORMAL_INPUT normalized_A18 byte=16,64,64 stride=32 " +
                "modelview=diag(2,3,4) geometry_xy_divisors=2,3 normalize=1 rescale=0 shade=7425");
            System.out.println("PRECISION_NORMAL_INPUT normalized_A20 byte=16,64,64 stride=32 " +
                "modelview=diag(2,3,4) geometry_xy_divisors=2,3 normalize=1 rescale=1 shade=7425");
            for (int target = 0; target < 2; ++target) {
                boolean floating = target == 1; String targetName = floating ? "rgba32f" : "rgba8";
                GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,floating ? framebuffer : 0);
                GL11.glDrawBuffer(floating ? GL30.GL_COLOR_ATTACHMENT0 : GL11.GL_BACK);
                GL11.glReadBuffer(floating ? GL30.GL_COLOR_ATTACHMENT0 : GL11.GL_BACK); format(targetName,floating);
                for (int clamp = 0; clamp < 3; ++clamp) {
                    String clampName = new String[]{"initial","vertex_clamped","unclamped"}[clamp];
                    clamps(targetName,clampName,initial,clamp);
                    floatLayout(vbo,fixedVao,genericVao);
                    for (int scene = 0; scene < labels.size(); ++scene) {
                        boolean gradient = scene >= 79;
                        for (int pipe = 0; pipe < PIPES.length; ++pipe) {
                            if (gradient && !(pipe == 5 || pipe == 6 || pipe >= 7 && pipe <= 10)) continue;
                            draw(targetName,clampName,pipe,labels.get(scene),inputs.get(scene),vbo,fixedVao,genericVao);
                        }
                    }
                    byteControls(targetName,clampName,vbo,fixedVao,genericVao);
                    normalizedControls(targetName,clampName,vbo,fixedVao);
                }
            }
            mrtControls(mrtFramebuffer,vbo,fixedVao,genericVao,initial);
            require(draws == 6486 && samples == 19530,"complete controlled precision inventory");
            require(network == 0,"no network/listener attempts");
        } finally {
            GL20.glUseProgram(0); GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,0);
            GL30.glBindVertexArray(0); GL15.glBindBuffer(GL15.GL_ARRAY_BUFFER,0);
            if (fixedVao != 0) GL30.glDeleteVertexArrays(fixedVao);
            if (genericVao != 0) GL30.glDeleteVertexArrays(genericVao);
            if (vbo != 0) GL15.glDeleteBuffers(vbo);
            if (framebuffer != 0) GL30.glDeleteFramebuffers(framebuffer);
            if (mrtFramebuffer != 0) GL30.glDeleteFramebuffers(mrtFramebuffer);
            if (texture != 0) GL11.glDeleteTextures(texture);
            if (byteTexture != 0) GL11.glDeleteTextures(byteTexture);
            for (int program : programs) if (program != 0) GL20.glDeleteProgram(program);
            for (int shader : shaders) if (shader != 0) GL20.glDeleteShader(shader);
            require(GL11.glGetError() == GL11.GL_NO_ERROR,"checked native cleanup"); Display.destroy();
        }
        System.out.println("Controlled driver precision: 6486 draws; 19530 samples; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
