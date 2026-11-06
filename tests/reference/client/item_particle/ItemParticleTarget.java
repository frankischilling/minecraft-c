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

/** Authored ordinary observer; genuine unchanged PB/XW/SN/World/N/DN. */
@SuppressWarnings({"unchecked","rawtypes","removal"})
public final class ItemParticleTarget {
    public static int active=-1,constructor_case=-1,texture_case=-1,observed=-1;
    public static fd world;public static pb particle;public static uw slime;public static bx camera;public static dn effects;public static n renderer;public static Minecraft app;public static ji engine;public static kv settings;public static gm item;public static Object constructed;
    static final List<pb>all=new ArrayList<>();static final IdentityHashMap<Object,Integer>ids=new IdentityHashMap<>();static final List<String>networkAttempts=new ArrayList<>();static Path output,inputs;static String mode;static PrintWriter journal;
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
    static fd makeWorld()throws Exception{fd w=new fd(new wy(),"ItemParticleTarget",xa.a(0),23L);w.B=false;w.q=1;Object provider=get(w,fd.class,"v");Map map=(Map)get(provider,ok.class,"e");List list=(List)get(provider,ok.class,"f");map.clear();list.clear();for(int x=-2;x<=2;x++)for(int z=-2;z<=2;z++){lm c=new lm(w,new byte[32768],x,z);c.n=true;for(int a=0;a<16;a++)for(int b=0;b<16;b++)c.b[(a<<11)|(b<<7)|63]=1;Arrays.fill(c.h,(byte)64);c.i=64;Arrays.fill((byte[])get(c.f,wi.class,"a"),(byte)255);map.put(yy.a(x,z),c);list.add(c);}((ei)get(w,fd.class,"x")).a(6000L);w.k();return w;}
    static Map<String,Object> entity(sn e)throws Exception {
        ud w=(ud)get(e,sn.class,"bD");Map records=(Map)get(w,ud.class,"b");List<Integer>keys=new ArrayList<>(records.keySet());Collections.sort(keys);List<Object>watch=new ArrayList<>();
        for(int key:keys)watch.add(ConstructorJson.map("index",key,"record",value(records.get(key)),"fields",fields(records.get(key))));
        return ConstructorJson.map("actual",value(e),"fields",fields(e),"bounds",fields(e.aW),"watcher",value(w),"watcher_dirty",get(w,ud.class,"c"),"watcher_records",watch);
    }
    static Map<String,Object> gl()throws Exception{FloatBuffer b=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(GL11.GL_CURRENT_COLOR,b);List<String>color=new ArrayList<>();for(int i=0;i<4;i++)color.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,b);List<String>mv=new ArrayList<>();for(int i=0;i<16;i++)mv.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_PROJECTION_MATRIX,b);List<String>pr=new ArrayList<>();for(int i=0;i<16;i++)pr.add(hex(b.get(i)));GL11.glGetFloat(GL11.GL_CURRENT_NORMAL,b);List<String>normal=new ArrayList<>();for(int i=0;i<3;i++)normal.add(hex(b.get(i)));List<Object>caps=new ArrayList<>();for(int cap:new int[]{GL11.GL_CULL_FACE,GL11.GL_BLEND,GL11.GL_ALPHA_TEST,GL11.GL_DEPTH_TEST,GL11.GL_LIGHTING,32826,GL11.GL_NORMALIZE,GL11.GL_TEXTURE_2D,GL11.GL_FOG})caps.add(ConstructorJson.map("cap",cap,"enabled",GL11.glIsEnabled(cap)));return ConstructorJson.map("color",color,"normal",normal,"modelview",mv,"projection",pr,"mode",GL11.glGetInteger(GL11.GL_MATRIX_MODE),"modelview_depth",GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),"projection_depth",GL11.glGetInteger(GL11.GL_PROJECTION_STACK_DEPTH),"depth_mask",GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK),"depth_function",GL11.glGetInteger(GL11.GL_DEPTH_FUNC),"blend_src",GL11.glGetInteger(GL11.GL_BLEND_SRC),"blend_dst",GL11.glGetInteger(GL11.GL_BLEND_DST),"texture",GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D),"caps",caps);}
    static void frame(boolean preserveNormalize){GL11.glViewport(0,0,256,256);GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glOrtho(-2,2,-1.5,2.5,.1,50);GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();GL11.glTranslatef(0,-.25f,-6);GL11.glRotatef(18,1,0,0);GL11.glRotatef(-25,0,1,0);GL11.glClearColor(.1f,.15f,.2f,1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT|GL11.GL_DEPTH_BUFFER_BIT);GL11.glEnable(GL11.GL_DEPTH_TEST);GL11.glDepthFunc(GL11.GL_LEQUAL);GL11.glDepthMask(true);GL11.glDisable(GL11.GL_CULL_FACE);GL11.glEnable(GL11.GL_TEXTURE_2D);GL11.glEnable(GL11.GL_ALPHA_TEST);GL11.glAlphaFunc(GL11.GL_GREATER,.1f);GL11.glDisable(GL11.GL_BLEND);GL11.glDisable(GL11.GL_LIGHTING);GL11.glDisable(GL11.GL_FOG);GL11.glDisable(32826);if(!preserveNormalize)GL11.glDisable(GL11.GL_NORMALIZE);GL11.glColor4f(.125f,.25f,.5f,.75f);GL11.glNormal3f(.125f,.25f,.5f);}
    static Object pixels(String filename)throws Exception{GL11.glFinish();ByteBuffer bytes=BufferUtils.createByteBuffer(256*256*4);GL11.glReadPixels(0,0,256,256,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,bytes);byte[]raw=new byte[bytes.capacity()];bytes.get(raw);Files.write(output.resolve(filename+".rgba"),raw);BufferedImage image=new BufferedImage(256,256,BufferedImage.TYPE_INT_ARGB);for(int y=0;y<256;y++)for(int x=0;x<256;x++){int i=(y*256+x)*4;image.setRGB(x,255-y,((raw[i+3]&255)<<24)|((raw[i]&255)<<16)|((raw[i+1]&255)<<8)|(raw[i+2]&255));}ImageIO.write(image,"png",output.resolve(filename+".png").toFile());return ConstructorJson.map("file",filename+".rgba","sha256",hash(raw));}

    static void nullSnowInput()throws Exception{Class<?>u=Class.forName("sun.misc.Unsafe");Object unsafe=field(u,"theUnsafe").get(null);Field target=field(uu.class,"aV");Object base=u.getMethod("staticFieldBase",Field.class).invoke(unsafe,target);long offset=(Long)u.getMethod("staticFieldOffset",Field.class).invoke(unsafe,target);u.getMethod("putObjectVolatile",Object.class,long.class,Object.class).invoke(unsafe,base,offset,null);need(target.get(null)==null,"declared pre-call static-final Snow reference NULL input");journal.println(ConstructorJson.json(ConstructorJson.map("event","source_input","operation","Unsafe static reference write before observation","field","uu.aV","value",null)));}
    static List<?>[] layers()throws Exception{return(List<?>[])get(effects,dn.class,"b");}
    static List<?> layer()throws Exception{return layers()[2];}
    static Object manager()throws Exception{
        List<?>[]a=layers();List<Object>rows=new ArrayList<>();
        for(List<?>list:a){Object[]cells=(Object[])get(list,ArrayList.class,"elementData");List<Object>values=new ArrayList<>();for(Object v:cells)values.add(value(v));rows.add(ConstructorJson.map("actual",value(list),"backing",value(cells),"size",list.size(),"capacity",cells.length,"mod_count",get(list,AbstractList.class,"modCount"),"values",values));}
        return ConstructorJson.map("actual",value(effects),"fields",fields(effects),"array",value(a),"lists",rows,"random_seed",seed((Random)get(effects,dn.class,"d")));
    }
    static Object streams()throws Exception{return ConstructorJson.map("math",seed(math()),"world",seed(world.r),"camera",seed(camera.bs),"particle",particle==null?null:seed(particle.bs),"slime",slime==null?null:seed(slime.bs),"dn",seed((Random)get(effects,dn.class,"d")),"uniquifier",value(((AtomicLong)get(null,Random.class,"seedUniquifier")).get()));}
    static Object nwState()throws Exception{Map<String,Object>r=fields(nw.a);IntBuffer b=(IntBuffer)get(nw.a,nw.class,"e");int count=Math.max(b.position(),field(nw.class,"o").getInt(nw.a));need(count>=0&&count<2048,"bounded selected backing");int[]g=(int[])get(nw.a,nw.class,"g");List<String>words=new ArrayList<>();for(int i=0;i<count;i++)words.add(String.format(Locale.ROOT,"%08x",g[i]));List<String>prefix=new ArrayList<>();for(int i=0;i<1024;i++)prefix.add(String.format(Locale.ROOT,"%08x",g[i]));return ConstructorJson.map("actual",value(nw.a),"fields",r,"position",b.position(),"selected_words",words,"backing_prefix",prefix);}
    static Object textureState()throws Exception{
        Map<?,?>cache=(Map<?,?>)get(engine,ji.class,"b");Map<String,Object>r=ConstructorJson.map("cache",new TreeMap<>(cache),"clamp",get(engine,ji.class,"j"),"blur",get(engine,ji.class,"k"),"bound",GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D));
        if(cache.containsKey("/gui/items.png")){need(((Integer)r.get("bound")).intValue()==((Integer)cache.get("/gui/items.png")).intValue(),"actual selected atlas remains bound");for(int key:new int[]{GL11.GL_TEXTURE_WRAP_S,GL11.GL_TEXTURE_WRAP_T,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_TEXTURE_MAG_FILTER})r.put(Integer.toString(key),GL11.glGetTexParameteri(GL11.GL_TEXTURE_2D,key));r.put("width",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_WIDTH));r.put("height",GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D,0,GL11.GL_TEXTURE_HEIGHT));ByteBuffer buffer=(ByteBuffer)get(engine,ji.class,"f");need(buffer.limit()==256*256*4,"actual atlas transfer extent");byte[]bytes=new byte[buffer.limit()];for(int i=0;i<bytes.length;i++)bytes[i]=buffer.get(i);r.put("transfer_sha256",hash(bytes));r.put("transfer_hex",HexFormat.of().formatHex(bytes));r.put("transfer_position",buffer.position());r.put("transfer_limit",buffer.limit());}
        return r;
    }
    static Object state()throws Exception{
        List<Object>particles=new ArrayList<>();for(Object x:layer())particles.add(entity((sn)x));
        return ConstructorJson.map("particle",particle==null?null:entity(particle),"camera",entity(camera),"slime",slime==null?null:entity(slime),"effects",manager(),"layer_particles",particles,"renderer",value(renderer),"renderer_world",value(get(renderer,n.class,"k")),"app_dn",value(app.j),"world_accesses",value(get(world,fd.class,"u")),"world_access_count",((List<?>)get(world,fd.class,"u")).size(),"streams",streams(),"origins",List.of(value(xw.l),value(xw.m),value(xw.n)),"box_cursor",get(null,eq.class,"h"),"vector_cursor",get(null,bt.class,"e"),"nw",nwState(),"gl",gl(),"texture",textureState(),"texture_cache_size",((Map<?,?>)get(engine,ji.class,"b")).size());
    }
    static void resources()throws Exception{
        for(String name:List.of("pack.png","gui/items.png")){int width=name.startsWith("gui")?256:64,height=name.startsWith("gui")?256:32;BufferedImage image=new BufferedImage(width,height,BufferedImage.TYPE_INT_ARGB);for(int y=0;y<height;y++)for(int x=0;x<width;x++)image.setRGB(x,y,0xff000000|(((x^y)&255)<<16)|((x&255)<<8)|(y&255));Path path=output.resolve("resources").resolve(name);Files.createDirectories(path.getParent());ImageIO.write(image,"png",path.toFile());java.net.URL selected=i.class.getResource("/"+name);need(selected!=null&&Path.of(selected.toURI()).toRealPath().equals(path.toRealPath()),"real authored classpath resource");journal.println(ConstructorJson.json(ConstructorJson.map("event","resource_input","resource",name,"sha256",hash(Files.readAllBytes(path)),"runtime_url",selected.toString())));}
    }
    static void rawConstructor(int index,fd owner,gm operand)throws Exception{
        Object before=streams();particle=null;constructed=null;constructor_case=index;
        try{particle=new pb(owner,8.5,65.5,8.5,operand);constructed=particle;all.add(particle);}finally{constructor_case=-1;}
        journal.println(ConstructorJson.json(ConstructorJson.map("event","constructor_context","case",index,"before",before,"after",streams(),"item_operand",value(operand),"particle",entity(particle),"default_entropy_rebound",false)));
    }
    static List<String[]> table(String name)throws Exception{List<String[]>rows=new ArrayList<>();for(String line:Files.readAllLines(inputs.resolve(name)))if(!line.isBlank()&&!line.startsWith("#")&&!line.startsWith("case\t")&&!line.startsWith("target\t"))rows.add(line.split("\\s+"));return rows;}
    static void prepare(Object receiver,String target,String scenario)throws Exception{
        for(String[]p:table("preparation.tsv"))if(p[0].equals(target)&&(p[1].equals("all")||p[1].equals(scenario))){need(p.length==6,"preparation row width");if(p[4].equals("position")){String[]v=p[5].split(",");((sn)receiver).e(Double.parseDouble(v[0]),Double.parseDouble(v[1]),Double.parseDouble(v[2]));}
            else if(p[4].equals("pose")){String[]v=p[5].split(",");((sn)receiver).c(Double.parseDouble(v[0]),Double.parseDouble(v[1]),Double.parseDouble(v[2]),Float.parseFloat(v[3]),Float.parseFloat(v[4]));}
            else if(p[4].equals("size")){((uw)receiver).e(Integer.parseInt(p[5]));}
            else{Field f=field(Class.forName(p[2]),p[3]);switch(p[4]){case"D":f.setDouble(receiver,Double.parseDouble(p[5]));break;case"F":f.setFloat(receiver,Float.parseFloat(p[5]));break;case"I":f.setInt(receiver,Integer.parseInt(p[5]));break;case"Z":f.setBoolean(receiver,Boolean.parseBoolean(p[5]));break;default:throw new AssertionError("preparation type");}}}
    }
    static void prep(String scenario)throws Exception{prepare(particle,"particle",scenario);}
    static void clear()throws Exception{effects.a(world);}
    static void action(int index,String operation,String scenario,float partial)throws Exception{
        String[]profile=table("profiles.tsv").stream().filter(p->Integer.parseInt(p[0])==index).findFirst().orElseThrow();need(profile[1].equals(mode)&&profile[2].equals(operation)&&profile[3].equals(scenario)&&Float.floatToRawIntBits(Float.parseFloat(profile[4]))==Float.floatToRawIntBits(partial),"authored exact profile input");frame(false);Object before=state(),baseline=pixels("baseline"+index);Throwable failure=null;active=index;
        try{
            switch(operation){
                case"emit":world.a("slime",8.5,65.5,8.5,.75,-.5,.25);break;
                case"construct":rawConstructor(index,scenario.equals("null_world")?null:world,scenario.equals("null_item")?null:item);break;
                case"tick":particle.w_();break;
                case"dn_tick":effects.a();break;
                case"draw":effects.a(camera,partial);break;
                case"guard":app.j=null;world.a("slime",8.5,65.5,8.5,.75,-.5,.25);break;
                case"landing":slime.w_();break;
                default:throw new AssertionError(operation);
            }
        }catch(Throwable e){failure=e;}finally{constructor_case=-1;active=-1;}
        if(operation.equals("emit")&&failure==null){need(layer().size()==1&&layer().get(0).getClass()==pb.class,"actual World N PB enqueue");particle=(pb)layer().get(0);all.add(particle);}
        Object after=state();observed=index;
        List<String>stack=new ArrayList<>();if(failure!=null)for(StackTraceElement e:failure.getStackTrace())stack.add(e.toString());
        journal.println(ConstructorJson.json(ConstructorJson.map("event","operation","case",index,"operation",operation,"scenario",scenario,"input",List.of(profile),"partial",value(partial),"outcome",failure==null?1:-1,"failure_class",failure==null?null:failure.getClass().getName(),"failure_stack",stack,"before",before,"after",after,"baseline_pixels",baseline,"pixels",pixels("result"+index),"gl_error",GL11.glGetError())));journal.flush();
        System.out.println("ITEM_PARTICLE "+index+" "+(failure==null?1:-1)+" layer2="+layer().size());
    }
    static void setup()throws Exception{
        resources();jl.a();settings=new kv();app=(Minecraft)raw(kq.class);app.z=settings;field(Minecraft.class,"a").set(null,app);world=makeWorld();app.f=world;
        camera=new bx(world);prepare(camera,"camera","all");app.i=camera;need(world.b(camera),"real ordinary camera publication");
        field(Minecraft.class,"S").set(app,new cx());ik packs=new ik(app,output.toFile());engine=new ji(packs,settings);app.p=engine;
        effects=new dn(world,engine);app.j=effects;renderer=new n(app,engine);app.g=renderer;field(n.class,"k").set(renderer,world);world.a((pm)renderer);
        nw.a.b();nw.a.b(.25f,-.5f,.75f);nw.a.a();
        Arrays.fill((int[])get(nw.a,nw.class,"g"),0);int[]g=(int[])get(nw.a,nw.class,"g");for(int i=0;i<128;i++){g[i*8+6]=0x10203000+i;g[i*8+7]=0x20304000+i;}((IntBuffer)get(nw.a,nw.class,"e")).position(0);bt.b();eq.a();
        journal.println(ConstructorJson.json(ConstructorJson.map("event","prepared_context","app",value(app),"app_fields",fields(app),"renderer",value(renderer),"renderer_fields",fields(renderer),"world",value(world),"effects",manager(),"camera",entity(camera),"streams",streams(),"default_entropy_rebound",false)));
    }
    static void mainMode()throws Exception{
        action(0,"emit","slime",0);item=gm.aK;
        action(1,"construct","normal",0);action(2,"construct","null_world",0);
        rawConstructor(100,world,item);prep("air");action(3,"tick","air",0);
        rawConstructor(101,world,item);prep("ground");action(4,"tick","ground",0);
        rawConstructor(102,world,item);prep("expiry_air");action(5,"tick","expiry_air",0);
        rawConstructor(103,world,item);prep("expiry_air");clear();effects.a(particle);action(6,"dn_tick","expiry",0);
        rawConstructor(104,world,item);prep("render");clear();effects.a(particle);action(7,"draw","first",.375f);action(8,"draw","reuse",1);
        action(9,"guard","initial_dn_null",0);app.j=effects;
        clear();slime=new uw(world);prepare(slime,"slime","all");need(world.b(slime),"actual Slime publication");action(10,"landing","slime",0);
        need(layer().size()==8,"actual size1 landing produced eight PB");int index=0;for(Object value:layer()){need(value.getClass()==pb.class,"real PB class");particle=(pb)value;all.add(particle);prep("air");particle.e(8.25+index*.125,66,8.5);index++;}
        action(11,"dn_tick","landing",0);action(12,"draw","landing",.375f);
    }
    public static void main(String[]args)throws Exception{
        need(args.length==3,"output input-directory and mode");output=Path.of(args[0]).toAbsolutePath();inputs=Path.of(args[1]).toAbsolutePath();mode=args[2];Locale.setDefault(Locale.ROOT);
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){networkAttempts.add(h+":"+p+" "+Arrays.toString(Thread.currentThread().getStackTrace()));throw new SecurityException("network denied");}public void checkListen(int p){throw new SecurityException("listener denied");}});
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("target.ndjson")))){journal=writer;Display.setDisplayMode(new DisplayMode(256,256));Display.setTitle("Original item particle");try{Display.create();journal.println(ConstructorJson.json(ConstructorJson.map("event","device","vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION))));setup();
            switch(mode){case"main":mainMode();break;case"null_item":action(13,"construct","null_item",0);break;case"null_snow":item=gm.aK;nullSnowInput();action(14,"construct","null_snow",0);break;case"null_world_tick":item=gm.aK;rawConstructor(105,null,item);prep("air");action(15,"tick","null_world",0);break;case"null_world_draw":item=gm.aK;rawConstructor(106,null,item);prep("render");effects.a(particle);action(16,"draw","null_world",.375f);break;default:throw new AssertionError(mode);}
            journal.println(ConstructorJson.json(ConstructorJson.map("event","network_guard","blocked_attempts",networkAttempts)));System.out.println("ORIGINAL_ITEM_PARTICLE complete "+mode);
            String classFile=System.getProperty("itemparticle.classes");if(classFile!=null)for(String name:Files.readAllLines(Path.of(classFile))){Class<?>c=Class.forName(name.replace('/','.'),false,ItemParticleTarget.class.getClassLoader());byte[]bytes;try(InputStream in=c.getResourceAsStream("/"+name+".class")){bytes=in.readAllBytes();}System.out.println("BIND "+name+" "+c.getProtectionDomain().getCodeSource().getLocation()+" "+bytes.length+" "+hash(bytes));}
        }finally{Display.destroy();}}
    }
}
