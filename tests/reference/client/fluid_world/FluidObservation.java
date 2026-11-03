import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.Buffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;

/** Authored inputs; actual unchanged CV.k and genuine FD/LM/EW/NW/BT owners. */
public final class FluidObservation {
    static int active=-1,network,nativeLinks,deniedWrites,allowedWorldWrites,execs;
    static ew observedCache;static uu observedBlock;static nw observedTess;
    static Object observedPool;static uu[] observedRegistered;static ln[] observedMaterials;static String[] observedMaterialNames;
    static final IdentityHashMap<Object,Integer> vectorIds=new IdentityHashMap<>();
    static final String[] LIGHT={"n","o","p","q","r","s","t","u","v","w","x","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
    static final String[] COLORS={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final String[] FLAGS={"ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am"};
    static final float[][] BOX={{0,0,0,1,1,1},{.125f,.25f,.375f,.875f,.75f,.625f}};
    static final int[][] XYZ={{8,64,8},{8,64,8},{3,65,3}};
    static final double[][] TRANSLATION={{-.25,.5,-.75},{-8,-64,-8},{0,-64,0}};
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

    static String vector(bt v){Integer id=vectorIds.get(v);if(id==null){id=vectorIds.size();vectorIds.put(v,id);}return "[\"v"+id+"\",\""+d(v.a)+"\",\""+d(v.b)+"\",\""+d(v.c)+"\"]";}
    static String pool()throws Exception{
        Object actual=field(bt.class,"d").get(null);require(actual==observedPool,"same actual Vector List");List<?> list=(List<?>)actual;int cursor=field(bt.class,"e").getInt(null);require(cursor>=0&&cursor<=list.size(),"actual bounded Vector cursor");StringJoiner entries=new StringJoiner(",");for(Object value:list){require(value instanceof bt,"actual Vector List element");entries.add(vector((bt)value));}
        return "{\"cursor\":"+cursor+",\"size\":"+list.size()+",\"class\":\""+actual.getClass().getName()+"\",\"entries\":["+entries+"]}";
    }
    static void cell(lm c,int x,int y,int z,int id,int metadata){require(x>=0&&x<16&&y>=0&&y<128&&z>=0&&z<16,"authored local input cell");c.b[(x<<11)|(z<<7)|y]=(byte)id;c.e.a(x,y,z,metadata);}
    static void prepare(lm[] chunks,int[] p,int x,int y,int z){
        for(lm c:chunks){Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);Arrays.fill(c.f.a,(byte)0);Arrays.fill(c.g.a,(byte)0);}
        lm c=chunks[0];int id=p[0],metadata=p[1],layout=p[2];
        for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy)for(int dz=-2;dz<=2;++dz){c.f.a(x+dx,y+dy,z+dz,p[3]==1?15:0);c.g.a(x+dx,y+dy,z+dz,p[3]==2?15:0);}
        if(layout>=1&&layout<=7){for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz)cell(c,x+dx,y+dy,z+dz,id,metadata);
            if(layout>=2)for(int a=-1;a<=1;++a)for(int b=-1;b<=1;++b){int nx=x+a,ny=y+b,nz=z;if(layout==2||layout==3){ny=y+(layout==2?1:-1);nz=z+b;}else if(layout==4||layout==5){nz=z+(layout==4?-1:1);}else{nx=x+(layout==6?-1:1);nz=z+a;}cell(c,nx,ny,nz,0,0);}}
        else if(layout!=10&&layout!=13){for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz)cell(c,x+dx,y,z+dz,id,metadata);}
        cell(c,x,y,z,id,metadata);
        if(layout==8){cell(c,x-1,y,z-1,id,0);cell(c,x-1,y,z,id,1);cell(c,x,y,z-1,id,7);cell(c,x+1,y,z+1,id,8);}
        if(layout==9)cell(c,x-1,y+1,z-1,id,metadata);
        if(layout==10)cell(c,x,y-1,z,12,0);
        if(layout==11){cell(c,x-1,y,z,79,0);cell(c,x+1,y,z,0,0);}
        if(layout==12){cell(c,x-1,y,z,id,1);cell(c,x+1,y,z,id,7);}
        if(layout==13)cell(c,x+1,y-1,z,id,2);
        if(layout==14){cell(c,x,y,z-1,1,0);cell(c,x,y-1,z-1,1,0);}
        if(layout==15){cell(c,x+1,y,z,id,3);cell(c,x,y,z+1,id,6);cell(c,x,y+1,z-1,1,0);}
    }
    static String material(ln value){for(int i=0;i<observedMaterials.length;++i)if(observedMaterials[i]==value)return "material:"+observedMaterialNames[i];throw new AssertionError("actual named Material");}
    static String cells(lm c,int x,int y,int z){StringJoiner out=new StringJoiner(";");for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy)for(int dz=-2;dz<=2;++dz){int xx=x+dx,yy=y+dy,zz=z+dz;out.add(xx+","+yy+","+zz+","+(c.b[(xx<<11)|(zz<<7)|yy]&255)+","+c.e.a(xx,yy,zz)+","+c.f.a(xx,yy,zz)+","+c.g.a(xx,yy,zz));}return out.toString();}
    static String arrays(lm[] chunks){StringJoiner out=new StringJoiner(";");for(lm c:chunks)out.add(Arrays.hashCode(c.b)+","+Arrays.hashCode(c.e.a)+","+Arrays.hashCode(c.f.a)+","+Arrays.hashCode(c.g.a));return out.toString();}
    static void deny(Runnable call){try{call.run();throw new AssertionError("guard denial");}catch(SecurityException expected){}}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);require(args.length==5,"jar,world,profiles,first-global-index,cohort");Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[2]));int first=Integer.parseInt(args[3]),cohort=Integer.parseInt(args[4]);
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            public void checkLink(String name){++nativeLinks;throw new SecurityException("native denied");}
            public void checkExec(String name){++execs;throw new SecurityException("exec denied");}
            public void checkWrite(String path){if(Path.of(path).toAbsolutePath().normalize().startsWith(directory)){++allowedWorldWrites;return;}++deniedWrites;throw new SecurityException("outside private world denied");}
            public void checkDelete(String path){checkWrite(path);}});
        for(Class<?> type:new Class<?>[]{cv.class,ew.class,fd.class,lm.class,nw.class,uu.class,rp.class,ln.class,mx.class,bt.class,in.class})require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged source origin");
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};ew cache=new ew(world,0,0,0,31,127,31);field(fd.class,"f").setInt(world,3);
        nw tess=nw.a;tess.b();int[] constructed=(int[])field(nw.class,"g").get(tess),prior=new int[constructed.length];require(constructed.length==2097152,"actual constructor backing");
        // Genuine warm static owners are explicit prerequisites, not authored source state.
        observedPool=field(bt.class,"d").get(null);float warm=in.a(0.0f);require(Float.floatToRawIntBits(warm)==0,"actual warm MathHelper sine");
        observedCache=cache;observedTess=tess;observedRegistered=uu.m;
        List<Field> materials=new ArrayList<>();for(Field f:ln.class.getDeclaredFields())if(Modifier.isStatic(f.getModifiers())&&f.getType()==ln.class)materials.add(f);materials.sort(Comparator.comparing(Field::getName));observedMaterials=new ln[materials.size()];observedMaterialNames=new String[materials.size()];for(int i=0;i<materials.size();++i){Field f=materials.get(i);f.setAccessible(true);observedMaterials[i]=(ln)f.get(null);observedMaterialNames[i]=f.getName();}
        for(int id:new int[]{0,1,8,9,10,11,12,79}){uu b=uu.m[id];if(b==null){require(id==0,"only actual Air slot is NULL");System.out.println("FLUID_REGISTRY\t0\tNULL\t-1\t0\t"+material(ln.a)+"\t-1\t-1\tfalse");}else System.out.println("FLUID_REGISTRY\t"+id+"\t"+b.getClass().getName()+"\t"+b.bn+"\t"+b.bm+"\t"+material(b.bA)+"\t"+b.b()+"\t"+b.b_()+"\t"+b.c());}
        StringJoiner lightValues=new StringJoiner(",");for(float v:world.t.f)lightValues.add(f(v));require(world.t.f.length==16,"genuine Dimension light table");System.out.println("FLUID_LIGHT\t"+field(fd.class,"f").getInt(world)+"\t"+world.t.getClass().getName()+"\t"+lightValues);
        System.out.println("FLUID_STATIC\tcohort="+cohort+"\tNW.b="+field(nw.class,"b").getBoolean(null)+"\tNW.c="+field(nw.class,"c").getBoolean(null)+"\tpool="+pool());
        int count=0;long unchangedWords=0;
        for(String line:profiles){if(line.isBlank())continue;String[] columns=line.split(" ");require(columns.length==14,"name plus thirteen signed integers");int[] p=Arrays.stream(Arrays.copyOfRange(columns,1,columns.length)).mapToInt(Integer::parseInt).toArray();
            require(p[0]>=8&&p[0]<=11&&p[1]>=0&&p[1]<=15&&p[2]>=0&&p[2]<=15&&p[3]>=0&&p[3]<=2&&p[4]>=0&&p[4]<=1&&p[7]>=0&&p[7]<=1&&p[8]>=0&&p[8]<=2&&p[9]>=0&&p[9]<=1&&p[10]>=0&&p[10]<=2&&p[11]==0&&p[12]==0,"closed authored operands");
            int index=first+count;int[] xyz=XYZ[p[10]];int x=xyz[0],y=xyz[1],z=xyz[2];uu block=uu.m[p[0]];require(block instanceof rp&&block.bn==p[0],"genuine registered fluid");require(Path.of(block.getClass().getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"registered receiver source origin");observedBlock=block;
            int originalBm=block.bm;double[] oldBounds={block.bs,block.bt,block.bu,block.bv,block.bw,block.bx};block.bm=originalBm+p[6];float[] box=BOX[p[7]];block.a(box[0],box[1],box[2],box[3],box[4],box[5]);prepare(chunks,p,x,y,z);
            cv renderer=new cv(p[8]==1?null:cache);field(cv.class,"f").setBoolean(renderer,p[4]!=0);field(cv.class,"d").setInt(renderer,p[5]);if(p[9]!=0){field(cv.class,"m").setBoolean(renderer,true);field(cv.class,"e").setBoolean(renderer,true);for(String name:new String[]{"g","h","i","j","k","l"})field(cv.class,name).setInt(renderer,2);for(int i=0;i<LIGHT.length;++i)field(cv.class,LIGHT[i]).setFloat(renderer,(i+1)*.03125f);for(int i=0;i<COLORS.length;++i)field(cv.class,COLORS[i]).setFloat(renderer,(i+1)*.015625f);for(int i=0;i<FLAGS.length;++i)field(cv.class,FLAGS[i]).setBoolean(renderer,(i&1)!=0);}
            double[] translation=TRANSLATION[p[10]];tess.b(translation[0],translation[1],translation[2]);int before=field(nw.class,"o").getInt(tess);System.arraycopy(constructed,0,prior,0,constructed.length);Object[] refs=references(tess);String bufferBefore=buffers(refs),cvBefore=primitive(renderer,cv.class),boundsBefore=bounds(block),nwBefore=primitive(tess,nw.class),poolBefore=pool(),enteredCells=cells(chunks[0],x,y,z),enteredArrays=arrays(chunks);int enteredBm=block.bm,renderPass=block.b_();
            String result="UNRETURNED",exception="NONE";lm.a=false;active=index;
            try{result=Boolean.toString(renderer.k(p[8]==2?null:block,x,y,z));}catch(NullPointerException failure){exception="NULL";}finally{active=-1;}
            int after=field(nw.class,"o").getInt(tess);require(before<=after&&after<constructed.length,"bounded observed writes");long untouched=0;
            for(int i=0;i<before;++i){if(prior[i]!=constructed[i])throw new AssertionError("actual prior emitted word preserved");++untouched;}for(int i=after;i<constructed.length;++i){if(prior[i]!=constructed[i])throw new AssertionError("actual unused word preserved");++untouched;}
            Object[] afterRefs=references(tess);for(int i=0;i<refs.length;++i)require(refs[i]==afterRefs[i],"same NW owner reference");require(bufferBefore.equals(buffers(afterRefs))&&cvBefore.equals(primitive(renderer,cv.class))&&field(cv.class,"c").get(renderer)==(p[8]==1?null:cache)&&block.bm==enteredBm&&enteredArrays.equals(arrays(chunks)),"retained CV/cache/bm/buffer/input-array state");
            StringJoiner operands=new StringJoiner(",");for(int v:p)operands.add(Integer.toString(v));System.out.println("FLUID_INPUT\t"+index+"\t"+block.getClass().getName()+"\t"+block.bn+"\t"+originalBm+"\t"+enteredBm+"\t"+operands+"\t"+x+","+y+","+z+"\t"+renderPass);System.out.println("FLUID_CELLS\t"+index+"\t"+enteredCells+"\t"+enteredArrays);
            StringJoiner words=new StringJoiner(",");int extent=Math.max(after,before+100);require(extent<=constructed.length,"observed tail extent");for(int i=before;i<extent;++i)words.add(String.format(Locale.ROOT,"%08x",constructed[i]));
            System.out.println("FLUID\t"+index+"\t"+columns[0]+"\t"+result+"\t"+exception+"\t"+before+"\t"+after+"\t"+constructed.length+"\t"+(lm.a?1:0)+"\t"+translations(tess)+"\t"+boundsBefore+"\t"+bounds(block)+"\t"+cvBefore+"\t"+nwBefore+"\t"+primitive(tess,nw.class)+"\t"+words);
            System.out.println("FLUID_POOL\t"+index+"\t"+poolBefore+"\t"+pool());System.out.println("FLUID_BACKING\t"+index+"\t"+untouched+"\t"+Arrays.hashCode(prior)+"\t"+Arrays.hashCode(constructed)+"\t"+bufferBefore);unchangedWords+=untouched;
            block.bm=originalBm;String[] boundNames={"bs","bt","bu","bv","bw","bx"};for(int i=0;i<6;++i)field(uu.class,boundNames[i]).setDouble(block,oldBounds[i]);++count;
        }
        for(int i=0;i<chunks.length;++i)require(world.c(i/2,i%2)==chunks[i],"same canonical World Chunks");require(network==0&&nativeLinks==0&&deniedWrites==0&&execs==0,"no source external attempts");
        System.out.println("FLUID_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes="+allowedWorldWrites);
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));deny(()->System.getSecurityManager().checkLink("fluid-denial"));deny(()->System.getSecurityManager().checkExec("fluid-denial"));deny(()->System.getSecurityManager().checkWrite(directory.resolveSibling("fluid-denial").toString()));
        require(network==2&&nativeLinks==1&&deniedWrites==1&&execs==1,"five denial controls");System.out.println("FLUID_GUARD controls network=2 native=1 denied-writes=1 execs=1");
        System.out.println("FLUID_COMPLETE cohort="+cohort+" rows="+count+" first="+first+" own-unchanged-words="+unchangedWords+" genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0");
    }
}
