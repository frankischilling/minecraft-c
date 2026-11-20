import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;
import net.minecraft.client.Minecraft;

/** Whole unchanged AO body, genuine EW/World/Block/NW; authored input fields. */
public final class AmbientOcclusionObservation {
    static int network,nativeLinks;
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z",
        "A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final String[] COLOR={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static Object raw(Class<?> type)throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance",Class.class)
            .invoke(field(unsafe,"theUnsafe").get(null),type);
    }
    static void prepare(lm chunk,int id,int pattern) {
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
        Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);
        for(int x=7;x<=9;++x)for(int y=63;y<=65;++y)for(int z=7;z<=9;++z) {
            int block=pattern==0?0:pattern==1?1:((x+2*y+4*z)&2)!=0?1:0;
            chunk.b[(x<<11)|(z<<7)|y]=(byte)block;
            chunk.e.a(x,y,z,(x+y+z)&3);
            chunk.f.a(x,y,z,(x*3+y*5+z*7)&15);
            chunk.g.a(x,y,z,(x*7+y*11+z*13)&15);
        }
        chunk.b[(8<<11)|(8<<7)|64]=(byte)id;
    }
    static void observe(int index,int[] profile,cv renderer,int result,String exception,
                        int before,nw tessellator)throws Exception {
        System.out.print("AO "+index);
        for(int value:profile)System.out.print(" "+value);
        System.out.printf(" %d %s %d",result,exception,field(cv.class,"m").getBoolean(renderer)?1:0);
        for(String name:LIGHT)System.out.printf(" %08x",Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        int flags=0;for(int i=0;i<FLAGS.length;++i)if(field(cv.class,FLAGS[i]).getBoolean(renderer))flags|=1<<i;
        System.out.print(" "+flags);
        for(String name:COLOR)System.out.printf(" %08x",Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        int end=field(nw.class,"o").getInt(tessellator);
        int features=(field(nw.class,"l").getBoolean(tessellator)?1:0)|
            (field(nw.class,"m").getBoolean(tessellator)?2:0)|
            (field(nw.class,"n").getBoolean(tessellator)?4:0);
        System.out.printf(" %d %d %d %08x %d %d %d",field(nw.class,"h").getInt(tessellator),end,
            field(nw.class,"p").getInt(tessellator),field(nw.class,"k").getInt(tessellator),features,
            lm.a?1:0,end-before);
        int[] data=(int[])field(nw.class,"g").get(tessellator);
        for(int i=before;i<end;++i)System.out.printf(" %08x",data[i]);
        System.out.println();
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==3,"JAR, private authored World and profiles");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();
        java.util.List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkLink(String lib){++nativeLinks;throw new SecurityException();}
            @Override public void checkWrite(String f){require(Path.of(f).toAbsolutePath().normalize().startsWith(directory),"private World writes only");}
        });
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,
                Minecraft.class,kq.class,kv.class,ia.class,px.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original class");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
        ew cache=new ew(world,0,0,0,31,127,31);
        ew edgeCache=new ew(world,0,0,0,15,127,15);
        field(fd.class,"f").setInt(world,3);
        nw tessellator=nw.a;tessellator.b();
        Minecraft app=(Minecraft)raw(kq.class);kv settings=(kv)raw(kv.class);
        settings.k=true;app.z=settings;
        field(Minecraft.class,"a").set(null,app);
        int[] palette=new int[65536];Arrays.fill(palette,0x003569a7);ia.a(palette);
        int index=0;
        for(String line:profiles) {
            if(line.isBlank())continue;
            int[] p=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();
            require(p.length==7,"profile width");
            prepare(chunks[0],p[0],p[2]);
            if(p[6]==4)chunks[0].b[(8<<7)|64]=(byte)p[0];
            cv renderer=new cv(p[6]==1?null:p[6]==4?edgeCache:cache);
            field(cv.class,"O").setInt(renderer,p[1]);
            field(cv.class,"f").setBoolean(renderer,p[3]!=0);
            field(cv.class,"d").setInt(renderer,p[4]);cv.a=p[5]!=0;
            for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*0.03125f);
            for(int i=0;i<COLOR.length;++i)field(cv.class,COLOR[i]).setFloat(renderer,(i+1)*0.015625f);
            for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);
            int before=field(nw.class,"o").getInt(tessellator);lm.a=false;
            int result=-1;String exception="NONE";
            px.a=p[6]==3;
            try {result=((p[6]==2||p[6]==3)?renderer.b(uu.m[p[0]],8,64,8):
                         renderer.a(uu.m[p[0]],p[6]==4?0:8,64,8,0.3f,0.6f,0.9f))?1:0;}
            catch(NullPointerException failure){exception="NULL";}
            catch(ArrayIndexOutOfBoundsException failure){exception="INDEX";}
            observe(index++,p,renderer,result,exception,before,tessellator);
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same captured Chunk identity");
        require(network==0&&nativeLinks==0,"no network/native GL calls");
        System.out.println("Original AO: "+index+" whole-body rows; network=0; native=0; draw=0");
    }
}
