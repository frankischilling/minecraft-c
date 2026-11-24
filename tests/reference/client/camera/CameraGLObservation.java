import java.lang.reflect.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.*;
import net.minecraft.client.Minecraft;

/** Authored actual-context observer; no substitute game class or constructor. */
public final class CameraGLObservation {
    static Object get(Object o,Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f.get(o);}
    static void set(Object o,Class<?> c,String n,Object v)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);f.set(o,v);}
    static Object call(px p,String n,Class<?>[] t,Object...a)throws Exception {
        Method m=px.class.getDeclaredMethod(n,t);m.setAccessible(true);
        try{return m.invoke(p,a);}catch(InvocationTargetException e){throw (Exception)e.getCause();}
    }
    static void matrix(String label,int mode) {
        FloatBuffer data=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(mode,data);
        StringBuilder row=new StringBuilder(label);
        for(int i=0;i<16;++i)row.append(String.format(Locale.ROOT," %08x",Float.floatToRawIntBits(data.get(i))));
        System.out.println(row);
    }
    static final class Added implements pm {
        final kq client; final dc player; final int action; final kv replacement; final fd target;
        int calls;
        Added(kq c,dc p,int a,kv r,fd w){client=c;player=p;action=a;replacement=r;target=w;}
        public void a(sn entity) {
            if(++calls!=1||entity.getClass()!=hl.class)throw new AssertionError("actual loaded Item");
            if(action==0||action==1){
                if(action==1)client.z=replacement;
                client.z.g=false;client.z.f=true;client.z.F=true;
            } else if(action==2) client.f=target;
            else if(action==3) client.z=null;
            else if(action==5) player.aI=target;
            else throw new AssertionError("observer action");
        }
        public void a(int x,int y,int z){throw new AssertionError("unexpected cell");}
        public void b(int a,int b,int c,int d,int e,int f){throw new AssertionError("unexpected range");}
        public void a(String n,double x,double y,double z,float a,float b){throw new AssertionError("unexpected sound");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void b(sn e){throw new AssertionError("unexpected removal");}
        public void e(){throw new AssertionError("unexpected render");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected music");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("unexpected tile");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static fd saved(Path root,String name,int id,int metadata)throws Exception {
        fd world=new fd(new mx(root.toFile(),name,false),name,xa.a(0),5143L);world.B=false;
        lm chunk=world.c(0,0);Arrays.fill(chunk.b,(byte)0);
        chunk.b[8*2048+8*128+64]=(byte)id;chunk.e.a(8,64,8,metadata);
        chunk.n=true;
        if(!world.b(new hl(world,8.5,64,8.5,new iz(1,1,0))))throw new AssertionError("real Item insert");
        world.a(true,null);
        Object provider=get(world,fd.class,"v");
        Set<?> pending=(Set<?>)get(provider,ok.class,"a");
        HashSet.class.getMethod("add",Object.class).invoke(pending,Integer.valueOf(yy.a(0,0)));
        world.p();
        if(!((Map<?,?>)get(provider,ok.class,"e")).isEmpty())throw new AssertionError("real cache detach");
        return world;
    }
    static void callbacks(kq client,px camera,Path root)throws Exception {
        Files.createDirectories(root);
        for(int which:new int[]{0,1,2,3,4,8,9}) {
            fd a=saved(root,"case"+which+"A",0,0),b=null,c=null;
            kv settings=new kv(),replacement=new kv();settings.g=true;settings.f=false;settings.F=false;
            client.z=settings;client.f=a;client.d=854;client.e=480;
            dc player=new dc(client,a,client.k,0);client.h=player;client.i=player;
            player.e(8.5,64.2,8.5);player.aJ=player.aM;player.aK=player.aN;player.aL=player.aO;
            player.aS=player.aU=33;player.aT=player.aV=12;player.Y=20;player.aa=0;
            fd watched=a;int action=which==1?1:0;fd target=null;
            if(which>=2&&which<=4) {
                a.a(8,64,8); // Acquire the Player World before the sleeping App.f getter.
                b=saved(root,"case"+which+"B",26,0);watched=b;client.f=b;
                set(player,gs.class,"u",true);action=which==4?3:2;
                if(which==2){c=saved(root,"case"+which+"C",26,3);target=c;}
            } else if(which>=8) {
                lm ready=a.c(0,0);ready.b[8*2048+8*128+64]=8;ready.e.a(8,64,8,7);
                b=saved(root,"case"+which+"B",8,0);watched=b;player.aI=b;
                action=5;target=which==8?a:null;
            }
            Added observer=new Added(client,player,action,replacement,target);watched.a(observer);
            Added second=null;
            if(c!=null){second=new Added(client,player,2,replacement,null);c.a(second);}
            boolean failed=false;
            try {call(camera,"a",new Class<?>[]{float.class,int.class},.5f,0);}
            catch(NullPointerException expected){failed=true;}
            if(failed!=(which==3||which==4||which==9)||observer.calls!=1||
               (second!=null&&second.calls!=1))throw new AssertionError("callback reached inventory "+which+" calls "+observer.calls+" failed "+failed+" XYZ "+player.aM+" "+player.aN+" "+player.aO);
            System.out.println("CALLBACK_ORIGINAL "+which+" "+(failed?"null":"return"));
            if(which<=4){matrix("CBPROJECTION "+which,GL11.GL_PROJECTION_MATRIX);matrix("CBMODELVIEW "+which,GL11.GL_MODELVIEW_MATRIX);}
            if(which==8) {
                float fov=(Float)call(camera,"d",new Class<?>[]{float.class},.5f);
                if(fov!=70)throw new AssertionError("actual aI73 metadata reload");
            }
            int error=GL11.glGetError();if(error!=GL11.GL_NO_ERROR)throw new AssertionError("callback GL error "+error);
        }
    }
    public static void main(String[]args)throws Exception {
        Pbuffer context=new Pbuffer(32,32,new PixelFormat(),null,null);
        kq client=null;
        try {
            context.makeCurrent();
            System.out.println("GL_VENDOR "+GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER "+GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION "+GL11.glGetString(GL11.GL_VERSION));
            client=new kq(null,null,null,854,480,false,null);client.z=new kv();
            client.p=new ji(null,client.z);set(client,Minecraft.class,"S",new cx());
            client.g=new n(client,client.p);
            fd world=new fd(new wy(),"CameraOriginalReference",xa.a(0),5143L);
            world.a(new br(11,190,-7));world.B=false;client.f=world;
            gr session=new gr("CameraReference","-");client.k=session;
            dc player=new dc(client,world,session,0);client.h=player;client.i=player;
            px camera=new px(client);
            System.out.println("OWNERS "+player.getClass().getName()+" "+world.getClass().getName()+" "+client.p.getClass().getName()+" "+client.g.getClass().getName()+" "+camera.getClass().getName());

            Object hand=get(camera,px.class,"c");
            Object map=get(hand,ra.class,"f");
            int texture=(Integer)get(map,iy.class,"b");
            int[] cells=(int[])get(map,iy.class,"a");
            if(cells.length!=16384)throw new AssertionError("real map cells");
            for(int cell:cells)if(cell!=0)throw new AssertionError("real map zero cells");
            java.util.Map<?,?> images=(java.util.Map<?,?>)get(client.p,ji.class,"d");
            if(images.size()!=1)throw new AssertionError("real map retention count");
            if(get(map,iy.class,"c")!=client.z||get(map,iy.class,"d")!=client.q)throw new AssertionError("real map settings/font identities");
            Object retained=images.get(texture);
            if(!(retained instanceof java.awt.image.BufferedImage))throw new AssertionError("real retained map image");
            java.awt.image.BufferedImage mapImage=(java.awt.image.BufferedImage)retained;
            if(mapImage.getWidth()!=128||mapImage.getHeight()!=128)throw new AssertionError("real map image dimensions");
            for(int argb:mapImage.getRGB(0,0,128,128,null,0,128))if(argb!=0)throw new AssertionError("real map transparent pixels");
            ByteBuffer texels=BufferUtils.createByteBuffer(65536);
            GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);
            GL11.glGetTexImage(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,texels);
            for(int i=0;i<texels.capacity();++i)if(texels.get(i)!=0)throw new AssertionError("actual map device texels");
            IntBuffer ids=(IntBuffer)get(client.p,ji.class,"e");ByteBuffer transfer=(ByteBuffer)get(client.p,ji.class,"f");
            if(ids.position()!=0||ids.limit()!=1||transfer.position()!=0||transfer.limit()!=65536||transfer.capacity()!=1048576)
                throw new AssertionError("actual constructor buffer prefix");
            java.awt.image.BufferedImage fallback=(java.awt.image.BufferedImage)get(client.p,ji.class,"m");
            java.awt.Graphics graphics=fallback.getGraphics();
            try {System.out.println("FALLBACK_FONT "+graphics.getFont().getName()+" "+graphics.getFont().getStyle()+" "+graphics.getFont().getSize());}
            finally {graphics.dispose();}
            System.out.println("MAP_OWNER actual-ra-iy-ji image128x128 int16384 rgba65536-zero retained1 transfer1048576-limit65536");

            player.aS=33.0f;player.aU=12.0f;player.aT=-17.0f;player.aV=3.0f;
            player.aJ=player.aM-0.75;player.aK=player.aN+0.25;player.aL=player.aO+1.25;

            for(String name:new String[]{"px","ra","iy","ji","n","ge","cu","cv","dc","gs","ls","sn","fd","wy","rh","gr","kq","net.minecraft.client.Minecraft","kv","cx","ln","uu","in","lm","wi","pm","mx","ok","yy","hl","iz"}) {
                Class<?> type=Class.forName(name,false,CameraGLObservation.class.getClassLoader());
                java.security.MessageDigest hash=java.security.MessageDigest.getInstance("SHA-256");
                try(java.io.InputStream input=type.getResourceAsStream("/"+name.replace('.','/')+".class")) {
                    byte[] bytes=input.readAllBytes();
                    System.out.println("BIND\t"+name+"\t"+type.getProtectionDomain().getCodeSource().getLocation()+"\t"+java.util.HexFormat.of().formatHex(hash.digest(bytes)));
                }
            }
            for(int which=0;which<29;++which) {
                if(which==19||which==20)continue; // Native FP-environment admission, not a JDK source exception.
                kv settings=client.z;
                settings.f=which>=1;settings.g=which==5||which==6;
                settings.A=which==10||which==11;settings.F=which==8||which==10;
                settings.e=which==12?-1:which==13?32:0;
                client.d=which==23?0:854;client.e=which==24?0:480;
                player.Y=which==7?0:20;player.ad=which==7?17:0;
                player.aa=which==2?7:0;player.ab=which==14?0:10;player.ac=60;
                player.bi=which>=1?1.125f:0;player.bj=which>=1?1.5f:0;
                player.h=which>=1?0.1f:0;player.i=which>=1?0.175f:0;
                player.af=which>=1?2.0f:0;player.ag=which>=1?-1.0f:0;
                player.C=which==3?0.2f:0;player.B=which==3?0.7f:0;
                if(which==22)player.B=Float.intBitsToFloat(0x7fc01234);
                set(player,gs.class,"u",which==9||which==10||which==26||which==27||which==28);
                player.bf=which==4?1.4f:1.62f;
                if(which>=25) {
                    lm chunk=world.c(0,-1);
                    chunk.b[11*2048+9*128+64]=(byte)(which==25?8:26);
                    chunk.e.a(11,64,9,which==27?3:0);
                    player.aN=64.1;player.aK=player.aN+.25;
                }
                float partial=which==14?0:which==15?1:which==16?1.5f:0.5f;
                if(which==17)client.z=null;
                if(which==18)client.i=null;
                if(which==21)player.aI=null;
                if(which==28)client.f=null;
                try {
                    call(camera,"a",new Class<?>[]{float.class,int.class},partial,which==6?1:0);
                    if(which==17||which==18||which==21||which==28)throw new AssertionError("missing actual null failure "+which);
                    float fov=(Float)call(camera,"d",new Class<?>[]{float.class},partial);
                    System.out.println("CASE "+which+" FOV "+String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(fov))+" CLOUD "+get(camera,px.class,"D"));
                    // Authored scalar projection of actual source fields; separately labelled from GL queries.
                    double x=player.aJ+(player.aM-player.aJ)*(double)partial;
                    double y=player.aK+(player.aN-player.aK)*(double)partial;
                    double z=player.aL+(player.aO-player.aL)*(double)partial;
                    float offset=player.bf-1.62f;
                    if(which==9||which==10||which==26||which==27||which==28)offset=(float)((double)offset+1.0);
                    System.out.printf(Locale.ROOT,"ORIGIN %d %016x %016x %016x %016x%n",which,
                        Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),Double.doubleToRawLongBits(y-(double)offset));
                    matrix("PROJECTION "+which,GL11.GL_PROJECTION_MATRIX);
                    matrix("MODELVIEW "+which,GL11.GL_MODELVIEW_MATRIX);
                    int error=GL11.glGetError();if(error!=GL11.GL_NO_ERROR)throw new AssertionError("GL error "+error);
                } catch(NullPointerException failure) {
                    if(which!=17&&which!=18&&which!=21&&which!=28)throw failure;
                    System.out.println("NULL "+which+" "+failure.getClass().getName());
                } finally {client.z=settings;client.i=player;player.aI=world;client.f=world;}
            }
            callbacks(client,camera,Path.of(args[0]));
            System.out.println("CAMERA_GL_ORIGINAL completed");
        } finally {
            if(client!=null){client.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);if(t.isAlive())throw new AssertionError("timer live");}}
            context.destroy();
        }
    }
}
