import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import net.minecraft.client.Minecraft;

/** Declared data and observers around unchanged original Item/Stack/World bodies. */
public final class FarmingObservation {
    static int active=-1,network,natives,execs,deniedWrites,worldWrites;
    static fd observedWorld; static dc observedPlayer; static iz observedStack;
    static gm observedItem; static uu observedGrass,observedDirt,observedFarmland;
    static ct observedSound;
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static void need(boolean value,String text){if(!value)throw new AssertionError(text);}
    static String bits(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static String bits(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static Map<String,Object> map(Object... p){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<p.length;i+=2)r.put((String)p[i],p[i+1]);return r;}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String)return "\""+((String)v).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";
        if(v instanceof Map){StringJoiner s=new StringJoiner(",","{","}");for(Object x:((Map<?,?>)v).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)x;s.add(json(e.getKey())+":"+json(e.getValue()));}return s.toString();}
        if(v instanceof Iterable){StringJoiner s=new StringJoiner(",","[","]");for(Object x:(Iterable<?>)v)s.add(json(x));return s.toString();}
        return v.toString();
    }
    static List<Integer> stack(iz s){return List.of(s.a,s.b,s.c,s.i());}
    static List<Integer> cell(fd w,int x,int y,int z){return List.of(w.a(x,y,z),w.e(x,y,z));}
    static byte[][] arrays(lm[] c){byte[][] r=new byte[c.length*5][];for(int i=0;i<c.length;++i){r[i*5]=c[i].b.clone();r[i*5+1]=c[i].e.a.clone();r[i*5+2]=c[i].f.a.clone();r[i*5+3]=c[i].g.a.clone();r[i*5+4]=c[i].h.clone();}return r;}
    static List<Object> changes(byte[][] a,byte[][] b){List<Object> r=new ArrayList<>();for(int i=0;i<a.length;++i){need(a[i].length==b[i].length,"same arrays");for(int j=0;j<a[i].length;++j)if(a[i][j]!=b[i][j])r.add(List.of(i/5,i%5,j,a[i][j]&255,b[i][j]&255));}return r;}
    static void put(lm[] chunks,int x,int y,int z,int id){if(x<0||x>31||z<0||z>31||y<0||y>127)return;lm c=chunks[(x>>4)*2+(z>>4)];c.b[((x&15)<<11)|((z&15)<<7)|y]=(byte)id;}
    static final class Observer implements pm {
        final List<Object> sounds=new ArrayList<>(),notices=new ArrayList<>();
        int soundMode,cellFail;
        public void a(String n,double x,double y,double z,float v,float p){
            sounds.add(List.of(n,bits(x),bits(y),bits(z),bits(v),bits(p)));
            if(soundMode==1)throw new IllegalStateException("declared sound observer failure");
            if(soundMode==2)observedWorld.B=true;
            if(soundMode==3)observedWorld.B=false;
            if(soundMode==4)try{field(uu.class,"bn").setInt(observedFarmland,3);}catch(Exception e){throw new AssertionError(e);}
        }
        public void a(int x,int y,int z){notices.add(List.of("cell",x,y,z));if(cellFail!=0)throw new IllegalStateException("declared cell observer failure");}
        public void b(int x,int y,int z,int a,int b,int c){notices.add(List.of("range",x,y,z,a,b,c));}
        public void a(String n,double x,double y,double z,double a,double b,double c){notices.add(List.of("particle",n,bits(x),bits(y),bits(z)));}
        public void a(sn e){notices.add(List.of("add",e.getClass().getName()));}
        public void b(sn e){notices.add(List.of("remove",e.getClass().getName()));}
        public void e(){notices.add(List.of("refresh"));}
        public void a(String n,int x,int y,int z){notices.add(List.of("record",n,x,y,z));}
        public void a(int x,int y,int z,ow t){notices.add(List.of("tile",x,y,z,t==null?"null":t.getClass().getName()));}
        public void a(gs p,int a,int b,int c,int d,int e){notices.add(List.of("level",a,b,c,d,e));}
    }
    static void deny(Runnable r){try{r.run();throw new AssertionError("expected guard denial");}catch(SecurityException expected){}}
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);need(args.length==3,"jar,privateworld,profiles");Path jar=Path.of(args[0]).toRealPath(),dir=Path.of(args[1]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String n){++natives;throw new SecurityException("native");}public void checkExec(String n){++execs;throw new SecurityException("exec");}
            public void checkWrite(String p){if(Path.of(p).toAbsolutePath().normalize().startsWith(dir)){++worldWrites;return;}++deniedWrites;throw new SecurityException("outside privateworld");}public void checkDelete(String p){checkWrite(p);}});
        for(Class<?> t:new Class<?>[]{la.class,rk.class,gm.class,iz.class,fd.class,lm.class,mx.class,dc.class,gs.class,ct.class,uu.class,xi.class,jl.class})need(Path.of(t.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original class origin");
        kq app=new kq(null,null,null,854,480,false,null);
        try{
            app.k=new gr("FarmingReference","-");field(Minecraft.class,"Z").set(app,dir.toFile());xi stats=new xi(app.k,dir.toFile());app.I=stats;
            fd world=new fd(new mx(dir.getParent().toFile(),dir.getFileName().toString(),false),"",23L);observedWorld=world;
            lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};dc player=new dc(app,world,app.k,0);app.f=world;app.h=player;observedPlayer=player;
            Observer observer=new Observer();world.a(observer);int index=0;
            for(String line:profiles){if(line.isBlank()||line.startsWith("#"))continue;String[] col=line.split("\\t");need(col.length==20,"name and nineteen signed inputs");int[] p=new int[19];for(int i=0;i<p.length;++i)p[i]=Integer.parseInt(col[i+1]);
                // kind,via,item,count,damage,damageMode,face,x,y,z,ground,above,remote,nulls,alias,sound,cellFail,cropId,dirtId
                gm item=gm.c[p[2]];need(p[0]==0?item instanceof la:item instanceof rk,"actual registered family");observedItem=item;
                uu grass=uu.v,dirt=uu.w,farmland=uu.aB;ct sound=farmland.by;int dirtId=dirt.bn,farmlandId=farmland.bn;Field planted=p[0]==1?field(rk.class,"a"):null;int crop=planted==null?0:planted.getInt(item);
                observedGrass=grass;observedDirt=dirt;observedFarmland=farmland;observedSound=sound;
                for(lm c:chunks){Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);Arrays.fill(c.f.a,(byte)15);Arrays.fill(c.g.a,(byte)0);Arrays.fill(c.h,(byte)0);c.i=0;c.n=true;}
                put(chunks,p[7],p[8],p[9],p[10]);put(chunks,p[7],p[8]+1,p[9],p[11]);
                world.B=p[12]!=0;observer.soundMode=p[15];observer.cellFail=p[16];observer.sounds.clear();observer.notices.clear();
                int maximum=item.f(),damage=p[5]==1?maximum:p[5]==2?maximum-1:p[5]==3?maximum+1:p[4];iz stack=new iz(p[2],p[3],damage);stack.b=5;observedStack=stack;
                need(p[14]==0&&p[15]<=4&&(p[13]&~71)==0,"final static references remain original");
                if(planted!=null)planted.setInt(item,p[17]);field(uu.class,"bn").setInt(dirt,p[18]);
                int mask=p[13];if((mask&64)!=0)farmland.by=null;
                List<Integer> before=stack(stack),centerBefore=cell(world,p[7],p[8],p[9]),aboveBefore=cell(world,p[7],p[8]+1,p[9]);byte[][] prior=arrays(chunks);int used=stats.a(jl.E[p[2]]),broken=jl.F[p[2]]==null?0:stats.a(jl.F[p[2]]);String result="UNRETURNED",exception="NONE";
                active=index;
                try{boolean returned=p[1]==0?item.a((mask&2)!=0?null:stack,(mask&4)!=0?null:player,(mask&1)!=0?null:world,p[7],p[8],p[9],p[6]):stack.a((mask&4)!=0?null:player,(mask&1)!=0?null:world,p[7],p[8],p[9],p[6]);result=Boolean.toString(returned);}
                catch(NullPointerException e){exception=e.getClass().getName();}catch(IllegalStateException e){exception=e.getClass().getName();}finally{active=-1;}
                List<Object> changed=changes(prior,arrays(chunks));List<Integer> after=stack(stack);int usedDelta=stats.a(jl.E[p[2]])-used,brokenDelta=jl.F[p[2]]==null?0:stats.a(jl.F[p[2]])-broken;
                // Restore declared statics before observational World queries, never during the source call.
                farmland.by=sound;field(uu.class,"bn").setInt(dirt,dirtId);field(uu.class,"bn").setInt(farmland,farmlandId);if(planted!=null)planted.setInt(item,crop);
                List<Integer> values=new ArrayList<>();for(int v:p)values.add(v);
                System.out.println("FARMING\t"+json(map("index",index,"name",col[0],"values",values,"maximum",maximum,"result",result,"exception",exception,"stack_before",before,"stack_after",after,"center_before",centerBefore,"center_after",cell(world,p[7],p[8],p[9]),"above_before",aboveBefore,"above_after",cell(world,p[7],p[8]+1,p[9]),"remote_after",world.B,"sounds",observer.sounds,"notifications",observer.notices,"changes",changed,"used_delta",usedDelta,"broken_delta",brokenDelta)));++index;
            }
            for(int i=0;i<chunks.length;++i)need(world.c(i/2,i%2)==chunks[i],"same original canonical Chunks");need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no external source attempt");System.out.println("FARMING_GUARD source network=0 native=0 execs=0 denied-writes=0 world-writes="+worldWrites);
            deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));deny(()->System.getSecurityManager().checkLink("farming-guard"));deny(()->System.getSecurityManager().checkExec("farming-guard"));deny(()->System.getSecurityManager().checkWrite(dir.resolveSibling("farming-guard").toString()));need(network==2&&natives==1&&execs==1&&deniedWrites==1,"five guard controls");System.out.println("FARMING_COMPLETE rows="+index+" game-code-replacements=0 jdi-source-writes=0");
        }finally{app.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker ended");}}
    }
}
