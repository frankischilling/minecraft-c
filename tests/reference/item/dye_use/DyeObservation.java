import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.client.Minecraft;

/** Declared operands and observers around unchanged original Dye calls. */
public final class DyeObservation {
    static int active=-1,network,natives,execs,deniedWrites,worldWrites;
    static fd observedWorld;static iz observedStack;static Random itemRandom;
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static void need(boolean v,String t){if(!v)throw new AssertionError(t);}
    static Map<String,Object> map(Object... p){Map<String,Object> m=new LinkedHashMap<>();for(int i=0;i<p.length;i+=2)m.put((String)p[i],p[i+1]);return m;}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String)return "\""+((String)v).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";
        if(v instanceof Map){StringJoiner j=new StringJoiner(",","{","}");for(Object x:((Map<?,?>)v).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)x;j.add(json(e.getKey())+":"+json(e.getValue()));}return j.toString();}
        if(v instanceof Iterable){StringJoiner j=new StringJoiner(",","[","]");for(Object x:(Iterable<?>)v)j.add(json(x));return j.toString();}
        return v.toString();
    }
    static Object random(Random r)throws Exception{return map("seed_bits",String.format(Locale.ROOT,"%012x",((AtomicLong)field(Random.class,"seed").get(r)).get()),"gaussian_bits",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(r))),"have_gaussian",field(Random.class,"haveNextNextGaussian").getBoolean(r));}
    static List<Integer> stack(iz s){return List.of(s.a,s.b,s.c,s.i());}
    static byte[][] arrays(lm[] chunks){byte[][] a=new byte[chunks.length*5][];for(int i=0;i<chunks.length;i++){a[i*5]=chunks[i].b.clone();a[i*5+1]=chunks[i].e.a.clone();a[i*5+2]=chunks[i].f.a.clone();a[i*5+3]=chunks[i].g.a.clone();a[i*5+4]=chunks[i].h.clone();}return a;}
    static List<Object> changes(byte[][] a,byte[][] b){List<Object> r=new ArrayList<>();for(int i=0;i<a.length;i++)for(int n=0;n<a[i].length;n++)if(a[i][n]!=b[i][n])r.add(List.of(i/5,i%5,n,a[i][n]&255,b[i][n]&255));return r;}
    static void put(lm[] chunks,int x,int y,int z,int id,int metadata){if(x<0||x>31||z<0||z>31||y<0||y>127)return;lm c=chunks[(x>>4)*2+(z>>4)];int index=((x&15)<<11)|((z&15)<<7)|y;c.b[index]=(byte)id;int shift=(index&1)*4;c.e.a[index>>1]=(byte)(((c.e.a[index>>1]&255)&~(15<<shift))|((metadata&15)<<shift));}
    static List<Object> cells(lm[] chunks){List<Object> r=new ArrayList<>();for(int x=0;x<32;x++)for(int z=0;z<32;z++)for(int y=0;y<128;y++){lm c=chunks[(x>>4)*2+(z>>4)];int n=((x&15)<<11)|((z&15)<<7)|y,id=c.b[n]&255;if(id!=0)r.add(List.of(x,y,z,id,((c.e.a[n>>1]&255)>>((n&1)*4))&15));}return r;}
    static final class Observer implements pm {
        final List<Object> notices=new ArrayList<>();int mode,cells;boolean switched;
        void changed(int x,int y,int z){
            cells++;Object a,b;try{a=random(itemRandom);b=random(observedWorld.r);}catch(Exception e){throw new AssertionError(e);}
            notices.add(map("event","cell","position",List.of(x,y,z),"stack",stack(observedStack),"item_random",a,"world_random",b));
            if(mode==2&&!switched){try{field(gm.class,"b").set(null,observedWorld.r);}catch(Exception e){throw new AssertionError(e);}switched=true;notices.add(map("event","gm_b_alias_world","after_cell",cells));}
            if(mode==4&&cells==1){observedStack.a=99;notices.add(map("event","authored_count_store","value",99));}
            if(mode==1&&cells==1||mode==3&&cells==3)throw new IllegalStateException("declared Dye cell observer failure");
        }
        public void a(int x,int y,int z){changed(x,y,z);}
        public void b(int x,int y,int z,int a,int b,int c){notices.add(map("event","range","coordinates",List.of(x,y,z,a,b,c)));}
        public void a(String n,double x,double y,double z,float v,float p){notices.add(map("event","sound","name",n));}
        public void a(String n,double x,double y,double z,double a,double b,double c){notices.add(map("event","particle","name",n));}
        public void a(sn e){notices.add(map("event","add","class",e.getClass().getName()));}
        public void b(sn e){notices.add(map("event","remove","class",e.getClass().getName()));}
        public void e(){notices.add(map("event","refresh"));}
        public void a(String n,int x,int y,int z){notices.add(map("event","record","name",n));}
        public void a(int x,int y,int z,ow t){notices.add(map("event","tile","class",t==null?"null":t.getClass().getName()));}
        public void a(gs p,int a,int b,int c,int d,int e){notices.add(map("event","level","values",List.of(a,b,c,d,e)));}
    }
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);need(args.length==3,"jar,privateworld,profiles");Path jar=Path.of(args[0]).toRealPath(),dir=Path.of(args[1]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){network++;throw new SecurityException("network");}public void checkListen(int p){network++;throw new SecurityException("listen");}
            public void checkLink(String n){natives++;throw new SecurityException("native");}public void checkExec(String n){execs++;throw new SecurityException("exec");}
            public void checkWrite(String p){if(Path.of(p).toAbsolutePath().normalize().startsWith(dir)){worldWrites++;return;}deniedWrites++;throw new SecurityException("outside privateworld");}public void checkDelete(String p){checkWrite(p);}});
        for(Class<?> t:new Class<?>[]{km.class,gm.class,iz.class,fd.class,lm.class,dc.class,gs.class,he.class,ni.class,uu.class,xi.class,jl.class})need(Path.of(t.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original class origin");
        kq app=new kq(null,null,null,854,480,false,null);
        try{
            app.k=new gr("DyeReference","-");field(Minecraft.class,"Z").set(app,dir.toFile());xi stats=new xi(app.k,dir.toFile());app.I=stats;
            fd world=new fd(new mx(dir.getParent().toFile(),dir.getFileName().toString(),false),"",23L);observedWorld=world;
            lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};dc player=new dc(app,world,app.k,0);app.f=world;app.h=player;
            gm item=gm.c[351];need(item.getClass()==km.class,"actual registered Dye");itemRandom=(Random)field(gm.class,"b").get(null);need(itemRandom!=world.r,"distinct actual random owners");
            Observer observer=new Observer();world.a(observer);int index=0;uu crop=uu.aA;int cropId=crop.bn;
            for(String line:profiles){if(line.isBlank()||line.startsWith("#"))continue;String[] c=line.split("\\t");need(c.length==16,"name plus15 signed inputs");long[] p=new long[15];for(int i=0;i<15;i++)p[i]=Long.parseLong(c[i+1]);
                // via,count,damage,face,x,y,z,target,target_metadata,remote,null_mask,item_seed,world_seed,scene,observer_mode
                for(lm chunk:chunks){Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)15);Arrays.fill(chunk.g.a,(byte)0);Arrays.fill(chunk.h,(byte)64);chunk.i=64;chunk.n=true;}
                for(int x=0;x<32;x++)for(int z=0;z<32;z++)put(chunks,x,63,z,p[13]==1?3:2,0);
                if(p[13]==1)put(chunks,(int)p[4],63,(int)p[6],2,0);
                if(p[7]==59)put(chunks,(int)p[4],(int)p[5]-1,(int)p[6],60,0);
                put(chunks,(int)p[4],(int)p[5],(int)p[6],(int)p[7],(int)p[8]);
                if(p[13]==2)put(chunks,(int)p[4],(int)p[5]+1,(int)p[6],1,0);
                if(p[13]==3)put(chunks,(int)p[4],(int)p[5]+1,(int)p[6],1,0);
                if(p[13]==4)field(uu.class,"bn").setInt(crop,0);
                world.B=p[9]!=0;field(gm.class,"b").set(null,itemRandom);itemRandom.setSeed(p[11]);world.r.setSeed(p[12]);
                iz stack=new iz(351,(int)p[1],(int)p[2]);stack.b=5;observedStack=stack;observer.mode=(int)p[14];observer.cells=0;observer.switched=false;observer.notices.clear();
                Object itemBefore=random(itemRandom),worldBefore=random(world.r);byte[][] prior=arrays(chunks);List<Integer> stackBefore=stack(stack);int used=stats.a(jl.E[351]);String result="UNRETURNED",exception="NONE";int mask=(int)p[10];active=index;
                try{boolean returned=p[0]==0?item.a((mask&4)!=0?null:stack,(mask&2)!=0?null:player,(mask&1)!=0?null:world,(int)p[4],(int)p[5],(int)p[6],(int)p[3]):((mask&4)!=0?null:stack).a((mask&2)!=0?null:player,(mask&1)!=0?null:world,(int)p[4],(int)p[5],(int)p[6],(int)p[3]);result=Boolean.toString(returned);}
                catch(NullPointerException e){exception=e.getClass().getName();}catch(IllegalStateException e){exception=e.getClass().getName();}
                finally{active=-1;}
                Object itemAfter=random(itemRandom),worldAfter=random(world.r);boolean alias=field(gm.class,"b").get(null)==world.r;
                field(gm.class,"b").set(null,itemRandom);field(uu.class,"bn").setInt(crop,cropId);
                List<Object> inputs=new ArrayList<>();for(long v:p)inputs.add(v);
                System.out.println("DYE\t"+json(map("index",index,"name",c[0],"inputs",inputs,"channel",p[0]==0?"direct":"wrapper","result",result,"exception",exception,"stack_before",stackBefore,"stack_after",stack(stack),"item_random_before",itemBefore,"item_random_after",itemAfter,"world_random_before",worldBefore,"world_random_after",worldAfter,"gm_b_is_world_after",alias,"used_delta",stats.a(jl.E[351])-used,"remote_after",world.B,"observer_cells",observer.cells,"notifications",observer.notices,"changes",changes(prior,arrays(chunks)),"blocks_metadata_after",cells(chunks))));index++;
            }
            for(int i=0;i<chunks.length;i++)need(world.c(i/2,i%2)==chunks[i],"same canonical original Chunks");need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no external source attempt");
            System.out.println("DYE_COMPLETE rows="+index+" network="+network+" native="+natives+" execs="+execs+" denied-writes="+deniedWrites+" world-writes="+worldWrites+" game-code-replacements=0");
        }finally{app.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker ended");}}
    }
}
