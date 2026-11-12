import java.lang.reflect.Field;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

/** Calls only unchanged genuine client constructors and CPU methods. */
public final class ParticleObservation {
    static Field field(Class<?> c, String n) throws Exception {
        while (c != null) {
            try { Field f = c.getDeclaredField(n); f.setAccessible(true); return f; }
            catch (NoSuchFieldException e) { c = c.getSuperclass(); }
        }
        throw new NoSuchFieldException(n);
    }
    static Object get(Object o, Class<?> c, String n) throws Exception { return field(c,n).get(o); }
    static String hex(long v) { String s=Long.toHexString(v); return "0".repeat(16-s.length())+s; }
    static String bits(Object v) {
        if(v instanceof Double) return hex(Double.doubleToRawLongBits((Double)v));
        if(v instanceof Float) return hex(Float.floatToRawIntBits((Float)v)&0xffffffffL);
        if(v instanceof Boolean) return (Boolean)v?"1":"0";
        return v.toString();
    }
    static int records;
    static void row(Object... a) {
        records++;
        StringBuilder b=new StringBuilder();
        for(Object x:a) { if(b.length()>0)b.append('\t');b.append(x); }
        System.out.println(b);
        if(System.out.checkError())throw new AssertionError("stdout");
    }
    static long state(Random r) throws Exception { return ((AtomicLong)get(r,Random.class,"seed")).get(); }
    static long u() throws Exception { return ((AtomicLong)get(null,Random.class,"seedUniquifier")).get(); }
    static Random math() throws Exception {
        Class<?> c=Class.forName("java.lang.Math$RandomNumberGeneratorHolder");
        return (Random)get(null,c,"randomNumberGenerator");
    }
    static void particle(String label, em p, sn picked, sn target) throws Exception {
        StringBuilder b=new StringBuilder("particle\t"+label);
        String[] base={"aD","aM","aN","aO","aP","aQ","aR","bf","bg","bh","be"};
        for(String n:base)b.append('\t').append(bits(get(p,sn.class,n)));
        Object box=get(p,sn.class,"aW");
        for(String n:new String[]{"a","b","c","d","e","f"})b.append('\t').append(bits(get(box,eq.class,n)));
        for(String n:new String[]{"b","c","d","e","f","g","h","i","j","k"})b.append('\t').append(bits(get(p,xw.class,n)));
        for(String n:new String[]{"p","q","r"})b.append('\t').append(bits(get(p,em.class,n)));
        b.append('\t').append(get(p,em.class,"a")==picked?1:0);
        b.append('\t').append(get(p,em.class,"o")==target?1:0);
        b.append('\t').append(get(p,sn.class,"aI")==null?1:0);
        Random r=(Random)get(p,sn.class,"bs");
        b.append('\t').append(hex(state(r))).append('\t').append(hex(state(math()))).append('\t').append(hex(u()));
        row(b.toString());
    }
    static Object managerArray;static Object[] managerLists;
    static void manager(String label,dn m,em a,em b) throws Exception {
        List<?>[] lists=(List<?>[])get(m,dn.class,"b");
        StringBuilder s=new StringBuilder("manager\t"+label);
        s.append('\t').append(lists.length);
        boolean distinct=true;for(int i=0;i<4;i++)for(int j=0;j<i;j++)distinct&=lists[i]!=lists[j];
        s.append('\t').append(distinct?1:0);
        if(managerArray==null){managerArray=lists;managerLists=lists.clone();}
        boolean stable=managerArray==lists;
        for(int i=0;i<4;i++)stable&=managerLists[i]==lists[i];
        s.append('\t').append(stable?1:0);
        for(List<?> l:lists) {
            s.append('\t').append(l.size());
            Object[] cells=(Object[])get(l,java.util.ArrayList.class,"elementData");
            s.append('\t').append(cells.length);
        }
        List<?> l=lists[3];
        s.append('\t').append(l.isEmpty()?0:l.get(0)==a?1:l.get(0)==b?2:3);
        s.append('\t').append(l.isEmpty()?0:l.get(l.size()-1)==a?1:l.get(l.size()-1)==b?2:3);
        s.append('\t').append(get(m,dn.class,"a")==null?1:0).append('\t').append(get(m,dn.class,"c")==null?1:0);
        s.append('\t').append(hex(state((Random)get(m,dn.class,"d"))));row(s.toString());
    }
    static final long[][] CASES={
        {0,0,0,0,0,0,0},
        {0x3ff4000000000000L,0xc000000000000000L,0x400e000000000000L,0x3fd0000000000000L,0xbfe0000000000000L,0x3ff0000000000000L,0x3f800000L},
        {0x7fefffffffffffffL,0xffefffffffffffffL,1,0x7fefffffffffffffL,0xffefffffffffffffL,1,0x80000000L},
        {0x7ff0000000000000L,0xfff0000000000000L,0,0x7ff0000000000000L,0xfff0000000000000L,0,0x7fc12345L},
        {0x7ff8123456789abcL,0xfff8fedcba987654L,0,0x7ff8123456789abcL,0xfff8fedcba987654L,0,0x7f800000L},
        {0x8000000000000000L,1,0x8000000000000001L,1,0x8000000000000001L,0x0010000000000000L,0xff800000L},
        {0,0,0,0x7ff0000000000001L,0x3ff0000000000000L,0xfff0000000000001L,0x7f812345L},
        {0x3ff0000000000000L,0x4000000000000000L,0x4008000000000000L,0,0,0,0xffc12345L}
    };
    public static void main(String[] args) throws Exception {
        if(args.length!=2)throw new IllegalArgumentException("verified jar origins file");
        Path jar=Path.of(args[0]).toRealPath();
        StringBuilder origins=new StringBuilder();
        for(String name:new String[]{"em","xw","sn","in","dc","dn","ju","eq"}) {
            Class<?> type=Class.forName(name,false,ParticleObservation.class.getClassLoader());
            Path origin=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!Files.isSameFile(jar,origin))throw new AssertionError("wrong actual class origin: "+name);
            origins.append(name).append('\t').append(origin).append('\n');
        }
        Files.writeString(Path.of(args[1]),origins,StandardOpenOption.CREATE_NEW);
        Class.forName("java.util.Random");
        ju picked=new ju((fd)null), target=new ju((fd)null);
        dn manager=new dn(null,null);
        manager("constructed",manager,null,null);
        Random mr=(Random)get(manager,dn.class,"d");
        row("manager-next",hex(mr.nextLong()),hex(state(mr)));
        long beforeCold=u();int counterCold=field(sn.class,"a").getInt(null);
        try {new em(null,null,target,1);throw new AssertionError("cold null accepted");}
        catch(NullPointerException expected){row("null-cold",u()==beforeCold?1:0,field(sn.class,"a").getInt(null)==counterCold?1:0);}
        for(int i=0;i<CASES.length;i++) {
            long[] c=CASES[i];
            String[] n={"aM","aN","aO","aP","aQ","aR"};
            for(int j=0;j<n.length;j++)field(sn.class,n[j]).setDouble(picked,Double.longBitsToDouble(c[j]));
            sn t=i==6?null:i==7?picked:target;
            em p=new em(null,picked,t,Float.intBitsToFloat((int)c[6]));
            particle("ctor"+i,p,picked,t);
            Random r=(Random)get(p,sn.class,"bs");
            row("next",i,bits(r.nextFloat()),bits(Math.random()),hex(state(r)),hex(state(math())));
            for(int j=1;j<=4;j++){p.w_();particle("tick"+i+"-"+j,p,picked,t);}
            row("layer",i,p.c_());
            if(i==0){field(em.class,"p").setInt(p,Integer.MAX_VALUE);field(em.class,"q").setInt(p,Integer.MIN_VALUE);field(sn.class,"be").setBoolean(p,false);p.w_();particle("wrap",p,picked,t);}
        }
        long before=u();int counter=field(sn.class,"a").getInt(null);
        try {new em(null,null,target,1);throw new AssertionError("null accepted");}
        catch(NullPointerException expected){row("null",u()==before?1:0,field(sn.class,"a").getInt(null)==counter?1:0);}
        em first=new em(null,picked,target,0), second=new em(null,picked,target,0);
        manager.a(first);manager("one",manager,first,second);
        for(int i=0;i<3;i++){manager.a();manager("tick"+i,manager,first,second);}
        field(em.class,"p").setInt(first,0);field(sn.class,"be").setBoolean(first,false);
        manager.a(first);for(int i=0;i<4000;i++)manager.a(second);
        manager("cap",manager,first,second);
        manager.a();manager("duplicates1",manager,first,second);
        row("duplicate-state",get(first,em.class,"p"),get(second,em.class,"p"),get(second,sn.class,"be"));
        manager.a();manager("duplicates2",manager,first,second);
        manager.a(second);manager.a((fd)null);manager("clear",manager,first,second);
        row("final",hex(u()),hex(state(math())));
        row("Original particles: "+records+" records");
    }
}
