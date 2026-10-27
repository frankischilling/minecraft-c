import java.awt.image.BufferedImage;
import java.lang.reflect.Field;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Map;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.Pbuffer;
import org.lwjgl.opengl.PixelFormat;

/** Authored observation of unchanged ji/ge on a real compatibility context. */
public final class TextureObservation {
    static Object get(Object object,Class<?> type,String name)throws Exception {
        Field field=type.getDeclaredField(name);field.setAccessible(true);return field.get(object);
    }
    static void state(String label,ji owner,int id,String outcome)throws Exception {
        IntBuffer e=(IntBuffer)get(owner,ji.class,"e");
        ByteBuffer f=(ByteBuffer)get(owner,ji.class,"f");
        List<?> ledger=(List<?>)get(null,ge.class,"b");Map<?,?> images=(Map<?,?>)get(owner,ji.class,"d");
        int bound=GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D);
        int min=GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER);
        int mag=GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER);
        System.out.printf(Locale.ROOT,"REG %s %s %d %d %d %d %d %d %d %d %d %d%n",label,outcome,ledger.size(),images.size(),e.position(),e.limit(),f.position(),f.limit(),bound==id?1:0,GL11.glIsTexture(id)?1:0,min,mag);
        if(outcome.equals("return")) {
            ByteBuffer pixels=BufferUtils.createByteBuffer(16);
            GL11.glGetTexImage(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixels);
            StringBuilder gpu=new StringBuilder("GPU "+label),transfer=new StringBuilder("TRANSFER "+label);
            for(int i=0;i<16;++i)gpu.append(String.format(Locale.ROOT," %02x",pixels.get(i)&255));
            for(int i=0;i<f.limit();++i)transfer.append(String.format(Locale.ROOT," %02x",f.get(i)&255));
            System.out.println(gpu);System.out.println(transfer);
        }
        if(GL11.glGetError()!=GL11.GL_NO_ERROR)throw new AssertionError("actual GL diagnostic");
    }
    public static void main(String[] args)throws Exception {
        if(args.length!=0)throw new IllegalArgumentException("no arguments");
        Pbuffer context=new Pbuffer(32,32,new PixelFormat(),null,null);
        try {
            context.makeCurrent();
            System.out.println("GL_VENDOR "+GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER "+GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION "+GL11.glGetString(GL11.GL_VERSION));
            for(String name:new String[]{"ji","ge","kv","qb"}) {
                Class<?> type=Class.forName(name,false,TextureObservation.class.getClassLoader());
                byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name+".class")){bytes=in.readAllBytes();}
                System.out.println("BIND\t"+name+"\t"+type.getProtectionDomain().getCodeSource().getLocation()+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
            }
            kv settings=new kv();ji owner=new ji(null,settings);
            try{owner.a((BufferedImage)null);throw new AssertionError("missing actual source null");}
            catch(NullPointerException expected){}
            state("null",owner,((IntBuffer)get(owner,ji.class,"e")).get(0),"null");
            BufferedImage image=new BufferedImage(2,2,BufferedImage.TYPE_INT_ARGB);
            int[] argb={0xff123456,0x80ff0000,0x00010203,0x4000ff00};image.setRGB(0,0,2,2,argb,0,2);
            int id=owner.a(image);state("plain",owner,id,"return");
            if(((Map<?,?>)get(owner,ji.class,"d")).get(Integer.valueOf(id))!=image)throw new AssertionError("original image identity");
            image.setRGB(0,0,0xffabcdef);
            ByteBuffer old=BufferUtils.createByteBuffer(16);GL11.glGetTexImage(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,old);
            if(old.get(0)!=(byte)0x12||image.getRGB(0,0)!=0xffabcdef)throw new AssertionError("distinct actual image/device copies");
            image.setRGB(0,0,argb[0]);settings.g=true;
            id=owner.a(image);state("anaglyph",owner,id,"return");
            ji noSettings=new ji(null,null);id=noSettings.a(image);state("null-settings",noSettings,id,"return");
            System.out.println("TEXTURE_ORIGINAL completed");
        }finally{context.destroy();}
    }
}
