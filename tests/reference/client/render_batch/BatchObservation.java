import java.lang.reflect.Field;
import java.nio.BufferOverflowException;
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

/** Actual TG constructors/methods and prepared actual DK leaf operands. */
public final class BatchObservation {
    static int network,cpu,device,base;
    static final List<String> links=new ArrayList<>();
    static void require(boolean b,String message){if(!b)throw new AssertionError(message);}
    static Field field(Class<?> c,String name)throws Exception {Field f=c.getDeclaredField(name);f.setAccessible(true);return f;}
    static dk rawDK()throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return (dk)unsafe.getMethod("allocateInstance",Class.class).invoke(field(unsafe,"theUnsafe").get(null),dk.class);
    }
    static void row(tg t,String label,String exception,boolean gl)throws Exception {
        StringBuilder s=new StringBuilder((gl?"DRAW ":"TG ")+label+" "+exception);
        for(String name:new String[]{"a","b","c"})s.append(' ').append(field(tg.class,name).getInt(t));
        for(String name:new String[]{"d","e","f"})s.append(String.format(" %08x",Float.floatToRawIntBits(field(tg.class,name).getFloat(t))));
        for(String name:new String[]{"h","i"})s.append(field(tg.class,name).getBoolean(t)?" 1":" 0");
        IntBuffer b=(IntBuffer)field(tg.class,"g").get(t);
        if(b==null)s.append(" -1 -1 -1");
        else {
            s.append(' ').append(b.capacity()).append(' ').append(b.position()).append(' ').append(b.limit());
            IntBuffer copy=b.duplicate();copy.clear();
            for(int i=0;i<Math.min(b.capacity(),4);++i){int v=copy.get(i);s.append(' ').append(v>=base&&v<base+4&&gl?v-base:v);}
        }
        if(gl) {
            java.nio.FloatBuffer v=BufferUtils.createFloatBuffer(16);
            GL11.glGetFloat(GL11.GL_CURRENT_COLOR,v);
            for(int i=0;i<4;++i)s.append(String.format(" %08x",Float.floatToRawIntBits(v.get(i))));
            GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,v);
            for(int i=0;i<16;++i)s.append(String.format(" %08x",Float.floatToRawIntBits(v.get(i))));
            ++device;
        } else ++cpu;
        System.out.println(s);
    }
    static String invoke(tg t,int op,int value)throws Exception {
        try {if(op==0)t.b();else if(op==1)t.a(value);else t.a();return "NONE";}
        catch(NullPointerException e){return "NULL";}
        catch(BufferOverflowException e){return "OVERFLOW";}
    }
    static void cpu()throws Exception {
        dk d=rawDK();boolean[] flags={false,false};
        field(dk.class,"p").set(d,flags);field(dk.class,"C").setInt(d,100);
        int[] passes={-1,0,1,2,Integer.MIN_VALUE,Integer.MAX_VALUE};int index=0;
        for(int visible=0;visible<2;++visible)for(int mask=0;mask<4;++mask)for(int pass:passes) {
            d.o=visible!=0;flags[0]=(mask&1)!=0;flags[1]=(mask&2)!=0;
            int result=123456;String exception="NONE";
            try{result=d.a(pass);}catch(ArrayIndexOutOfBoundsException e){exception="INDEX";}
            System.out.printf("DK %d %d %d %d %d %s%n",index++,visible,mask,pass,result,exception);++cpu;
        }
        field(dk.class,"C").setInt(d,Integer.MAX_VALUE);d.o=true;flags[0]=flags[1]=false;
        System.out.printf("DK %d 1 0 1 %d NONE%n",index++,d.a(1));++cpu;
        field(dk.class,"p").set(d,null);
        for(int visible=0;visible<2;++visible) {
            d.o=visible!=0;int result=123456;String exception="NONE";
            try{result=d.a(-1);}catch(NullPointerException e){exception="NULL";}
            System.out.printf("DK %d %d -1 -1 %d %s%n",index++,visible,result,exception);++cpu;
        }
        field(dk.class,"p").set(d,flags);
        for(int built=0;built<2;++built)for(int mask=0;mask<4;++mask) {
            field(dk.class,"E").setBoolean(d,built!=0);flags[0]=(mask&1)!=0;flags[1]=(mask&2)!=0;
            System.out.printf("EMPTY %d %d %d%n",built,mask,d.e()?1:0);++cpu;
        }
        field(dk.class,"p").set(d,null);
        for(int built=0;built<2;++built) {
            field(dk.class,"E").setBoolean(d,built!=0);int result=-1;String exception="NONE";
            try{result=d.e()?1:0;}catch(NullPointerException e){exception="NULL";}
            System.out.printf("EMPTY_NULL %d %d %s%n",built,result,exception);++cpu;
        }
        tg t=new tg();row(t,"ctor","NONE",false);
        row(t,"inactive",invoke(t,2,0),false);
        t.a(4096,-64,-2048,4095.75,-63.5,-2047.125);row(t,"configure","NONE",false);
        int mask=(t.a(4096,-64,-2048)?1:0)|(t.a(4095,-64,-2048)?2:0)|
            (t.a(4096,-63,-2048)?4:0)|(t.a(4096,-64,-2047)?8:0);
        System.out.println("MATCH active "+mask);++cpu;
        t.a(123);t.a(-1);row(t,"append","NONE",false);
        t.b();row(t,"reset","NONE",false);
        System.out.println("MATCH reset "+(t.a(4096,-64,-2048)?1:0));++cpu;
        t.a(1,2,3,16777217.0,-0.0,Double.MIN_NORMAL);row(t,"narrow","NONE",false);
        t.a();row(t,"empty_flip","NONE",false);
        row(t,"empty_overflow",invoke(t,1,7),false);
        t.a(4,5,6,Double.POSITIVE_INFINITY,Double.NaN,Double.NEGATIVE_INFINITY);row(t,"special","NONE",false);
        t.b();row(t,"special_reset","NONE",false);
        field(tg.class,"g").set(t,null);
        row(t,"null_inactive",invoke(t,2,0),false);
        String exception="NONE";
        try{t.a(9,8,7,6,5,4);}catch(NullPointerException e){exception="NULL";}
        row(t,"null_configure",exception,false);
        row(t,"null_flip",invoke(t,2,0),false);
        field(tg.class,"i").setBoolean(t,true);
        row(t,"null_remaining",invoke(t,2,0),false);
        row(t,"null_put",invoke(t,1,7),false);
    }
    static void device()throws Exception {
        Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Original TG observation");
        try {
            Display.create();base=GL11.glGenLists(4);require(base>0,"real list names");
            float[][] colors={{.125f,.25f,.5f,.75f},{.75f,.5f,.25f,.125f},{.25f,.75f,.125f,.5f},{1,0,.5f,1}};
            for(int i=0;i<4;++i){GL11.glNewList(base+i,GL11.GL_COMPILE);float[] c=colors[i];GL11.glColor4f(c[0],c[1],c[2],c[3]);GL11.glTranslatef(i+.25f,-i-.5f,i*.125f);GL11.glEndList();}
            GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();GL11.glTranslatef(.25f,.5f,.75f);GL11.glColor4f(.5f,.25f,.75f,1);
            tg t=new tg();IntBuffer g=ge.d(2);field(tg.class,"g").set(t,g);
            t.a(16,32,48,15.5,31.25,47.125);row(t,"configure2","NONE",true);
            t.a(base);row(t,"put0","NONE",true);
            t.a(base+1);row(t,"auto_full","NONE",true);
            t.a();row(t,"replay","NONE",true);
            t.a(base+2);row(t,"overwrite0","NONE",true);
            t.a();row(t,"suffix_only","NONE",true);
            t.a(base+3);row(t,"full_no_flip","NONE",true);
            row(t,"overflow",invoke(t,1,base),true);
            t.b();row(t,"reset_full","NONE",true);
            t.a();row(t,"inactive_full","NONE",true);
            row(t,"inactive_overflow",invoke(t,1,base),true);
            t.a(-16,0,32,-15.5,0,31.25);row(t,"configure_again","NONE",true);
            t.a();row(t,"empty_again","NONE",true);
            row(t,"flipped_overflow",invoke(t,1,base),true);
            /* Configure keeps i=true; a source draw sees the current suffix. */
            t.a(4096,64,-2048,4095.75,63.5,-2047.125);t.a(base);row(t,"configure_keeps_i","NONE",true);
            t.a();row(t,"kept_i_suffix","NONE",true);
            dk d=rawDK();field(dk.class,"C").setInt(d,base);d.d();row(t,"dk_bounds","NONE",true);
            require(GL11.glGetError()==GL11.GL_NO_ERROR,"source GL completion");GL11.glDeleteLists(base,4);
        } finally {Display.destroy();}
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==2,"original JAR and private native directory");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),nativeDir=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkWrite(String f){throw new SecurityException("observer writes denied");}
            @Override public void checkLink(String lib){require(links.size()<16,"native link bound");links.add(lib);require(List.of("awt","jawt","awt_headless").contains(lib)||(Path.of(lib).isAbsolute()&&Path.of(lib).normalize().equals(nativeDir.resolve("lwjgl64.dll"))),"only pinned SDK/JDK native libraries");}
        });
        for(Class<?> c:new Class<?>[]{tg.class,dk.class,ge.class,nw.class})require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original class");
        cpu();require(links.isEmpty(),"CPU methods do not load native libraries");device();
        require(network==0,"no network");for(String link:links)System.out.println("NATIVE_LINK "+link);
        System.out.printf("Original batches: CPU=%d; device=%d; network=0%n",cpu,device);
    }
}
