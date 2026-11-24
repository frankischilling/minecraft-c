import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.file.Path;
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

/** Separate authored conversion/precision probe, with no original game execution. */
public final class NormalConversionReceiptObservation {
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
    static int shader(int type, String source) {
        int id = GL20.glCreateShader(type); GL20.glShaderSource(id,source); GL20.glCompileShader(id);
        require(GL20.glGetShaderi(id,GL20.GL_COMPILE_STATUS) != 0, GL20.glGetShaderInfoLog(id,4096)); return id;
    }
    static FloatBuffer transformedNormal(int program, int buffer, ByteBuffer vertices) {
        RasterObservation.arrays(vertices,true,false,false);
        GL15.glBindBuffer(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
        GL15.glBufferData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,RasterObservation.vector(0,0,0,0),GL15.GL_STREAM_READ);
        GL30.glBindBufferBase(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
        GL20.glUseProgram(program); GL11.glEnable(GL30.GL_RASTERIZER_DISCARD);
        GL30.glBeginTransformFeedback(GL11.GL_POINTS); GL11.glDrawArrays(GL11.GL_POINTS,0,1); GL30.glEndTransformFeedback();
        GL11.glDisable(GL30.GL_RASTERIZER_DISCARD); GL20.glUseProgram(0);
        FloatBuffer values = BufferUtils.createFloatBuffer(4);
        GL15.glGetBufferSubData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,values);
        require(Float.floatToRawIntBits(values.get(3)) == 0, "feedback writes only three float components");
        return values;
    }
    static String lit(int framebuffer, boolean floating, ByteBuffer vertices, int variant, byte value,
                      FloatBuffer captured) {
        GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,floating ? framebuffer : 0);
        GL11.glDrawBuffer(floating ? GL30.GL_COLOR_ATTACHMENT0 : GL11.GL_BACK);
        GL11.glReadBuffer(floating ? GL30.GL_COLOR_ATTACHMENT0 : GL11.GL_BACK);
        GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);
        if (variant == 0) GL11.glNormal3b(value,value,value);
        if (variant == 2) {
            float f = (2 * (int)value + 1) / 255.0f; GL11.glNormal3f(f,f,f);
        }
        if (variant == 3) GL11.glNormal3f(captured.get(0),captured.get(1),captured.get(2));
        RasterObservation.arrays(vertices,variant == 1,false,false);
        GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
        if (floating) {
            FloatBuffer result = BufferUtils.createFloatBuffer(4);
            GL11.glReadPixels(16,16,1,1,GL11.GL_RGBA,GL11.GL_FLOAT,result); return words(result,4);
        }
        ByteBuffer result = BufferUtils.createByteBuffer(4);
        GL11.glReadPixels(16,16,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,result);
        return String.format("%02x%02x%02x%02x",result.get(0)&255,result.get(1)&255,result.get(2)&255,result.get(3)&255);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 1, "native directory argument");
        Locale.setDefault(Locale.ROOT); Path natives = Path.of(args[0]).toRealPath();
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
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Legacy normal conversion and precision");
        int vertex = 0, fragment = 0, program = 0, buffer = 0, framebuffer = 0, texture = 0;
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("CONVERSION_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            RasterObservation.setup(true);
            GL30.glClampColor(GL30.GL_CLAMP_VERTEX_COLOR,GL11.GL_TRUE);
            GL30.glClampColor(GL30.GL_CLAMP_FRAGMENT_COLOR,GL11.GL_FALSE);
            GL30.glClampColor(GL30.GL_CLAMP_READ_COLOR,GL11.GL_FALSE);
            vertex = shader(GL20.GL_VERTEX_SHADER,"#version 120\nvarying vec3 capturedNormal; void main(){capturedNormal=gl_Normal;gl_Position=gl_Vertex;}\n");
            fragment = shader(GL20.GL_FRAGMENT_SHADER,"#version 120\nvoid main(){gl_FragColor=vec4(1.0);}\n");
            program = GL20.glCreateProgram(); GL20.glAttachShader(program,vertex); GL20.glAttachShader(program,fragment);
            GL30.glTransformFeedbackVaryings(program,new CharSequence[]{"capturedNormal"},GL30.GL_INTERLEAVED_ATTRIBS);
            GL20.glLinkProgram(program); require(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS) != 0,
                GL20.glGetProgramInfoLog(program,4096)); buffer = GL15.glGenBuffers();
            texture = GL11.glGenTextures(); GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL30.GL_RGBA32F,64,64,0,GL11.GL_RGBA,GL11.GL_FLOAT,(FloatBuffer)null);
            framebuffer = GL30.glGenFramebuffers(); GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,framebuffer);
            GL30.glFramebufferTexture2D(GL30.GL_FRAMEBUFFER,GL30.GL_COLOR_ATTACHMENT0,GL11.GL_TEXTURE_2D,texture,0);
            require(GL30.glCheckFramebufferStatus(GL30.GL_FRAMEBUFFER) == GL30.GL_FRAMEBUFFER_COMPLETE,
                    "actual complete RGBA32F framebuffer");
            require(GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_RED_SIZE) == 32,
                    "actual float texture precision");
            System.out.println("CONVERSION_FORMAT RGBA32F vertex_clamp=1 fragment_clamp=0 read_clamp=0");
            ByteBuffer vertices = RasterObservation.geometry(false,false,false);
            for (int number = -128; number <= 127; ++number) {
                byte value = (byte)number; RasterObservation.constantBytes(vertices,number,number,number);
                RasterObservation.light(0,GL11.GL_POSITION,0,0,number < 0 ? -1 : 1,0);
                GL11.glNormal3b(value,value,value);
                FloatBuffer current = BufferUtils.createFloatBuffer(16); GL11.glGetFloat(GL11.GL_CURRENT_NORMAL,current);
                FloatBuffer captured = transformedNormal(program,buffer,vertices);
                FloatBuffer formula = RasterObservation.vector((2*number+1)/255.0f,(2*number+1)/255.0f,(2*number+1)/255.0f);
                int arrayType = GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_TYPE);
                int arrayStride = GL11.glGetInteger(GL11.GL_NORMAL_ARRAY_STRIDE);
                int arrayBuffer = GL11.glGetInteger(GL15.GL_NORMAL_ARRAY_BUFFER_BINDING);
                require(arrayType == GL11.GL_BYTE && arrayStride == 32 && arrayBuffer == 0,
                        "actual client GL_BYTE normal array layout");
                if (number == -128) System.out.println("NORMAL_ARRAY_LAYOUT type=" + arrayType +
                    " stride=" + arrayStride + " buffer_binding=" + arrayBuffer);
                System.out.println("NORMAL_DECODE " + number + " " + words(current,3) + " " +
                    words(captured,3) + " " + words(formula,3));
                require(GL11.glGetError() == GL11.GL_NO_ERROR,"actual byte decode and layout query");
                if (number != -128 && number != -127 && number != -1 && number != 0 &&
                    number != 1 && number != 64 && number != 126 && number != 127) continue;
                StringBuilder row = new StringBuilder("NORMAL_CONVERSION " + number + " " + words(current,3) + " " +
                    words(captured,3) + " " + words(formula,3));
                for (int variant = 0; variant < 4; ++variant)
                    row.append(" ").append(lit(framebuffer,false,vertices,variant,value,captured));
                for (int variant = 0; variant < 4; ++variant)
                    row.append(" ").append(lit(framebuffer,true,vertices,variant,value,captured));
                System.out.println(row);
                require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual conversion/precision probe completed");
            }
            require(network == 0,"no network/listener attempts");
        } finally {
            GL20.glUseProgram(0); GL30.glBindFramebuffer(GL30.GL_FRAMEBUFFER,0);
            if (buffer != 0) GL15.glDeleteBuffers(buffer);
            if (framebuffer != 0) GL30.glDeleteFramebuffers(framebuffer);
            if (texture != 0) GL11.glDeleteTextures(texture);
            if (program != 0) GL20.glDeleteProgram(program);
            if (vertex != 0) GL20.glDeleteShader(vertex); if (fragment != 0) GL20.glDeleteShader(fragment);
            Display.destroy();
        }
        System.out.println("Original normal conversion receipts: 256 decode and 8 precision rows; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
