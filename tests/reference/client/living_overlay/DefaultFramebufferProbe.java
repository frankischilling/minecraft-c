import java.nio.*;
import java.nio.file.*;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.*;

/** Ordinary pinned SDK default Display framebuffer observations; no game classes. */
public final class DefaultFramebufferProbe {
    public static void main(String[] args) throws Exception {
        PixelFormat request=new PixelFormat();
        Display.setDisplayMode(new DisplayMode(64,64));
        Display.setTitle("Original SDK framebuffer observation");
        Display.create();
        try {
            Map<String,Object> data=ConstructorJson.map("vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"request",ConstructorJson.map("bits_per_pixel",request.getBitsPerPixel(),"alpha",request.getAlphaBits(),"depth",request.getDepthBits(),"stencil",request.getStencilBits()),"actual",ConstructorJson.map("red",GL11.glGetInteger(GL11.GL_RED_BITS),"green",GL11.glGetInteger(GL11.GL_GREEN_BITS),"blue",GL11.glGetInteger(GL11.GL_BLUE_BITS),"alpha",GL11.glGetInteger(GL11.GL_ALPHA_BITS),"depth",GL11.glGetInteger(GL11.GL_DEPTH_BITS),"stencil",GL11.glGetInteger(GL11.GL_STENCIL_BITS),"double_buffer",GL11.glGetBoolean(GL11.GL_DOUBLEBUFFER),"profile_mask",GL11.glGetInteger(0x9126)));
            ByteBuffer mask=BufferUtils.createByteBuffer(16);GL11.glGetBoolean(GL11.GL_COLOR_WRITEMASK,mask);List<Boolean> writeMask=new ArrayList<>();for(int i=0;i<4;i++)writeMask.add(mask.get(i)!=0);data.put("initial_color_writemask",writeMask);
            GL11.glViewport(0,0,64,64);GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glOrtho(-1,1,-1,1,-1,1);GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
            GL11.glDisable(GL11.GL_DEPTH_TEST);GL11.glDisable(GL11.GL_TEXTURE_2D);GL11.glDisable(GL11.GL_LIGHTING);GL11.glDisable(GL11.GL_FOG);GL11.glDisable(GL11.GL_ALPHA_TEST);GL11.glClearColor(.125f,.25f,.5f,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT|GL11.GL_DEPTH_BUFFER_BIT);
            ByteBuffer before=BufferUtils.createByteBuffer(64*64*4);GL11.glReadPixels(0,0,64,64,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,before);byte[] a=new byte[before.capacity()];before.get(a);Files.write(Path.of(args[0]).resolve("before.rgba"),a);
            GL11.glEnable(GL11.GL_BLEND);GL11.glBlendFunc(GL11.GL_SRC_ALPHA,GL11.GL_ONE_MINUS_SRC_ALPHA);GL11.glColor4f(1,0,0,.4f);GL11.glBegin(GL11.GL_QUADS);GL11.glVertex2f(-1,-1);GL11.glVertex2f(1,-1);GL11.glVertex2f(1,1);GL11.glVertex2f(-1,1);GL11.glEnd();GL11.glFinish();
            ByteBuffer after=BufferUtils.createByteBuffer(64*64*4);GL11.glReadPixels(0,0,64,64,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,after);byte[] b=new byte[after.capacity()];after.get(b);Files.write(Path.of(args[0]).resolve("after.rgba"),b);
            Set<Integer> beforeAlpha=new TreeSet<>(),afterAlpha=new TreeSet<>();for(int i=3;i<a.length;i+=4){beforeAlpha.add(a[i]&255);afterAlpha.add(b[i]&255);}data.put("before_alpha_values",beforeAlpha);data.put("after_alpha_values",afterAlpha);data.put("gl_error",GL11.glGetError());
            System.out.println(ConstructorJson.json(data));
        } finally {Display.destroy();}
    }
}
