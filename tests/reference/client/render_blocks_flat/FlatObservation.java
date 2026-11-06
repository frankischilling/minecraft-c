import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;

/** Whole unchanged flat body over genuine World/EW/Block/NW owners. */
public final class FlatObservation {
    static int network,nativeLinks;
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z",
        "A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final String[] COLOR={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] BOUNDS={"bs","bt","bu","bv","bw","bx"};
    static final float[][] BOX={{0,0,0,1,1,1},{.125f,.25f,.375f,.875f,.75f,.625f},
        {-.25f,-.5f,-.125f,1.5f,1.25f,1.75f},{0,0,0,1,.5f,1}};
    static final float[][] RGB={{.3f,.6f,.9f},{-.25f,1.5f,2f},{0,0,0}};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
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
    static void observe(int index,int[] profile,cv renderer,uu block,int result,String exception,
                        int before,nw tessellator)throws Exception {
        System.out.print("FLAT "+index);
        for(int value:profile)System.out.print(" "+value);
        System.out.printf(" %d %s %d",result,exception,field(cv.class,"m").getBoolean(renderer)?1:0);
        for(String name:LIGHT)System.out.printf(" %08x",Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        int flags=0;for(int i=0;i<FLAGS.length;++i)if(field(cv.class,FLAGS[i]).getBoolean(renderer))flags|=1<<i;
        System.out.print(" "+flags);
        for(String name:COLOR)System.out.printf(" %08x",Float.floatToRawIntBits(field(cv.class,name).getFloat(renderer)));
        for(String name:BOUNDS)System.out.printf(" %016x",Double.doubleToRawLongBits(field(uu.class,name).getDouble(block)));
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
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,ln.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original class");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
        ew cache=new ew(world,0,0,0,31,127,31);
        field(fd.class,"f").setInt(world,3);
        nw tessellator=nw.a;tessellator.b();tessellator.b(-.25,.5,-.75);
        Method bounds=uu.class.getDeclaredMethod("a",float.class,float.class,float.class,float.class,float.class,float.class);
        int index=0;
        for(String line:profiles) {
            if(line.isBlank())continue;
            int[] p=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();
            require(p.length==11,"profile width");
            prepare(chunks[0],p[0],p[1]);
            uu block=uu.m[p[0]];require(block!=null,"actual Registry Block");
            int priorTexture=block.bm;ln priorMaterial=block.bA;
            double[] priorBounds=new double[6];
            for(int i=0;i<6;++i)priorBounds[i]=field(uu.class,BOUNDS[i]).getDouble(block);
            float[] box=BOX[p[6]];
            bounds.invoke(block,box[0],box[1],box[2],box[3],box[4],box[5]);
            if(p[7]>=0)block.bm=p[7];
            if(p[5]==3)field(uu.class,"bA").set(block,null);
            cv renderer=new cv(p[5]==1?null:cache);
            field(cv.class,"m").setBoolean(renderer,true);
            field(cv.class,"f").setBoolean(renderer,p[2]!=0);
            field(cv.class,"d").setInt(renderer,p[3]);cv.a=p[4]!=0;
            field(cv.class,"e").setBoolean(renderer,p[9]!=0);
            for(String name:new String[]{"g","h","i","j","k","l"})field(cv.class,name).setInt(renderer,p[8]);
            for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*.03125f);
            for(int i=0;i<COLOR.length;++i)field(cv.class,COLOR[i]).setFloat(renderer,(i+1)*.015625f);
            for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);
            int before=field(nw.class,"o").getInt(tessellator);lm.a=false;
            int result=-1;String exception="NONE";
            float[] rgb=RGB[p[10]];
            try {result=renderer.b(p[5]==2?null:block,8,64,8,rgb[0],rgb[1],rgb[2])?1:0;}
            catch(NullPointerException failure){exception="NULL";}
            catch(ArrayIndexOutOfBoundsException failure){exception="INDEX";}
            require(field(cv.class,"c").get(renderer)==(p[5]==1?null:cache)&&
                field(cv.class,"d").getInt(renderer)==p[3]&&field(cv.class,"e").getBoolean(renderer)==(p[9]!=0)&&
                field(cv.class,"f").getBoolean(renderer)==(p[2]!=0)&&field(cv.class,"b").getBoolean(renderer)&&
                field(cv.class,"O").getInt(renderer)==1,"unchanged scalar CV fields");
            for(String name:new String[]{"g","h","i","j","k","l"})require(field(cv.class,name).getInt(renderer)==p[8],"unchanged rotation");
            for(int i=0;i<6;++i)require(field(uu.class,BOUNDS[i]).getDouble(block)==box[i],"same live Block bounds");
            require(field(nw.class,"w").getBoolean(tessellator)&&field(nw.class,"r").getInt(tessellator)==7&&
                field(nw.class,"s").getDouble(tessellator)==-.25&&field(nw.class,"t").getDouble(tessellator)==.5&&
                field(nw.class,"u").getDouble(tessellator)==-.75,"same unfinished NW batch and translation");
            observe(index++,p,renderer,block,result,exception,before,tessellator);
            block.bm=priorTexture;field(uu.class,"bA").set(block,priorMaterial);
            for(int i=0;i<6;++i)field(uu.class,BOUNDS[i]).setDouble(block,priorBounds[i]);
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same captured Chunk identity");
        require(network==0&&nativeLinks==0,"no network/native GL calls");
        System.out.println("Original flat: "+index+" whole-body rows; network=0; native=0; draw=0");
    }
}
