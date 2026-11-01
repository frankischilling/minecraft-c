import java.nio.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.*;

public final class NormalTransition {
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value,String message) { if(!value)throw new AssertionError(message); }
    static FloatBuffer vector(float... values) { FloatBuffer b=BufferUtils.createFloatBuffer(values.length);b.put(values).flip();return b; }
    static String words(FloatBuffer b,int count) { StringBuilder s=new StringBuilder();for(int i=0;i<count;i++){if(i>0)s.append(',');s.append(String.format("%08x",Float.floatToRawIntBits(b.get(i))));}return s.toString(); }
    static String normal() { FloatBuffer b=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(GL11.GL_CURRENT_NORMAL,b);return words(b,3); }
    static String pixel() { ByteBuffer b=BufferUtils.createByteBuffer(4);GL11.glReadPixels(32,32,1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,b);return String.format("%02x%02x%02x%02x",b.get(0)&255,b.get(1)&255,b.get(2)&255,b.get(3)&255); }
    static void quad(boolean offscreen,boolean normal,float x,float y,float z) {
        nw.a.b();
        if(normal)nw.a.b(x,y,z); else nw.a.a(1.0f,1.0f,1.0f,1.0f);
        double a=offscreen?3:-0.5,b=offscreen?4:0.5;
        nw.a.a(a,-0.5,0.0,0.0,0.0);nw.a.a(b,-0.5,0.0,1.0,0.0);
        nw.a.a(b,0.5,0.0,1.0,1.0);nw.a.a(a,0.5,0.0,0.0,1.0);
        nw.a.a();
    }
    static int shader(int kind,String text) {
        int value=GL20.glCreateShader(kind);GL20.glShaderSource(value,text);GL20.glCompileShader(value);
        require(GL20.glGetShaderi(value,GL20.GL_COMPILE_STATUS)!=0,GL20.glGetShaderInfoLog(value,4096));return value;
    }
    static void setup(boolean lighting) {
        GL11.glViewport(0,0,64,64);GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();
        GL11.glOrtho(-1,1,-1,1,-1,1);GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
        GL11.glDisable(GL11.GL_TEXTURE_2D);GL11.glDisable(GL11.GL_DEPTH_TEST);GL11.glDisable(GL11.GL_BLEND);
        GL11.glDisable(GL11.GL_CULL_FACE);GL11.glDisable(GL11.GL_ALPHA_TEST);GL11.glDisable(GL11.GL_FOG);
        GL11.glDisable(GL11.GL_NORMALIZE);GL11.glEnable(GL11.GL_COLOR_MATERIAL);
        GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_AMBIENT_AND_DIFFUSE);
        GL11.glLightModel(GL11.GL_LIGHT_MODEL_AMBIENT,vector(0,0,0,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_AMBIENT,vector(0,0,0,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_DIFFUSE,vector(1,1,1,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_SPECULAR,vector(0,0,0,1));
        GL11.glLight(GL11.GL_LIGHT0,GL11.GL_POSITION,vector(0,0,1,0));GL11.glEnable(GL11.GL_LIGHT0);
        if(lighting)GL11.glEnable(GL11.GL_LIGHTING);else GL11.glDisable(GL11.GL_LIGHTING);
        GL11.glColor4f(1,1,1,1);GL11.glNormal3f(.25f,.5f,.75f);
        GL11.glClearColor(0,0,0,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length==1,"actual native directory");Locale.setDefault(Locale.ROOT);
        Path natives=Path.of(args[0]).toRealPath();String nativeName=System.getProperty("os.name").startsWith("Windows")?"lwjgl64.dll":"liblwjgl64.so";
        Set<Path> jdkNatives=new HashSet<>();
        for(String name:List.of("awt","awt_xawt","awt_headless","jawt","fontmanager")) {
            for(String folder:List.of("lib","bin")){Path p=Path.of(System.getProperty("java.home"),folder,System.mapLibraryName(name));if(Files.isRegularFile(p))jdkNatives.add(p.toRealPath());}
        }
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){network++;throw new SecurityException("network");}
            public void checkListen(int p){network++;throw new SecurityException("network");}
            public void checkWrite(String p){throw new SecurityException("write");}
            public void checkDelete(String p){throw new SecurityException("delete");}
            public void checkLink(String p){
                links.add(p);Path path=Path.of(p);
                if(path.isAbsolute()&&(path.normalize().equals(natives.resolve(nativeName))||jdkNatives.contains(path.normalize())))return;
                if(List.of("awt","awt_xawt","awt_headless","jawt","fontmanager").contains(p))return;
                throw new SecurityException("unapproved native "+p);
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Original NW normal transition");
        int program=0,vs=0,fs=0,buffer=0;
        try {
            Display.create(new PixelFormat().withDepthBits(24));
            System.out.println("NORMAL_DRIVER "+GL11.glGetString(GL11.GL_VENDOR)+" | "+GL11.glGetString(GL11.GL_RENDERER)+" | "+GL11.glGetString(GL11.GL_VERSION));
            System.out.println("NORMAL_ORIGIN "+nw.class.getProtectionDomain().getCodeSource().getLocation()+" | "+GL11.class.getProtectionDomain().getCodeSource().getLocation());
            vs=shader(GL20.GL_VERTEX_SHADER,"#version 120\nvarying vec4 observed; void main(){gl_Position=gl_Vertex;observed=vec4(gl_Normal,0.0);}");
            fs=shader(GL20.GL_FRAGMENT_SHADER,"#version 120\nvoid main(){gl_FragColor=vec4(1.0);}");
            program=GL20.glCreateProgram();GL20.glAttachShader(program,vs);GL20.glAttachShader(program,fs);
            GL30.glTransformFeedbackVaryings(program,new CharSequence[]{"observed"},GL30.GL_INTERLEAVED_ATTRIBS);GL20.glLinkProgram(program);
            require(GL20.glGetProgrami(program,GL20.GL_LINK_STATUS)!=0,GL20.glGetProgramInfoLog(program,4096));
            buffer=GL15.glGenBuffers();
            float[][] normals={{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}};
            for(boolean feedback:new boolean[]{false,true})for(boolean lighting:new boolean[]{false,true})for(boolean queried:new boolean[]{false,true}) {
                GL20.glUseProgram(0);setup(lighting);GL20.glUseProgram(feedback?program:0);
                String tag=(feedback?"feedback":"fixed")+"-lighting"+(lighting?1:0)+"-queries"+(queried?1:0);
                if(feedback){GL15.glBindBuffer(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,buffer);GL15.glBufferData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,96,GL15.GL_STREAM_READ);GL30.glBindBufferBase(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);}
                if(queried)System.out.println("NORMAL_CURRENT "+tag+" initial "+normal());
                GL11.glPushMatrix();GL11.glEnable(GL12.GL_RESCALE_NORMAL);GL11.glScalef(.25f,.25f,.25f);
                for(int i=0;i<6;i++) {
                    quad(true,true,normals[i][0],normals[i][1],normals[i][2]);
                    if(queried)System.out.println("NORMAL_CURRENT "+tag+" face"+i+" "+normal());
                }
                GL11.glDisable(GL12.GL_RESCALE_NORMAL);GL11.glPopMatrix();
                if(feedback)GL30.glBeginTransformFeedback(GL11.GL_TRIANGLES);
                quad(false,false,0,0,0);
                if(feedback){GL30.glEndTransformFeedback();FloatBuffer out=BufferUtils.createFloatBuffer(24);GL15.glGetBufferSubData(GL30.GL_TRANSFORM_FEEDBACK_BUFFER,0,out);System.out.println("NORMAL_CONSUMED "+tag+" "+words(out,24));}
                else System.out.println("NORMAL_PIXEL "+tag+" "+pixel());
                System.out.println("NORMAL_AFTER "+tag+" "+normal()+" normal-array="+(GL11.glIsEnabled(GL11.GL_NORMAL_ARRAY)?1:0)+" error="+GL11.glGetError());
            }
            System.out.println("NORMAL_FINISH network="+network+" links="+String.join("|",links));
        } finally {
            GL20.glUseProgram(0);if(buffer!=0)GL15.glDeleteBuffers(buffer);if(program!=0)GL20.glDeleteProgram(program);
            if(vs!=0)GL20.glDeleteShader(vs);if(fs!=0)GL20.glDeleteShader(fs);Display.destroy();
        }
    }
}
