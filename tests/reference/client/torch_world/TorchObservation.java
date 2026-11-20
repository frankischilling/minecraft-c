import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.Buffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.StringJoiner;

/** Declared data inputs; unchanged original Torch wrapper and primitive bodies. */
public final class TorchObservation {
    static int active=-1,network,nativeLinks,deniedWrites,allowedWorldWrites,execs;
    static ew observedCache;static uu observedBlock;static nw observedTess;static int[] observedEmissions;
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] COLORS={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final float[][] BOX={{0,0,0,1,1,1},{.125f,.25f,.375f,.875f,.75f,.625f},{-.25f,-.5f,-.125f,1.5f,1.25f,1.75f}};
    static final double[][] GEOMETRY={{8,64,8,0,0},{-.5,-.5,-.5,0,0},{-0.0,-0.0,-0.0,-0.0,0.0},{1.25,-2.5,3.75,(double).4f,-(double).4f}};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String primitive(Object receiver,Class<?> type)throws Exception{
        Field[] fields=type.getDeclaredFields();Arrays.sort(fields,Comparator.comparing(Field::getName));StringJoiner out=new StringJoiner(",");
        for(Field field:fields)if(!Modifier.isStatic(field.getModifiers())&&field.getType().isPrimitive()){
            field.setAccessible(true);Object v=field.get(receiver);out.add(field.getName()+"="+(v instanceof Float?f((float)v):v instanceof Double?d((double)v):v.toString()));}
        return out.toString();
    }
    static Object[] references(nw tess)throws Exception{
        return new Object[]{field(nw.class,"d").get(tess),field(nw.class,"e").get(tess),field(nw.class,"f").get(tess),field(nw.class,"g").get(tess),field(nw.class,"y").get(tess)};
    }
    static String buffers(Object[] values){StringJoiner out=new StringJoiner(",");for(Object v:values){if(v==null)out.add("NULL");else if(v instanceof Buffer){Buffer b=(Buffer)v;out.add(v.getClass().getName()+":"+b.position()+":"+b.limit()+":"+b.capacity());}else if(v instanceof int[])out.add("int[]:"+((int[])v).length);else throw new AssertionError("unexpected NW reference");}return out.toString();}
    static String translations(nw tess)throws Exception{return d(field(nw.class,"s").getDouble(tess))+","+d(field(nw.class,"t").getDouble(tess))+","+d(field(nw.class,"u").getDouble(tess));}
    static String bounds(uu b){return d(b.bs)+","+d(b.bt)+","+d(b.bu)+","+d(b.bv)+","+d(b.bw)+","+d(b.bx);}
    static void prepare(lm chunk,int id,int metadata,int light){
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);
        for(int x=7;x<=9;++x)for(int y=63;y<=65;++y)for(int z=7;z<=9;++z){chunk.f.a(x,y,z,light==1?15:0);chunk.g.a(x,y,z,light==2?15:0);}
        chunk.b[(8<<11)|(8<<7)|64]=(byte)id;chunk.e.a(8,64,8,metadata);
    }
    static void deny(Runnable call){try{call.run();throw new AssertionError("guard denial");}catch(SecurityException expected){}}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);require(args.length==3,"jar,world,profiles");Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            public void checkLink(String name){++nativeLinks;throw new SecurityException("native denied");}
            public void checkExec(String name){++execs;throw new SecurityException("exec denied");}
            public void checkWrite(String path){if(Path.of(path).toAbsolutePath().normalize().startsWith(directory)){++allowedWorldWrites;return;}++deniedWrites;throw new SecurityException("outside private world denied");}
            public void checkDelete(String path){checkWrite(path);}});
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,vm.class,db.class,ln.class,mx.class})require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged source origin");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};ew cache=new ew(world,0,0,0,31,127,31);field(fd.class,"f").setInt(world,3);
        nw tess=nw.a;tess.b();tess.b(-.25,.5,-.75);int[] constructed=(int[])field(nw.class,"g").get(tess),prior=new int[constructed.length];
        require(constructed.length==2097152,"actual constructor backing");observedCache=cache;observedTess=tess;observedEmissions=uu.s;
        require(Modifier.isStatic(field(uu.class,"s").getModifiers())&&Modifier.isFinal(field(uu.class,"s").getModifiers()),"actual static final array authoring boundary");
        System.out.println("TORCH_STATIC\tNW.b="+field(nw.class,"b").getBoolean(null)+"\tNW.c="+field(nw.class,"c").getBoolean(null)+"\tUU.s.length="+uu.s.length+"\tUU.s.final=true");
        int index=0;long unchangedWords=0;
        for(String line:profiles){if(line.isBlank())continue;String[] columns=line.split(" ");require(columns.length==14,"name plus thirteen signed integers");int[] p=Arrays.stream(Arrays.copyOfRange(columns,1,columns.length)).mapToInt(Integer::parseInt).toArray();
            require(p[0]>=0&&p[0]<=1&&(p[1]==50||p[1]==75||p[1]==76)&&p[2]>=0&&p[2]<=15&&p[3]>=0&&p[3]<=2&&p[7]>=0&&p[7]<=4,"closed authored operands");
            uu block=uu.m[p[1]];require(block instanceof vm&&block.bn==p[1]&&(p[1]==50||block instanceof db),"genuine registered Torch");observedBlock=block;
            int originalBm=block.bm,originalEmission=uu.s[block.bn];ln originalMaterial=block.bA;block.bm=originalBm+p[5];if(p[4]!=Integer.MIN_VALUE)uu.s[block.bn]=p[4];if(p[7]==3)field(uu.class,"bA").set(block,null);
            prepare(chunks[0],p[1],p[2],p[3]);float[] box=BOX[p[8]];block.a(box[0],box[1],box[2],box[3],box[4],box[5]);
            cv renderer=new cv(p[7]==1||p[7]==4?null:cache);field(cv.class,"m").setBoolean(renderer,p[11]!=0);field(cv.class,"f").setBoolean(renderer,(index&1)!=0);field(cv.class,"d").setInt(renderer,p[6]);field(cv.class,"e").setBoolean(renderer,p[10]!=0);
            for(String name:new String[]{"g","h","i","j","k","l"})field(cv.class,name).setInt(renderer,p[9]);
            for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*.03125f);for(int i=0;i<COLORS.length;++i)field(cv.class,COLORS[i]).setFloat(renderer,(i+1)*.015625f);for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);
            int before=field(nw.class,"o").getInt(tess);System.arraycopy(constructed,0,prior,0,constructed.length);Object[] refs=references(tess);String bufferBefore=buffers(refs),cvBefore=primitive(renderer,cv.class),boundsBefore=bounds(block),nwBefore=primitive(tess,nw.class);int[] emissionBefore=uu.s.clone();
            int enteredBm=block.bm,enteredEmission=uu.s[block.bn];String result="UNRETURNED",exception="NONE";lm.a=false;active=index;double[] geometry=GEOMETRY[p[12]];
            try{uu operand=p[7]==2||p[7]==4?null:block;if(p[0]==0)result=Boolean.toString(renderer.c(operand,8,64,8));else{renderer.a(operand,geometry[0],geometry[1],geometry[2],geometry[3],geometry[4]);result="VOID";}}
            catch(NullPointerException failure){exception="NULL";}catch(ArrayIndexOutOfBoundsException failure){exception="INDEX";}finally{active=-1;}
            int after=field(nw.class,"o").getInt(tess);require(before<=after&&after<constructed.length,"bounded observed writes");long untouched=0;
            for(int i=0;i<before;++i){if(prior[i]!=constructed[i])throw new AssertionError("actual prior emitted word preserved");++untouched;}for(int i=after;i<constructed.length;++i){if(prior[i]!=constructed[i])throw new AssertionError("actual unused word preserved");++untouched;}
            Object[] afterRefs=references(tess);for(int i=0;i<refs.length;++i)require(refs[i]==afterRefs[i],"same NW owner reference");
            require(bufferBefore.equals(buffers(afterRefs))&&cvBefore.equals(primitive(renderer,cv.class))&&boundsBefore.equals(bounds(block))&&field(cv.class,"c").get(renderer)==(p[7]==1||p[7]==4?null:cache),"retained actual CV/Block/cache/buffer state");
            require(uu.s==observedEmissions&&Arrays.equals(uu.s,emissionBefore)&&block.bm==enteredBm&&block.bA==(p[7]==3?null:originalMaterial),"same actual static array and declared data");
            System.out.println("TORCH_INPUT\t"+index+"\t"+block.getClass().getName()+"\t"+block.bn+"\t"+originalBm+"\t"+p[5]+"\t"+enteredBm+"\t"+originalEmission+"\t"+enteredEmission+"\t"+p[2]+"\t"+p[3]);
            StringJoiner words=new StringJoiner(",");int extent=Math.max(after,before+100);require(extent<=constructed.length,"observed tail extent");for(int i=before;i<extent;++i)words.add(String.format(Locale.ROOT,"%08x",constructed[i]));
            System.out.println("TORCH\t"+index+"\t"+columns[0]+"\t"+p[0]+"\t"+result+"\t"+exception+"\t"+before+"\t"+after+"\t"+constructed.length+"\t"+(lm.a?1:0)+"\t"+translations(tess)+"\t"+boundsBefore+"\t"+cvBefore+"\t"+nwBefore+"\t"+primitive(tess,nw.class)+"\t"+words);
            System.out.println("TORCH_BACKING\t"+index+"\t"+untouched+"\t"+Arrays.hashCode(prior)+"\t"+Arrays.hashCode(constructed)+"\t"+bufferBefore);unchangedWords+=untouched;
            uu.s[block.bn]=originalEmission;block.bm=originalBm;field(uu.class,"bA").set(block,originalMaterial);++index;
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same canonical World Chunks");require(network==0&&nativeLinks==0&&deniedWrites==0&&execs==0,"no source external attempts");
        System.out.println("TORCH_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes="+allowedWorldWrites);
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));deny(()->System.getSecurityManager().checkLink("torch-denial"));deny(()->System.getSecurityManager().checkExec("torch-denial"));deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("torch-denial").toString()));
        require(network==2&&nativeLinks==1&&deniedWrites==1&&execs==1,"five denial controls");System.out.println("TORCH_GUARD controls network=2 native=1 denied-writes=1 execs=1");
        System.out.println("TORCH_COMPLETE rows="+index+" own-unchanged-words="+unchangedWords+" genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0");
    }
}
