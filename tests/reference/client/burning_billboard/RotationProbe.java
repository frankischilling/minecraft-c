import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.*;
/** Authored ordinary SDK-only matrix probe: no game JAR/classes or game field writes. */
public final class RotationProbe {
 static PrintWriter out;static int ordinal;
 static String bits(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
 static Object matrix(){FloatBuffer v=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,v);List<String>r=new ArrayList<>();for(int j=0;j<16;j++)r.add(bits(v.get(j)));return r;}
 static void emit(String name,Object args){out.println(ConstructorJson.json(ConstructorJson.map("event","matrix","ordinal",ordinal++,"name",name,"arguments",args,"words",matrix(),"error",GL11.glGetError())));out.flush();}
 static List<String> args(float...a){List<String>r=new ArrayList<>();for(float v:a)r.add(bits(v));return r;}
 static void rotate(String name,float angle,float x,float y,float z){GL11.glLoadIdentity();GL11.glRotatef(angle,x,y,z);emit(name,args(angle,x,y,z));}
 public static void main(String[]args)throws Exception{
  Locale.setDefault(Locale.ROOT);Path output=Path.of(args[0]);try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("observations.ndjson")))){out=writer;Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Pinned SDK rotation probe");try{Display.create();
   out.println(ConstructorJson.json(ConstructorJson.map("event","device","vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"profile_mask",GL11.glGetInteger(0x9126),"framebuffer",ConstructorJson.map("red",GL11.glGetInteger(GL11.GL_RED_BITS),"green",GL11.glGetInteger(GL11.GL_GREEN_BITS),"blue",GL11.glGetInteger(GL11.GL_BLUE_BITS),"alpha",GL11.glGetInteger(GL11.GL_ALPHA_BITS),"depth",GL11.glGetInteger(GL11.GL_DEPTH_BITS),"stencil",GL11.glGetInteger(GL11.GL_STENCIL_BITS),"double_buffer",GL11.glGetBoolean(GL11.GL_DOUBLEBUFFER)),"error",GL11.glGetError())));
   GL11.glMatrixMode(GL11.GL_MODELVIEW);float[] angles={18,-18,25,-25,67.5f,-67.5f,0,90,180};for(int axis=0;axis<3;axis++)for(float angle:angles)rotate("axis"+axis+"_"+angle,angle,axis==0?1:0,axis==1?1:0,axis==2?1:0);
   for(int axis=0;axis<3;axis++)rotate("negative_axis"+axis,25,axis==0?-1:0,axis==1?-1:0,axis==2?-1:0);
   for(float angle:new float[]{18,-25,-67.5f,0,90,180})rotate("general123_"+angle,angle,1,2,3);
   float[] widths={1.4f,.6f,.95f,.6f,1.4f,1,1},heights={.9f,.6f,.95f,1.8f,0,1.4f,2.1f};for(int j=0;j<7;j++){
    GL11.glLoadIdentity();GL11.glTranslatef(0,-.25f,-6);emit("composition"+j+"_translate",args(0,-.25f,-6));
    GL11.glRotatef(18,1,0,0);emit("composition"+j+"_rotateX",args(18,1,0,0));
    GL11.glRotatef(-25,0,1,0);emit("composition"+j+"_rotateY",args(-25,0,1,0));
    float scale=widths[j]*1.4f;GL11.glScalef(scale,scale,scale);emit("composition"+j+"_scale",args(scale,scale,scale));
    GL11.glRotatef(-67.5f,0,1,0);emit("composition"+j+"_billboard",args(-67.5f,0,1,0));
    float depth=-.3f+(int)(heights[j]/scale)*.02f;GL11.glTranslatef(0,0,depth);emit("composition"+j+"_depth",args(0,0,depth));
   }
   System.out.println("SDK_ROTATION_PROBE "+ordinal+" complete");
  }finally{Display.destroy();}}
 }
}
