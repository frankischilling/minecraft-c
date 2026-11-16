import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import org.lwjgl.opengl.*;
import net.minecraft.client.Minecraft;

/** Declared constructors/direct source calls; reflection is read-only. */
public final class PistonDrawObservation {
    public static int active=-1;
    static int network,execs,deniedWrites;
    static final List<Object> links=new ArrayList<>(),writes=new ArrayList<>(),deletes=new ArrayList<>();
    static final String[] CORE={"hy","je","ll","uk","ow","wj","cv","jq","h","ut","uu","nw","u","ji","ik","i","kv","kq","net.minecraft.client.Minecraft","fd","mx","fm","ei","lm","rh","xa","ok","ld","nu","rw","wh","ls","sn","pm","eq"};
    static void need(boolean v,String message){if(!v)throw new AssertionError(message);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static String f(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static String d(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static Map<String,Object> map(Object...v){return ConstructorJson.map(v);}
    static String sha(byte[] raw)throws Exception{StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    static Object primitive(Class<?> c,Object o,boolean statics)throws Exception {
        Map<String,Object> out=new TreeMap<>();
        for(Field v:c.getDeclaredFields())if(v.getType().isPrimitive()&&Modifier.isStatic(v.getModifiers())==statics){v.setAccessible(true);Object x=v.get(o);out.put(v.getName(),x instanceof Float?f((Float)x):x instanceof Double?d((Double)x):x);}
        return out;
    }
    static Object bounds(uu block){return List.of(d(block.bs),d(block.bt),d(block.bu),d(block.bv),d(block.bw),d(block.bx));}
    static Object tile(uk t,fd w)throws Exception{return map("class",t.getClass().getName(),"world_same",t.d==w,"xyz",List.of(t.e,t.f,t.g),"invalid",get(t,ow.class,"h"),"block",t.a(),"metadata",t.e(),"facing",t.d(),"extending",t.b(),"render_head",t.k(),"current_bits",f((Float)get(t,uk.class,"k")),"previous_bits",f((Float)get(t,uk.class,"l")));}
    static List<String> words(int n)throws Exception{int[] raw=(int[])get(nw.a,nw.class,"g");List<String> out=new ArrayList<>();need(raw.length>=n,"bounded retained words");for(int i=0;i<n;i++)out.add(String.format(Locale.ROOT,"%08x",raw[i]));return out;}
    static List<Object> tileList(List<?> list,uk selected){List<Object> out=new ArrayList<>();for(Object o:list)out.add(map("class",o==null?null:o.getClass().getName(),"selected",o==selected));return out;}
    static Object gl(){return map("lighting",GL11.glIsEnabled(2896),"light0",GL11.glIsEnabled(16384),"light1",GL11.glIsEnabled(16385),"color_material",GL11.glIsEnabled(2903),"blend",GL11.glIsEnabled(3042),"cull",GL11.glIsEnabled(2884),"shade",GL11.glGetInteger(2900),"blend_src",GL11.glGetInteger(3041),"blend_dst",GL11.glGetInteger(3040),"bound_texture",GL11.glGetInteger(32873),"error",GL11.glGetError());}
    static Object state(fd w,lm chunk,uk t,hy renderer,uu carried)throws Exception {
        cv blocks=(cv)get(renderer,hy.class,"b");ei info=(ei)get(w,fd.class,"x");
        List<Object> mapped=new ArrayList<>();for(Object value:chunk.l.values())mapped.add(map("class",value.getClass().getName(),"selected",value==t));
        return map("tile",tile(t,w),"world_time",get(info,ei.class,"e"),"world_iterating",get(w,fd.class,"L"),"world_entities",w.b.size(),"world_players",w.d.size(),"active_tiles",tileList(w.c,t),"pending_tiles",tileList((List<?>)get(w,fd.class,"G"),t),"mapped_tiles",mapped,
          "block",w.a(8,64,8),"world_metadata",w.e(8,64,8),"cv_class",blocks.getClass().getName(),"cv_world_same",get(blocks,cv.class,"c")==w,"cv",primitive(cv.class,blocks,false),"carried_bounds",carried==null?null:bounds(carried),"head_bounds",bounds(uu.ab),"moving_bounds",bounds(uu.ad),"head_texture",get(uu.ab,h.class,"a"),"nw",primitive(nw.class,nw.a,false),"retained_word_window",words(768),"gl",gl());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        need(args.length==5,"jar saves profiles natives owned-work");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),saves=Path.of(args[1]).toRealPath(),profiles=Path.of(args[2]).toRealPath(),natives=Path.of(args[3]).toRealPath(),work=Path.of(args[4]).toRealPath();
        List<String[]> cases=new ArrayList<>();for(String line:Files.readAllLines(profiles)){String[] row=line.split("\\t",-1);need(row.length==12,"closed profile columns");cases.add(row);}
        Set<Path> locks=new HashSet<>(),regions=new HashSet<>(),dirs=new HashSet<>();
        for(String[] row:cases){Path world=saves.resolve(row[1]).toRealPath();need(world.getParent().equals(saves),"contained owned World");locks.add(world.resolve("session.lock"));regions.add(world.resolve("region/r.0.0.mcr"));dirs.add(world);}
        Map<String,Object> origins=map();for(String name:CORE){Class<?> c=Class.forName(name,false,PistonDrawObservation.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"actual original Class JAR");try(InputStream in=c.getResourceAsStream("/"+name.replace('.','/')+".class")){need(in!=null,"actual Class resource");origins.put(name,sha(in.readAllBytes()));}}
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){network++;throw new SecurityException("network");}
            public void checkListen(int p){network++;throw new SecurityException("listen");}
            public void checkExec(String s){execs++;throw new SecurityException("exec");}
            public void checkDelete(String s){Path p=Path.of(s).toAbsolutePath().normalize();boolean cache=p.getParent().equals(work.resolve("tmp"))&&p.getFileName().toString().startsWith("imageio")&&p.getFileName().toString().endsWith(".tmp");deletes.add(map("role",cache?"owned-imageio-cache":null));if(!cache){deniedWrites++;throw new SecurityException("delete");}}
            public void checkWrite(String s){Path p=Path.of(s).toAbsolutePath().normalize();String role=locks.contains(p)?"owned-session-lock":regions.contains(p)?"existing-owned-region-rw-open":dirs.contains(p)?"existing-world-directory":p.startsWith(work)?"owned-preparation-workspace":null;writes.add(map("role",role));if(role==null){deniedWrites++;throw new SecurityException("outside declared owned paths");}}
            public void checkLink(String s){Path p=Path.of(s);boolean lwjgl=p.isAbsolute()&&p.normalize().equals(natives.resolve("lwjgl64.dll"));boolean jdk=Set.of("awt","awt_headless","fontmanager","freetype","lcms","zip","nio","net").contains(s);links.add(map("role",lwjgl?"pinned-lwjgl64":jdk?"jdk-native":null,"name",lwjgl?"lwjgl64.dll":s));need(lwjgl||jdk,"unexpected native library");}
        });
        Pbuffer context=null;kq app=null;
        try {
            context=new Pbuffer(32,32,new PixelFormat(),null,null);context.makeCurrent();
            app=new kq(null,null,null,854,480,false,null);app.z=new kv();
            ik packs=new ik(app,work.toFile());ji engine=new ji(packs,app.z);
            ll manager=ll.a;je selected=manager.a(uk.class);need(selected!=null&&selected.getClass()==hy.class,"actual LL canonical HY");hy renderer=(hy)selected;
            try(InputStream in=uu.class.getResourceAsStream("/terrain.png")){need(in!=null,"actual original terrain resource");System.out.println("PISTON_DRAW_ORIGINS "+ConstructorJson.json(map("jar_sha256",sha(Files.readAllBytes(jar)),"classes",origins,"terrain_sha256",sha(in.readAllBytes()),"terrain_jar",uu.class.getResource("/terrain.png").toString().startsWith("jar:"+jar.toUri().toURL().toString()+"!/"))));}
            System.out.println("PISTON_DRAW_DEVICE "+ConstructorJson.json(map("vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"hidden_pbuffer",true)));
            for(String[] row:cases){
                int index=Integer.parseInt(row[0]),block=Integer.parseInt(row[2]),metadata=Integer.parseInt(row[3]),direction=Integer.parseInt(row[4]),ticks=Integer.parseInt(row[5]);float partial=Float.intBitsToFloat((int)Long.parseLong(row[6],16));
                fd world=new fd(new mx(saves.toFile(),row[1],false),row[10],Long.parseLong(row[11]));
                lm chunk=null;for(int x=0;x<2;x++)for(int z=0;z<2;z++){lm c=world.c(x,z);need(c!=null&&c.getClass()==lm.class,"genuine saved Chunk");if(x==0&&z==0)chunk=c;}
                need(world.a(8,64,8)==36&&world.e(8,64,8)==metadata&&chunk.l.isEmpty(),"actual authored moving-cell input");
                uk tile=new uk(block,metadata,direction,Boolean.parseBoolean(row[7]),Boolean.parseBoolean(row[8]));
                world.a(8,64,8,tile);need(world.b(8,64,8)==tile&&chunk.l.containsValue(tile),"actual World/Chunk Tile publication");
                for(int i=0;i<ticks;i++)tile.n_();
                wh viewer=new wh(world);manager.a(world,engine,null,viewer,partial);app.z.k=Boolean.parseBoolean(row[9]);
                uu carried=uu.m[block];Object before=state(world,chunk,tile,renderer,carried);
                active=index;Throwable failure=null;try{renderer.a(tile,.375,.625,-.125,partial);}catch(Throwable error){failure=error;}finally{active=-1;}
                Object after=state(world,chunk,tile,renderer,carried);
                int texture=(Integer)((Map<?,?>)get(engine,ji.class,"b")).getOrDefault("/terrain.png",null);
                Object textureInfo=map("id",texture,"width",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_WIDTH),"height",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_HEIGHT));
                System.out.println("PISTON_DRAW_ROW "+ConstructorJson.json(map("index",index,"profile",Arrays.asList(row),"before",before,"returned",failure==null,"failure",failure==null?null:map("class",failure.getClass().getName(),"message",failure.getMessage()),"after",after,"texture",textureInfo)));
                need(failure==null,"unexpected direct draw failure");
            }
            need(network==0&&execs==0&&deniedWrites==0,"declared external scope");
            System.out.println("PISTON_DRAW_COMPLETE "+ConstructorJson.json(map("rows",cases.size(),"network",network,"execs",execs,"denied_writes",deniedWrites,"links",links,"write_checks",writes,"delete_checks",deletes,"raw_allocations",0,"uk_field_writes",0,"world_tick_calls",0)));
        } finally {
            if(app!=null)app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer retired");}
            if(context!=null)context.destroy();
        }
    }
}
