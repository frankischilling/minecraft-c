import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Authored callers/operands; every damage body is unchanged original. */
public final class ZombieDamageTarget {
    static Object rawContext(sn e)throws Exception{return ConstructorJson.map("fields",MobTickTarget.fields(e),"bounds",MobTickTarget.fields(e.aW),"watcher",MobTickTarget.watcher(e),"own_seed",MobTickTarget.seed(e.bs));}
    static void field(sn e,String owner,String member,Object value)throws Exception{Field f=MobTickTarget.field(Class.forName(owner),member);f.set(e,value);}
    static void run(String[] p,PrintWriter out)throws Exception{
        int id=Integer.parseInt(p[0]);MobTickTarget.world=MobTickTarget.make();MobTickTarget.actor=null;MobTickTarget.others.clear();MobTickTarget.currentObserver=null;
        List<Object> contexts=new ArrayList<>();sn actor=MobTickTarget.construct("uz",id*2,contexts);MobTickTarget.actor=actor;
        actor.c(8.5,64.5,8.5,30,15);field(actor,"ls","Y",Integer.valueOf(p[5]));field(actor,"ls","av",17);field(actor,"ls","E",20);field(actor,"sn","by",Integer.valueOf(p[6]));field(actor,"ls","au",Integer.valueOf(p[7]));field(actor,"ls","aa",3);field(actor,"ls","ab",4);field(actor,"ls","ac",99f);field(actor,"ls","al",.25f);field(actor,"sn","bb",false);field(actor,"ii","d",null);
        fd world=MobTickTarget.world;world.B=p[4].equals("remote");MobTickTarget.need(world.b(actor),"actual ordinary Zombie publication");sn attacker=null;
        if(p[8].equals("COW")||p[2].equals("existing_target_null_hit")){sn cow=MobTickTarget.construct("bx",id*2+1,contexts);cow.c(11.5,64.5,12.5,0,0);MobTickTarget.others.add(cow);MobTickTarget.need(world.b(cow),"actual ordinary Cow publication");if(p[8].equals("COW"))attacker=cow;else field(actor,"ii","d",cow);}
        if(p[4].equals("NULL"))field(actor,"sn","aI",null);
        if(p[2].equals("null_private_random"))field(actor,"sn","bs",null);
        if(p[1].equals("living_environment")){field(actor,"sn","bv",20);field(actor,"sn","bC",false);field(actor,"ls","a",0);}
        MobTickTarget.Observer observer=null;if(p[9].equals("throw_sound")){observer=new MobTickTarget.Observer(id,"fail_sound");world.a(observer);MobTickTarget.currentObserver=observer;}
        eq.b();bt.b();MobTickTarget.identities.clear();MobTickTarget.value(actor);MobTickTarget.value(world);for(sn e:MobTickTarget.others)MobTickTarget.value(e);
        Map<String,Object> before=MobTickTarget.state();Object result=null;Throwable failure=null;
        MobTickTarget.active=id;
        try{if(p[1].equals("living_environment")){Method m=ls.class.getDeclaredMethod("U");m.setAccessible(true);result=m.invoke(actor);}else result=actor.a(attacker,Integer.parseInt(p[3]));}catch(Throwable t){failure=t instanceof InvocationTargetException?((InvocationTargetException)t).getCause():t;}
        MobTickTarget.active=-1;
        out.println(ConstructorJson.json(ConstructorJson.map("event","damage","case",id,"scene",p[2],"scope",p[10],"input",List.of(p),"result",result,"failure",failure==null?null:failure.getClass().getName(),"failure_text",failure==null?null:failure.toString(),"top",failure==null?null:failure.getStackTrace()[0].toString(),"constructor_contexts",contexts,"before",before,"after",MobTickTarget.state(),"events",observer==null?List.of():observer.events,"actual_entity_published",world.b.contains(actor),"default_constructor_entropy_rebound",false,"no_Random_setSeed",true)));
        out.flush();System.out.println("DAMAGE "+id+" "+(failure==null?String.valueOf(result):failure.getClass().getName()));
    }
    static void bindings(Path jar)throws Exception{
        for(String name:Files.readAllLines(Path.of(System.getProperty("mobtick.classes")))){
            try{Class<?> c=Class.forName(name.replace('/','.'),false,ZombieDamageTarget.class.getClassLoader());Path origin=Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();MobTickTarget.need(origin.equals(jar),"foreign original class");byte[] b;try(InputStream in=c.getResourceAsStream("/"+name+".class")){b=in.readAllBytes();}System.out.println("BIND "+name+" "+c.getProtectionDomain().getCodeSource().getLocation()+" "+b.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b)));}
            catch(LinkageError t){System.out.println("BIND_UNAVAILABLE "+name+" "+t.getClass().getName()+" "+t.getMessage());}
        }
    }
    public static void main(String[]args)throws Exception{
        MobTickTarget.need(args.length==3,"output jar input-dir");Locale.setDefault(Locale.ROOT);new gr("ZombieDamageSetup","");
        Path inputs=Path.of(args[2]);try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){List<String> lines=Files.readAllLines(inputs.resolve("profiles.tsv"));for(int i=1;i<lines.size();i++)run(lines.get(i).split("\t",-1),out);MobTickTarget.need(!out.checkError(),"output");}
        bindings(Path.of(args[1]).toRealPath());System.out.println("ORIGINAL_ZOMBIE_DAMAGE_COMPLETE");
    }
}
