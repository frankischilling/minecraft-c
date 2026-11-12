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

/** Declared data inputs; unchanged original Crops wrapper and primitive bodies. */
public final class CropsObservation {
    static int active=-1,network,nativeLinks,deniedWrites,allowedWorldWrites,execs;
    static ew observedCache;static uu observedBlock;static nw observedTess;static int[] observedEmissions;
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] COLORS={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final float[][] BOX={{0,0,0,1,1,1},{.125f,.25f,.375f,.875f,.75f,.625f},{-.25f,-.5f,-.125f,1.5f,1.25f,1.75f}};
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

    static final int[][] XYZ={{8,64,8},{8,64,8},{3,65,3},{8,16777217,8},{8,-16777217,8},{8,Integer.MAX_VALUE,8},{8,Integer.MIN_VALUE,8}};
    static final double[][] TRANSLATION={{-.25,.5,-.75},{-8,-64,-8},{0,-64,0}};
    static void prepare(lm[] chunks,int metadata,int light,int[] xyz){
        for(lm c:chunks){Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);Arrays.fill(c.f.a,(byte)0);Arrays.fill(c.g.a,(byte)0);}
        lm c=chunks[0];int x=xyz[0],y=xyz[1]>=1&&xyz[1]<127?xyz[1]:64,z=xyz[2];
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz){c.f.a(x+dx,y+dy,z+dz,light==1?15:0);c.g.a(x+dx,y+dy,z+dz,light==2?15:0);}
        c.b[(x<<11)|(z<<7)|y]=(byte)59;c.e.a(x,y,z,metadata);
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
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,ni.class,wb.class,ln.class,mx.class})require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged source origin");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};ew cache=new ew(world,0,0,0,31,127,31);field(fd.class,"f").setInt(world,3);
        nw tess=nw.a;tess.b();int[] constructed=(int[])field(nw.class,"g").get(tess),prior=new int[constructed.length];
        require(constructed.length==2097152,"actual constructor backing");observedCache=cache;observedTess=tess;observedEmissions=uu.s;
        require(Modifier.isStatic(field(uu.class,"s").getModifiers())&&Modifier.isFinal(field(uu.class,"s").getModifiers()),"actual final emission array");
        System.out.println("CROPS_STATIC\tNW.b="+field(nw.class,"b").getBoolean(null)+"\tNW.c="+field(nw.class,"c").getBoolean(null)+"\tUU.s.length="+uu.s.length+"\tUU.s.final=true");
        StringJoiner lightValues=new StringJoiner(",");for(float v:world.t.f)lightValues.add(f(v));System.out.println("CROPS_LIGHT\t3\t"+lightValues);
        int index=0;long unchangedWords=0;
        for(String line:profiles){if(line.isBlank())continue;String[] columns=line.split(" ");require(columns.length==14,"name plus thirteen signed integers");int[] p=Arrays.stream(Arrays.copyOfRange(columns,1,columns.length)).mapToInt(Integer::parseInt).toArray();
            require((p[12]==1||p[0]>=0&&p[0]<=15)&&p[1]>=0&&p[1]<=2&&p[4]>=0&&p[4]<=2&&p[5]>=0&&p[5]<=3&&p[6]>=0&&p[6]<=1&&p[8]>=0&&p[8]<=6&&p[9]>=0&&p[9]<=1&&p[10]>=0&&p[10]<=1&&p[11]>=0&&p[11]<=3&&p[12]>=0&&p[12]<=1,"closed authored operands");
            uu block=uu.m[59];require(block instanceof ni&&block.bn==59&&block.b()==6&&block.b_()==0,"genuine registered Crops");observedBlock=block;
            int originalBm=block.bm,originalEmission=uu.s[59];block.bm=originalBm+p[3];if(p[7]!=Integer.MIN_VALUE)uu.s[59]=p[7];
            int[] xyz=XYZ[p[8]];prepare(chunks,p[0],p[1],xyz);float[] box=BOX[p[4]];block.a(box[0],box[1],box[2],box[3],box[4],box[5]);
            cv renderer=new cv(p[5]==1||p[5]==3?null:cache);field(cv.class,"m").setBoolean(renderer,p[6]!=0);field(cv.class,"f").setBoolean(renderer,p[9]!=0);field(cv.class,"d").setInt(renderer,p[2]);field(cv.class,"e").setBoolean(renderer,p[10]!=0);
            for(String name:new String[]{"g","h","i","j","k","l"})field(cv.class,name).setInt(renderer,p[11]);
            for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*.03125f);for(int i=0;i<COLORS.length;++i)field(cv.class,COLORS[i]).setFloat(renderer,(i+1)*.015625f);for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);
            double[] translation=TRANSLATION[p[8]<=2?p[8]:0];tess.b(translation[0],translation[1],translation[2]);int before=field(nw.class,"o").getInt(tess);System.arraycopy(constructed,0,prior,0,constructed.length);Object[] refs=references(tess);String bufferBefore=buffers(refs),cvBefore=primitive(renderer,cv.class),boundsBefore=bounds(block),nwBefore=primitive(tess,nw.class);int[] emissionBefore=uu.s.clone();
            int enteredBm=block.bm,enteredEmission=uu.s[59];String result="UNRETURNED",exception="NONE";lm.a=false;active=index;
            try{if(p[12]==0)result=Boolean.toString(renderer.j(p[5]==2||p[5]==3?null:block,xyz[0],xyz[1],xyz[2]));else{renderer.b(p[5]==2||p[5]==3?null:block,p[0],(double)xyz[0]+.125,(double)xyz[1]+.375,(double)xyz[2]-.25);result="VOID";}}catch(NullPointerException failure){exception="NULL";}finally{active=-1;}
            int after=field(nw.class,"o").getInt(tess);require(before<=after&&after<constructed.length,"bounded source writes");long untouched=0;for(int i=0;i<before;++i){require(prior[i]==constructed[i],"prior emitted word");++untouched;}for(int i=after;i<constructed.length;++i){require(prior[i]==constructed[i],"own inactive word");++untouched;}
            Object[] afterRefs=references(tess);for(int i=0;i<refs.length;++i)require(refs[i]==afterRefs[i],"same NW references");require(bufferBefore.equals(buffers(afterRefs))&&cvBefore.equals(primitive(renderer,cv.class))&&boundsBefore.equals(bounds(block))&&Arrays.equals(emissionBefore,uu.s)&&block.bm==enteredBm,"retained CV/bounds/emission/bm/buffers");
            StringJoiner operands=new StringJoiner(",");for(int v:p)operands.add(Integer.toString(v));System.out.println("CROPS_INPUT\t"+index+"\t59\t"+originalBm+"\t"+p[3]+"\t"+enteredBm+"\t"+originalEmission+"\t"+enteredEmission+"\t"+operands+"\t"+xyz[0]+","+xyz[1]+","+xyz[2]+"\t0");
            StringJoiner words=new StringJoiner(",");int extent=Math.max(after,before+384);require(extent<=constructed.length,"tail bound");for(int i=before;i<extent;++i)words.add(String.format(Locale.ROOT,"%08x",constructed[i]));
            System.out.println("CROPS\t"+index+"\t"+columns[0]+"\t"+p[0]+"\t"+result+"\t"+exception+"\t"+before+"\t"+after+"\t"+constructed.length+"\t"+(lm.a?1:0)+"\t"+translations(tess)+"\t"+boundsBefore+"\t"+cvBefore+"\t"+nwBefore+"\t"+primitive(tess,nw.class)+"\t"+words);
            System.out.println("CROPS_BACKING\t"+index+"\t"+untouched+"\t"+Arrays.hashCode(prior)+"\t"+Arrays.hashCode(constructed)+"\t"+bufferBefore);unchangedWords+=untouched;block.bm=originalBm;uu.s[59]=originalEmission;++index;
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same canonical World Chunks");require(network==0&&nativeLinks==0&&deniedWrites==0&&execs==0,"no source external attempts");
        System.out.println("CROPS_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes="+allowedWorldWrites);
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));deny(()->System.getSecurityManager().checkLink("crops-denial"));deny(()->System.getSecurityManager().checkExec("crops-denial"));deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("crops-denial").toString()));
        require(network==2&&nativeLinks==1&&deniedWrites==1&&execs==1,"five denial controls");System.out.println("CROPS_GUARD controls network=2 native=1 denied-writes=1 execs=1");System.out.println("CROPS_COMPLETE rows="+index+" own-unchanged-words="+unchangedWords+" genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0");
    }
}
