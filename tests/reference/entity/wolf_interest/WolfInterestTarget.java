import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Real constructors plus authored closed-component operands, no virtual replacements. */
public final class WolfInterestTarget {
    static dc player;
    static Object array(iz[] a)throws Exception{if(a==null)return null;List<Object> rows=new ArrayList<>();for(iz s:a)rows.add(s==null?null:ConstructorJson.map("actual",MobTickTarget.value(s),"fields",MobTickTarget.fields(s)));return ConstructorJson.map("actual",MobTickTarget.value(a),"length",a.length,"stacks",rows);}
    static Map<String,Object> state()throws Exception{Map<String,Object> r=MobTickTarget.state();ix inv=player.c;r.put("selected_inventory",inv==null?null:ConstructorJson.map("actual",MobTickTarget.value(inv),"fields",MobTickTarget.fields(inv),"main",array(inv.a),"armor",array(inv.b)));return r;}
    static void run(String[]p,PrintWriter out)throws Exception{
        int id=Integer.parseInt(p[0]);MobTickTarget.world=MobTickTarget.make();MobTickTarget.actor=null;MobTickTarget.others.clear();MobTickTarget.currentObserver=null;
        List<Object> contexts=new ArrayList<>();gi wolf=(gi)MobTickTarget.construct("gi",id*2,contexts);MobTickTarget.actor=wolf;
        String mathBefore=MobTickTarget.seed(MobTickTarget.sharedMath);gr session=new gr("WolfInterestPlayer","");MobTickTarget.constructor_case=id*2+1;MobTickTarget.constructed=null;player=new dc(null,MobTickTarget.world,session,0);MobTickTarget.constructed=player;MobTickTarget.constructor_case=-1;
        contexts.add(ConstructorJson.map("token",id*2+1,"kind","dc","fields",MobTickTarget.fields(player),"bounds",MobTickTarget.fields(player.aW),"watcher",MobTickTarget.watcher(player),"own_seed",MobTickTarget.seed(player.bs),"math_seed_before",mathBefore,"math_seed_after",MobTickTarget.seed(MobTickTarget.sharedMath),"default_entropy_rebound",false));
        MobTickTarget.need(player.c!=null&&player.c.a.length==36&&player.c.b.length==4&&player.c.d==player,"actual constructed Player Inventory");MobTickTarget.others.add(player);
        wolf.c(8.5,64.5,8.5,30,15);player.c(10.5,64.5,8.5,0,0);MobTickTarget.field(ls.class,"V").setBoolean(wolf,true);MobTickTarget.field(ls.class,"b").set(wolf,player);wolf.d(Boolean.parseBoolean(p[3]));
        player.c.c=Integer.parseInt(p[6]);if(!p[2].equals("NULL"))player.c.a[0]=new iz(Integer.parseInt(p[2]),1,0);
        MobTickTarget.need(MobTickTarget.world.b(wolf)&&MobTickTarget.world.b(player),"actual ordinary World publication");
        if(p[5].equals("NULL"))player.c.a=null;if(p[4].equals("NULL"))player.c=null;
        eq.b();bt.b();MobTickTarget.identities.clear();MobTickTarget.value(wolf);MobTickTarget.value(MobTickTarget.world);MobTickTarget.value(player);
        Map<String,Object> before=state();Throwable failure=null;MobTickTarget.active=id;try{wolf.o();}catch(Throwable t){failure=t;}MobTickTarget.active=-1;
        out.println(ConstructorJson.json(ConstructorJson.map("event","interest","case",id,"scene",p[1],"input",List.of(p),"failure",failure==null?null:failure.getClass().getName(),"failure_text",failure==null?null:failure.toString(),"top",failure==null?null:failure.getStackTrace()[0].toString(),"constructor_contexts",contexts,"before",before,"after",state(),"default_constructor_entropy_rebound",false,"closed_component_V",true,"ordinary_loaded_Wolf_or_full_tick_claimed",false)));
        out.flush();System.out.println("INTEREST "+id+" "+(failure==null?"returned":failure));
    }
    static void bindings(Path jar)throws Exception{for(String name:Files.readAllLines(Path.of(System.getProperty("mobtick.classes")))){Class<?> c=Class.forName(name.replace('/','.'),false,WolfInterestTarget.class.getClassLoader());MobTickTarget.need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");byte[] bytes;try(InputStream in=c.getResourceAsStream("/"+name+".class")){bytes=in.readAllBytes();}System.out.println("BIND "+name+" "+c.getProtectionDomain().getCodeSource().getLocation()+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));}}
    public static void main(String[]args)throws Exception{MobTickTarget.need(args.length==3,"output jar inputs");Locale.setDefault(Locale.ROOT);new gr("WolfInterestSetup","");hk.a();
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){List<String> lines=Files.readAllLines(Path.of(args[2]).resolve("profiles.tsv"));for(int i=1;i<lines.size();i++)run(lines.get(i).split("\t",-1),out);MobTickTarget.need(!out.checkError(),"output");}bindings(Path.of(args[1]).toRealPath());System.out.println("ORIGINAL_WOLF_INTEREST_COMPLETE");}
}
