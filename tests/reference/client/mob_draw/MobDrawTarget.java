import java.awt.image.BufferedImage;
import java.io.*;
import java.lang.reflect.*;
import java.nio.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import javax.imageio.ImageIO;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.*;

/** Authored observer of unchanged original Sheep drawing and texture lookup. */
@SuppressWarnings({"unchecked","rawtypes","removal"})
public final class MobDrawTarget {
    public static int active=-1,constructor_case=-1,texture_case=-1,observed=-1;
    public static fd world;public static dl actor;public static bx camera;
    public static th manager;public static bw renderer;public static ko primary,fur;
    public static ji engine;public static kv settings;public static Object constructed;
    static Path output,inputs;static PrintWriter journal;static int network;static final List<String> networkAttempts=new ArrayList<>();
    static final IdentityHashMap<Object,Integer> ids=new IdentityHashMap<>();
    static final Set<String> expand=Set.of("th","xy","gv","bw","mw","ea","nn","ko","fh","ps","tz","ib","bt");
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static void need(boolean v,String m){if(!v)throw new AssertionError(m);}
    static Object raw(Class<?> c)throws Exception{Class<?> u=Class.forName("sun.misc.Unsafe");return u.getMethod("allocateInstance",Class.class).invoke(field(u,"theUnsafe").get(null),c);}
    static String hex(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static String hex(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static String hash(byte[]b)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b));}
    static Object value(Object v){if(v==null)return null;if(v instanceof Float)return ConstructorJson.map("F",hex((Float)v));if(v instanceof Double)return ConstructorJson.map("D",hex((Double)v));if(v instanceof Long)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",(Long)v));if(v instanceof Number||v instanceof Boolean||v instanceof String)return v;return ConstructorJson.map("class",v.getClass().getName(),"identity",ids.computeIfAbsent(v,k->ids.size()+1));}
    static Map<String,Object> fields(Object o)throws Exception{if(o==null)return null;Map<String,Object>r=new TreeMap<>();for(Class<?>c=o.getClass();c!=Object.class;c=c.getSuperclass())for(Field f:c.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())){f.setAccessible(true);r.put(c.getName()+"."+f.getName(),value(f.get(o)));}return r;}
    static String seed(Random r)throws Exception{return String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,Random.class,"seed")).get());}
    static Random math()throws Exception{return(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");}
    static Map<String,Object> entropy()throws Exception{return ConstructorJson.map("actor",seed(actor.bs),"camera",seed(camera.bs),"world",seed(world.r),"math",seed(math()),"uniquifier",value(((AtomicLong)get(null,Random.class,"seedUniquifier")).get()));}
    static Map<String,Object> graph()throws Exception{
        Map<String,Object>roots=ConstructorJson.map("manager",value(manager),"renderer",value(renderer),"primary",value(primary),"fur",value(fur),"actor",value(actor),"camera",value(camera),"world",value(world),"engine",value(engine),"settings",value(settings));
        ArrayDeque<Object>q=new ArrayDeque<>();for(Object o:new Object[]{manager,renderer,primary,fur})if(o!=null)q.add(o);Set<Object>seen=Collections.newSetFromMap(new IdentityHashMap<>());List<Object>nodes=new ArrayList<>();
        while(!q.isEmpty()){Object o=q.remove();if(!seen.add(o))continue;Class<?>c=o.getClass();boolean array=c.isArray()&&!c.getComponentType().isPrimitive();if(!array&&!expand.contains(c.getName()))continue;Map<String,Object>n=new LinkedHashMap<>((Map<String,Object>)value(o));if(array){int len=Array.getLength(o);List<Object>v=new ArrayList<>();for(int i=0;i<len;i++){Object x=Array.get(o,i);v.add(value(x));if(x!=null)q.add(x);}n.put("length",len);n.put("values",v);}else{n.put("fields",fields(o));for(Class<?>a=c;a!=Object.class;a=a.getSuperclass())for(Field f:a.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())&&!f.getType().isPrimitive()){f.setAccessible(true);Object x=f.get(o);if(x!=null)q.add(x);}}nodes.add(n);}
        return ConstructorJson.map("roots",roots,"objects",nodes);
    }
    static fd makeWorld()throws Exception{fd w=new fd(new wy(),"MobDrawTarget",xa.a(0),23L);w.B=false;w.q=1;Object provider=get(w,fd.class,"v");Map map=(Map)get(provider,ok.class,"e");List list=(List)get(provider,ok.class,"f");map.clear();list.clear();for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){lm c=new lm(w,new byte[32768],x,z);c.n=true;for(int a=0;a<16;a++)for(int b=0;b<16;b++)c.b[(a<<11)|(b<<7)|63]=1;Arrays.fill(c.h,(byte)64);c.i=64;Arrays.fill((byte[])get(c.f,wi.class,"a"),(byte)255);map.put(yy.a(x,z),c);list.add(c);}((ei)get(w,fd.class,"x")).a(6000L);w.k();return w;}
    static void resources()throws Exception {
        Path resources=output.resolve("resources");Files.createDirectories(resources);
        for(String filename:new String[]{"skin.png","fur.png","shadow.png","pack.png"}) {
            boolean shadow=filename.equals("shadow.png");int w=shadow?128:64,h=shadow?128:32;
            BufferedImage image=new BufferedImage(w,h,BufferedImage.TYPE_INT_ARGB);
            for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
                int color;
                if(shadow){double dx=(x+.5)/64-1,dy=(y+.5)/64-1;int alpha=(int)(Math.max(0,1-Math.sqrt(dx*dx+dy*dy))*180);color=alpha<<24;}
                else if(filename.startsWith("fur")){int alpha=((x/4+y/4)&1)==0?255:0;color=(alpha<<24)|((x<32?0xe8:0x70)<<16)|((y<16?0xc0:0x40)<<8)|0x50;}
                else color=0xff000000|((x<32?0x30:0xe0)<<16)|((y<16?0x90:0x40)<<8)|(x%8<4?0xd0:0x40);
                image.setRGB(x,y,color);
            }
            String name=shadow?"misc/shadow.png":filename.equals("pack.png")?"pack.png":filename.startsWith("fur")?"mob/sheep_fur.png":"mob/sheep.png";
            Path path=resources.resolve(name);Files.createDirectories(path.getParent());ImageIO.write(image,"png",path.toFile());
            java.net.URL selected=i.class.getResource("/"+name);need(selected!=null&&Path.of(selected.toURI()).toRealPath().equals(path.toRealPath()),"authored resource-only classpath selected");
            journal.println(ConstructorJson.json(ConstructorJson.map("event","resource_input","resource",name,"bytes",Files.size(path),"sha256",hash(Files.readAllBytes(path)),"runtime_url",selected.toString())));
        }
    }
    static Map<String,Object> textureState()throws Exception {
        return ConstructorJson.map("cache_size",((Map)get(engine,ji.class,"b")).size(),"clamp",get(engine,ji.class,"j"),"blur",get(engine,ji.class,"k"),
            "bound",GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D),"wrap_s",GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_WRAP_S),
            "wrap_t",GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_WRAP_T),"min",GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER),
            "mag",GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER),"width",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_WIDTH),
            "height",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_HEIGHT),"border",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_BORDER));
    }
    static void texture(int index,String key)throws Exception {
        Map<String,Object> before=textureState();int name;texture_case=index;try{name=engine.b(key);}finally{texture_case=-1;}
        Map<String,Object> after=textureState();ByteBuffer buffer=(ByteBuffer)get(engine,ji.class,"f");need(buffer.limit()<=262144,"bounded original transfer");StringBuilder transfer=new StringBuilder();for(int at=0;at<buffer.limit();at++)transfer.append(String.format(Locale.ROOT,"%02x",buffer.get(at)&255));int cached;texture_case=index+3;try{cached=engine.b(new String(key));}finally{texture_case=-1;}
        need(name==cached,"actual cached texture name reuse");
        journal.println(ConstructorJson.json(ConstructorJson.map("event","texture_lookup","index",index,"key",key,"name",name,"cached_name",cached,"before",before,"after",after,"after_hit",textureState(),"transfer_hex",transfer.toString(),"gl_error",GL11.glGetError())));journal.flush();
    }
    static List<String[]> table(String name)throws Exception {
        List<String[]> rows=new ArrayList<>();for(String line:Files.readAllLines(inputs.resolve(name)))if(!line.isBlank()&&!line.startsWith("#"))rows.add(line.split("\\s+"));return rows;
    }
    static void prepare()throws Exception {
        for(String[] p:table("preparation.tsv")) {
            need(p.length==5,"preparation width");Object receiver=p[0].equals("actor")?actor:p[0].equals("camera")?camera:null;
            if(p[3].equals("pose")){String[]v=p[4].split(",");need(receiver instanceof sn&&v.length==5&&p[2].equals("c"),"real Entity pose input");((sn)receiver).c(Double.parseDouble(v[0]),Double.parseDouble(v[1]),Double.parseDouble(v[2]),Float.parseFloat(v[3]),Float.parseFloat(v[4]));}
            else {Field f=field(Class.forName(p[1]),p[2]);switch(p[3]){case"D":f.setDouble(receiver,Double.parseDouble(p[4]));break;case"F":f.setFloat(receiver,Float.parseFloat(p[4]));break;case"I":f.setInt(receiver,Integer.parseInt(p[4]));break;default:throw new AssertionError("preparation type");}}
        }
    }
    static Map<String,Object> entity(sn e)throws Exception {
        ud w=(ud)get(e,sn.class,"bD");Map records=(Map)get(w,ud.class,"b");List<Integer>keys=new ArrayList<>(records.keySet());Collections.sort(keys);List<Object>watch=new ArrayList<>();
        for(int key:keys)watch.add(ConstructorJson.map("index",key,"record",value(records.get(key)),"fields",fields(records.get(key))));
        return ConstructorJson.map("actual",value(e),"fields",fields(e),"bounds",fields(e.aW),"watcher",value(w),"watcher_dirty",get(w,ud.class,"c"),"watcher_records",watch);
    }
    static Object pool()throws Exception {
        List storage=(List)get(null,bt.class,"d");List<Object>vectors=new ArrayList<>();for(Object v:storage)vectors.add(ConstructorJson.map("actual",value(v),"fields",fields(v)));
        return ConstructorJson.map("storage",value(storage),"length",storage.size(),"vectors",vectors);
    }
    static Map<String,Object> state()throws Exception {
        return ConstructorJson.map("graph",graph(),"parts",partState(),"actor",entity(actor),"camera",entity(camera),"entropy",entropy(),"pool",field(bt.class,"e").getInt(null),"pool_storage",pool(),"gl",gl());
    }
    static List<ps> parts(ko model)throws Exception{List<ps>r=new ArrayList<>();for(Field f:nn.class.getDeclaredFields())if(f.getType()==ps.class)r.add((ps)f.get(model));need(r.size()==6,"six actual NN parts");return r;}
    static Object partState()throws Exception{List<Object>r=new ArrayList<>();for(ko m:new ko[]{primary,fur})for(ps p:parts(m)){int name=field(ps.class,"o").getInt(p);r.add(ConstructorJson.map("part",value(p),"fields",fields(p),"is_list",name>0&&GL11.glIsList(name)));}return r;}
    static Map<String,Object> gl()throws Exception{FloatBuffer b=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(GL11.GL_CURRENT_COLOR,b);List<String>color=new ArrayList<>();for(int i=0;i<4;i++)color.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,b);List<String>mv=new ArrayList<>();for(int i=0;i<16;i++)mv.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_PROJECTION_MATRIX,b);List<String>pr=new ArrayList<>();for(int i=0;i<16;i++)pr.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_CURRENT_NORMAL,b);List<String>normal=new ArrayList<>();for(int i=0;i<3;i++)normal.add(hex(b.get(i)));List<Object>caps=new ArrayList<>();for(int cap:new int[]{GL11.GL_CULL_FACE,GL11.GL_BLEND,GL11.GL_ALPHA_TEST,GL11.GL_DEPTH_TEST,GL11.GL_LIGHTING,32826,GL11.GL_NORMALIZE,GL11.GL_TEXTURE_2D,GL11.GL_FOG})caps.add(ConstructorJson.map("cap",cap,"enabled",GL11.glIsEnabled(cap)));return ConstructorJson.map("color",color,"normal",normal,"modelview",mv,"projection",pr,"mode",GL11.glGetInteger(GL11.GL_MATRIX_MODE),"modelview_depth",GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),"projection_depth",GL11.glGetInteger(GL11.GL_PROJECTION_STACK_DEPTH),"depth_mask",GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK),"depth_function",GL11.glGetInteger(GL11.GL_DEPTH_FUNC),"blend_src",GL11.glGetInteger(GL11.GL_BLEND_SRC),"blend_dst",GL11.glGetInteger(GL11.GL_BLEND_DST),"texture",GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D),"caps",caps);}
    static void frame(){GL11.glViewport(0,0,256,256);GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glOrtho(-2,2,-1.5,2.5,.1,50);GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();GL11.glTranslatef(0,-.25f,-6);GL11.glRotatef(18,1,0,0);GL11.glRotatef(-25,0,1,0);GL11.glClearColor(.1f,.15f,.2f,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT|GL11.GL_DEPTH_BUFFER_BIT);GL11.glEnable(GL11.GL_DEPTH_TEST);GL11.glDepthFunc(GL11.GL_LEQUAL);GL11.glDepthMask(true);GL11.glDisable(GL11.GL_TEXTURE_2D);GL11.glDisable(GL11.GL_CULL_FACE);GL11.glColor4f(.7f,.7f,.7f,1);GL11.glBegin(GL11.GL_QUADS);GL11.glVertex3d(-3,0,-3);GL11.glVertex3d(-3,0,3);GL11.glVertex3d(3,0,3);GL11.glVertex3d(3,0,-3);GL11.glEnd();GL11.glEnable(GL11.GL_TEXTURE_2D);GL11.glEnable(GL11.GL_ALPHA_TEST);GL11.glAlphaFunc(GL11.GL_GREATER,.1f);GL11.glDisable(GL11.GL_BLEND);GL11.glDisable(GL11.GL_LIGHTING);GL11.glDisable(GL11.GL_FOG);GL11.glDisable(32826);GL11.glDisable(GL11.GL_NORMALIZE);GL11.glColor4f(.125f,.25f,.5f,.75f);}
    static Object pixels(String filename)throws Exception{GL11.glFinish();ByteBuffer bytes=BufferUtils.createByteBuffer(256*256*4);GL11.glReadPixels(0,0,256,256,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,bytes);byte[]raw=new byte[bytes.capacity()];bytes.get(raw);Files.write(output.resolve(filename+".rgba"),raw);BufferedImage image=new BufferedImage(256,256,BufferedImage.TYPE_INT_ARGB);for(int y=0;y<256;y++)for(int x=0;x<256;x++){int i=(y*256+x)*4;image.setRGB(x,255-y,((raw[i+3]&255)<<24)|((raw[i]&255)<<16)|((raw[i+1]&255)<<8)|(raw[i+2]&255));}ImageIO.write(image,"png",output.resolve(filename+".png").toFile());return ConstructorJson.map("file",filename+".rgba","sha256",hash(raw));}
    static void observe(String[] profile)throws Exception{need(profile.length==7,"profile width");int index=Integer.parseInt(profile[0]);boolean shadowOnly=profile[1].equals("shadow");need(shadowOnly||profile[1].equals("manager"),"profile entry");float partial=Float.parseFloat(profile[2]);frame();Object beforePixels=pixels("baseline"+index);Map<String,Object>before=state();int outcome=1;String failure=null;active=index;try{if(shadowOnly)renderer.b(actor,Double.parseDouble(profile[3]),Double.parseDouble(profile[4]),Double.parseDouble(profile[5]),Float.parseFloat(profile[6]),partial);else manager.a(actor,partial);}catch(Throwable e){outcome=-1;failure=e.toString();e.printStackTrace();}active=-1;Map<String,Object>after=state();IntBuffer ints=(IntBuffer)get(nw.a,nw.class,"e");int[]raw=(int[])get(nw.a,nw.class,"g");observed=index;List<String>words=new ArrayList<>();for(int i=0;i<ints.position();i++)words.add(String.format(Locale.ROOT,"%08x",raw[i]));journal.println(ConstructorJson.json(ConstructorJson.map("event","draw","index",index,"input",List.of(profile),"shadow_only",shadowOnly,"outcome",outcome,"failure",failure,"before",before,"after",after,"baseline_pixels",beforePixels,"pixels",pixels("draw"+index),"nw_position",ints.position(),"nw_words",words,"gl_error",GL11.glGetError())));journal.flush();System.out.println("SHEEP_DRAW "+index+" "+outcome+" pool="+field(bt.class,"e").getInt(null)+" nw="+ints.position());}
    public static void main(String[]args)throws Exception{need(args.length==2,"output and input directory");output=Path.of(args[0]).toAbsolutePath();inputs=Path.of(args[1]).toAbsolutePath();Locale.setDefault(Locale.ROOT);System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){network++;networkAttempts.add(h+":"+p+" "+Arrays.toString(Thread.currentThread().getStackTrace()));throw new SecurityException("network denied");}public void checkListen(int p){network++;throw new SecurityException("listener denied");}});try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output.resolve("target.ndjson")))){journal=out;resources();jl.a();Display.setDisplayMode(new DisplayMode(256,256));Display.setTitle("Original Sheep renderer observation");try{Display.create();out.println(ConstructorJson.json(ConstructorJson.map("event","device","vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION))));settings=new kv();need(settings.j&&!settings.B,"genuine default shadow/debug flags");Minecraft app=(Minecraft)raw(kq.class);app.z=settings;field(Minecraft.class,"a").set(null,app);world=makeWorld();app.f=world;constructor_case=0;actor=new dl(world);constructed=actor;constructor_case=-1;out.println(ConstructorJson.json(ConstructorJson.map("event","constructor","kind","dl","fields",fields(actor),"bounds",fields(actor.aW),"own_seed",seed(actor.bs),"math_seed",seed(math()),"default_entropy_rebound",false)));constructor_case=1;camera=new bx(world);constructed=camera;constructor_case=-1;out.println(ConstructorJson.json(ConstructorJson.map("event","constructor","kind","bx","fields",fields(camera),"own_seed",seed(camera.bs),"math_seed",seed(math()),"default_entropy_rebound",false)));prepare();need(world.b(actor)&&world.b(camera),"genuine ordinary publication");ik packs=new ik(app,output.toFile());engine=new ji(packs,settings);app.p=engine;texture(0,"/mob/sheep.png");texture(1,"/mob/sheep_fur.png");texture(2,"%clamp%/misc/shadow.png");constructor_case=2;manager=th.a;constructed=manager;constructor_case=-1;out.println(ConstructorJson.json(ConstructorJson.map("event","constructor","kind","th","fields",fields(manager),"map_entries",((Map)get(manager,th.class,"o")).size(),"math_seed",seed(math()),"default_entropy_rebound",false)));manager.a(world,engine,null,camera,settings,.375f);th.b=8.5;th.c=64;th.d=8.5;renderer=manager.a(actor);need(renderer.getClass()==xy.class,"actual canonical Sheep renderer");primary=(ko)get(renderer,gv.class,"e");fur=(ko)get(renderer,gv.class,"f");need(primary.getClass()==mw.class&&fur.getClass()==ea.class&&primary!=fur,"actual MW and EA models");need(!actor.s()&&actor.r()==0&&!actor.ak()&&!actor.al()&&!Minecraft.w(),"real default fur/fire/riding/debug predicates");Arrays.fill((int[])get(nw.a,nw.class,"g"),0);((IntBuffer)get(nw.a,nw.class,"e")).position(0);bt.b();for(String[] profile:table("profiles.tsv"))observe(profile);out.println(ConstructorJson.json(ConstructorJson.map("event","network_guard","blocked_attempts",networkAttempts)));System.out.println("ORIGINAL_MOB_DRAW complete blocked_network="+network);String classFile=System.getProperty("mobdraw.classes");if(classFile!=null)for(String name:Files.readAllLines(Path.of(classFile))){Class<?> c=Class.forName(name.replace('/','.'),false,MobDrawTarget.class.getClassLoader());byte[]bytes;try(InputStream stream=c.getResourceAsStream("/"+name+".class")){bytes=stream.readAllBytes();}System.out.println("BIND "+name+" "+c.getProtectionDomain().getCodeSource().getLocation()+" "+bytes.length+" "+hash(bytes));}}finally{Display.destroy();}}}
}
