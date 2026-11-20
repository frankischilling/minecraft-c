import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.StringJoiner;

/** Unchanged m/c calls on genuine constructed FD/LM/EW/CV/NW owners. */
public final class CactusObservation {
    static int active=-1, network, nativeLinks, deniedWrites, allowedWorldWrites, execs;
    static ew observedCache; static uu observedBlock; static nw observedTess;
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] COLORS={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final float[][] BOX={{0,0,0,1,1,1},{.125f,.25f,.375f,.875f,.75f,.625f},
        {-.25f,-.5f,-.125f,1.5f,1.25f,1.75f},{0,0,0,1,.5f,1},
        {Float.NaN,0,0,1,1,1},{0,Float.NaN,0,1,1,1},{0,0,Float.NaN,1,1,1},
        {0,0,0,Float.NaN,1,1},{0,0,0,1,Float.NaN,1},{0,0,0,1,1,Float.NaN},
        {-0.0f,-0.0f,-0.0f,1,1,1}};
    static final float[][] RGB={{.3f,.6f,.9f},{-.25f,1.5f,2f},{0,0,0}};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String primitive(Object receiver,Class<?> type)throws Exception{
        Field[] fields=type.getDeclaredFields();Arrays.sort(fields,Comparator.comparing(Field::getName));StringJoiner out=new StringJoiner(",");
        for(Field field:fields)if(!Modifier.isStatic(field.getModifiers())&&field.getType().isPrimitive()){
            field.setAccessible(true);Object v=field.get(receiver);
            out.add(field.getName()+"="+(v instanceof Float?f((float)v):v instanceof Double?d((double)v):v.toString()));}
        return out.toString();
    }
    static String translations(nw tess)throws Exception{return d(field(nw.class,"s").getDouble(tess))+","+d(field(nw.class,"t").getDouble(tess))+","+d(field(nw.class,"u").getDouble(tess));}
    static String bounds(uu block){return d(block.bs)+","+d(block.bt)+","+d(block.bu)+","+d(block.bv)+","+d(block.bw)+","+d(block.bx);}
    static void prepare(lm chunk,int pattern){
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);
        for(int x=7;x<=9;++x)for(int y=63;y<=65;++y)for(int z=7;z<=9;++z){
            int id=pattern==0?0:pattern==1?1:((x+2*y+4*z)&2)!=0?1:0;
            chunk.b[(x<<11)|(z<<7)|y]=(byte)id;
            chunk.e.a(x,y,z,(x+y+z)&3);chunk.f.a(x,y,z,(x*3+y*5+z*7)&15);chunk.g.a(x,y,z,(x*7+y*11+z*13)&15);}
        chunk.b[(8<<11)|(8<<7)|64]=(byte)81;
    }
    static void deny(Runnable call){try{call.run();throw new AssertionError("guard denial");}catch(SecurityException expected){}}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);require(args.length==3,"jar, authored world, profiles");
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            public void checkLink(String name){++nativeLinks;throw new SecurityException("native denied");}
            public void checkExec(String name){++execs;throw new SecurityException("exec denied");}
            public void checkWrite(String path){if(Path.of(path).toAbsolutePath().normalize().startsWith(directory)){++allowedWorldWrites;return;}++deniedWrites;throw new SecurityException("outside private world denied");}});
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,or.class,ln.class,px.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged source origin");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};ew cache=new ew(world,0,0,0,31,127,31);
        field(fd.class,"f").setInt(world,3);
        nw tess=nw.a;tess.b();tess.b(-.25,.5,-.75);int[] constructed=(int[])field(nw.class,"g").get(tess);
        require(constructed.length==2097152,"genuine original constructor backing");
        uu block=uu.m[81];require(block instanceof or,"actual registered Cactus");
        observedCache=cache;observedBlock=block;observedTess=tess;
        int originalBm=block.bm,index=0;
        for(String line:profiles){
            if(line.isBlank())continue;String[] cols=line.split(" ");require(cols.length==14,"profile width");
            int[] p=Arrays.stream(Arrays.copyOfRange(cols,1,cols.length)).mapToInt(Integer::parseInt).toArray();
            require(p[11]==0,"ordinary constructor backing only");
            prepare(chunks[0],p[1]);block.bm=originalBm+p[12];float[] box=BOX[p[3]];block.a(box[0],box[1],box[2],box[3],box[4],box[5]);
            ln savedMaterial=block.bA;if(p[5]==3)field(uu.class,"bA").set(block,null);
            cv renderer=new cv(p[5]==1?null:cache);field(cv.class,"m").setBoolean(renderer,p[4]!=0);
            field(cv.class,"f").setBoolean(renderer,p[2]!=0);field(cv.class,"d").setInt(renderer,p[8]);field(cv.class,"e").setBoolean(renderer,p[7]!=0);
            for(String name:new String[]{"g","h","i","j","k","l"})field(cv.class,name).setInt(renderer,p[6]);
            for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*.03125f);
            for(int i=0;i<COLORS.length;++i)field(cv.class,COLORS[i]).setFloat(renderer,(i+1)*.015625f);
            for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);
            field(px.class,"a").setBoolean(null,p[10]!=0);
            int before=field(nw.class,"o").getInt(tess);String cvBefore=primitive(renderer,cv.class),nwBefore=primitive(tess,nw.class),boundsBefore=bounds(block);
            int[] enteredBacking=(int[])field(nw.class,"g").get(tess);int capacity=enteredBacking.length;
            int result=-1;String exception="NONE";lm.a=false;active=index;
            float[] rgb=RGB[p[9]];
            try{uu operand=p[5]==2?null:block;result=(p[0]==0?renderer.c(operand,8,64,8,rgb[0],rgb[1],rgb[2]):renderer.m(operand,8,64,8))?1:0;}
            catch(NullPointerException failure){exception="NULL";}
            catch(ArrayIndexOutOfBoundsException failure){exception="INDEX";}
            finally{active=-1;}
            require(cvBefore.equals(primitive(renderer,cv.class))&&boundsBefore.equals(bounds(block)),"same actual CV fields/bounds");
            require(field(cv.class,"c").get(renderer)==(p[5]==1?null:cache)&&field(nw.class,"g").get(tess)==enteredBacking,"same source operands/backing");
            int end=field(nw.class,"o").getInt(tess),extent=Math.min(capacity,Math.max(end,before+100));
            System.out.println("CACTUS_INPUT\t"+index+"\t"+originalBm+"\t"+p[12]+"\t"+block.bm);
            StringJoiner words=new StringJoiner(",");for(int i=before;i<extent;++i)words.add(String.format(Locale.ROOT,"%08x",enteredBacking[i]));
            System.out.println("CACTUS\t"+index+"\t"+cols[0]+"\t"+p[0]+"\t"+result+"\t"+exception+"\t"+before+"\t"+end+"\t"+capacity+"\t"+(lm.a?1:0)+"\t"+translations(tess)+"\t"+boundsBefore+"\t"+cvBefore+"\t"+nwBefore+"\t"+primitive(tess,nw.class)+"\t"+words);
            field(uu.class,"bA").set(block,savedMaterial);block.bm=originalBm;++index;
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same canonical World/cache Chunks");
        require(network==0&&nativeLinks==0&&deniedWrites==0&&execs==0,"source external attempts");
        System.out.println("CACTUS_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes="+allowedWorldWrites);
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));
        deny(()->System.getSecurityManager().checkLink("cactus-denial"));deny(()->System.getSecurityManager().checkExec("cactus-denial"));
        deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("cactus-denial").toString()));
        require(network==2&&nativeLinks==1&&deniedWrites==1&&execs==1,"five denial controls");
        System.out.println("CACTUS_GUARD controls network=2 native=1 denied-writes=1 execs=1");
        System.out.println("CACTUS_COMPLETE rows="+index+" malformed=0"+" draw=0 raw-allocations=0 source-replacements=0");
    }
}
