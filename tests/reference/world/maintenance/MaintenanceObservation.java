import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Actual constructors/methods; controlled inputs use the actual owned Set.add. */
public final class MaintenanceObservation {
    public static int activeCase=-1;
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object value(Object owner,Class<?> type,String name)throws Exception{return field(type,name).get(owner);}
    static List<Object> states(List<lm> chunks)throws Exception{
        List<Object> out=new ArrayList<>();for(lm chunk:chunks)out.add(Arrays.asList(chunk.j,chunk.k,field(lm.class,"c").get(chunk),field(lm.class,"r").get(chunk),field(lm.class,"o").get(chunk),field(lm.class,"q").get(chunk)));return out;
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=1)throw new IllegalArgumentException("fresh output directory");
        Path root=Path.of(args[0]);Files.createDirectories(root);
        String[] cases={"empty","cached","pending101","missing","lock","item"};
        for(int index=0;index<cases.length;++index){
            String kind=cases[index];
            mx handler=new mx(root.toFile(),kind,false);
            fd world=new fd(handler,"Maintenance",xa.a(0),23L);world.a(123L);
            Object provider=value(world,fd.class,"v");
            if(provider.getClass()!=ok.class)throw new AssertionError("real normal cache");
            Map<?,?> cache=(Map<?,?>)value(provider,ok.class,"e");
            Set<?> pending=(Set<?>)value(provider,ok.class,"a");
            if(!cache.isEmpty()||!pending.isEmpty())throw new AssertionError("direct constructor prefix");
            int count=kind.equals("pending101")?101:kind.equals("cached")||kind.equals("lock")||kind.equals("item")?1:0;
            List<lm> chunks=new ArrayList<>();
            for(int i=0;i<count;++i)chunks.add(world.c(20+i,0));
            if(kind.equals("item")){hl item=new hl(world,328.5,80.0,8.5,new iz(1,1,0));if(!world.b(item))throw new AssertionError("real Item insertion");}
            if(kind.equals("pending101")||kind.equals("lock")||kind.equals("item"))for(int i=0;i<count;++i)
                HashSet.class.getMethod("add",Object.class).invoke(pending,Integer.valueOf(yy.a(20+i,0)));
            if(kind.equals("missing"))HashSet.class.getMethod("add",Object.class).invoke(pending,Integer.valueOf(yy.a(20,0)));
            if(kind.equals("lock"))Files.write(root.resolve(kind).resolve("session.lock"),new byte[8]);
            activeCase=index;String outcome="return";
            try{world.p();}catch(Throwable error){outcome=error.getClass().getName();}
            activeCase=-1;
            List<Object> remaining=new ArrayList<>();for(Object item:cache.values()){lm chunk=(lm)item;remaining.add(Arrays.asList(chunk.j,chunk.k));}
            System.out.println("observation\t"+ConstructorJson.json(ConstructorJson.map("case",kind,"outcome",outcome,"cache",cache.size(),"pending",pending.size(),"states",states(chunks),"remaining",remaining)));
        }
    }
}
