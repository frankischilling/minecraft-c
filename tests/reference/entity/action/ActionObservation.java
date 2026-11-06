import java.lang.invoke.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Actual unchanged dc, World, Chunk, Inventory and nearby Entity identities. */
public final class ActionObservation {
    static fd world;static lm chunk;static dc p,other;static yl cart;static hl item;static uo input;
    static Field field(Class<?> owner,String name)throws Exception {Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static int flag(boolean value){return value?1:0;}
    static void nullAt(NullPointerException failure,String method){
        StackTraceElement top=failure.getStackTrace()[0];
        if(!top.getClassName().equals("dc")||!top.getMethodName().equals(method))
            throw new AssertionError("unexpected actual null prefix",failure);
    }
    static void reset()throws Exception {
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
        for(int x=6;x<=12;++x)for(int z=6;z<=12;++z)chunk.b[(x<<11)|(z<<7)|7]=1;
        p.bf=p.bo=p.bp=0;field(sn.class,"bk").setFloat(p,0);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;p.bq=false;
        field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.bj=0;field(sn.class,"b").setInt(p,1);
        p.b(1f,1f);p.e(8.5,8,8.5);p.aP=p.aQ=p.aR=0;p.by=0;p.aX=true;p.bt=1;p.aS=p.aT=0;p.br=0;
        p.Y=20;p.E=20;p.V=false;p.an=0;p.aw=p.ax=p.ay=0;p.az=false;p.ag=.5f;p.X=-1;
        p.ao=10.5;p.ap=8;p.aq=8.5;p.ar=10;p.as=30;
        p.u=p.j=false;p.k=0;p.h=-1;p.i=.125f;world.q=1;world.B=false;
        input.a=input.b=0;input.d=input.e=false;p.a=input;eq.b();bt.b();
        ((Random)field(sn.class,"bs").get(p)).setSeed(38);
        for(sn actor:new sn[]{other,cart,item}){actor.e(14,8,14);actor.aP=actor.aR=0;actor.aQ=-0.0;actor.be=false;actor.br=0;}
        item.c=3;
    }
    static void row(int scenario,boolean complete)throws Exception {
        System.out.printf("ACTION %d %d %016x %016x %016x %016x %016x %016x",scenario,flag(complete),
            Double.doubleToRawLongBits(p.aM),Double.doubleToRawLongBits(p.aN),Double.doubleToRawLongBits(p.aO),
            Double.doubleToRawLongBits(p.aP),Double.doubleToRawLongBits(p.aQ),Double.doubleToRawLongBits(p.aR));
        System.out.printf(" %08x %08x %08x %08x %08x %d %08x %d %d",Float.floatToRawIntBits(p.aS),Float.floatToRawIntBits(p.aT),
            Float.floatToRawIntBits(p.aw),Float.floatToRawIntBits(p.ax),Float.floatToRawIntBits(p.ay),flag(p.az),Float.floatToRawIntBits(p.X),flag(p.j),p.k);
        System.out.printf(" %08x %08x %08x %d %d %d %d %d %d %08x %d",Float.floatToRawIntBits(p.h),Float.floatToRawIntBits(p.i),
            Float.floatToRawIntBits(p.ag),p.Y,p.by,p.an,flag(p.aX),flag(p.aY),flag(p.aZ),Float.floatToRawIntBits(p.bj),field(sn.class,"b").getInt(p));
        System.out.printf(" %016x %016x %016x %016x %016x %016x",Double.doubleToRawLongBits(p.aW.a),Double.doubleToRawLongBits(p.aW.b),
            Double.doubleToRawLongBits(p.aW.c),Double.doubleToRawLongBits(p.aW.d),Double.doubleToRawLongBits(p.aW.e),Double.doubleToRawLongBits(p.aW.f));
        for(sn actor:new sn[]{other,cart,item})System.out.printf(" %016x %016x %016x",Double.doubleToRawLongBits(actor.aP),Double.doubleToRawLongBits(actor.aQ),Double.doubleToRawLongBits(actor.aR));
        Random random=(Random)field(sn.class,"bs").get(p);long seed=((AtomicLong)field(Random.class,"seed").get(random)).get();
        System.out.printf(" %012x%n",seed);
    }
    public static void main(String[] args)throws Throwable {
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        world=new fd(new mx(root.toFile(),"Action",false),"Action",xa.a(0),23L);chunk=world.c(0,0);
        if(jl.x==null)throw new AssertionError("actual prepared statistics");gr session=new gr(null,null);
        p=new dc(null,world,session,0);other=new dc(null,world,session,0);cart=new yl(world,14,8,14,0);item=new hl(world,14,8,14,new iz(262,5,7));
        if(cart.f()!=null||cart.a((sn)other)!=other.aW)throw new AssertionError("actual distinct Minecart shape virtuals");
        input=new uo();other.e(14,8,14);for(sn actor:new sn[]{other,cart,item})if(!world.b(actor))throw new AssertionError("actual publication failed");
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> type:new Class<?>[]{fd.class,lm.class,dc.class,gs.class,ls.class,sn.class,uo.class,ix.class,iz.class,hl.class,yl.class,jl.class,in.class}){
            Path origin=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign runtime class "+type);System.out.println("ORIGIN "+type.getName()+" "+origin);
        }
        // Special invocation selects the actual ancestor body. All internal
        // virtual calls still use this unchanged concrete dc receiver.
        MethodHandle action=MethodHandles.privateLookupIn(dc.class,MethodHandles.lookup()).findSpecial(gs.class,"o",MethodType.methodType(void.class),dc.class);
        int[] counters={0,6,7,8,-2,Integer.MAX_VALUE,Integer.MIN_VALUE,Integer.MAX_VALUE,6};
        boolean[] active={true,true,true,true,true,true,true,false,true};
        for(int i=0;i<counters.length;++i){reset();p.j=active[i];p.k=counters[i];input.a=-.25f;input.b=.75f;input.d=true;
            if(i==8)p.a=null;boolean complete=false;
            try{p.f_();complete=true;}catch(NullPointerException failure){if(i!=8)throw failure;nullAt(failure,"f_");}
            if(complete!=(i!=8))throw new AssertionError("input completion differs "+i);
            System.out.printf("INPUT %d %d %d %d %08x %08x %08x %d%n",i,flag(complete),flag(p.j),p.k,Float.floatToRawIntBits(p.X),Float.floatToRawIntBits(p.aw),Float.floatToRawIntBits(p.ax),flag(p.az));
        }
        for(int scenario=0;scenario<28;++scenario){
            if(scenario==11)continue; // Native-only reached Inventory insertion frontier.
            reset();if(scenario==1){world.q=0;p.Y=19;p.bt=20;}
            if(scenario==2)p.Y=0;if(scenario==3)p.u=true;if(scenario==4)p.V=true;
            if(scenario>=2&&scenario<=4){input.a=1;input.b=-1;input.d=true;}
            if(scenario==5||scenario==6){p.an=2;p.aS=350;p.aT=10;if(scenario==6)chunk.b[(9<<11)|(8<<7)|8]=1;}
            if(scenario==7){p.j=true;p.k=6;}
            if(scenario==8)other.e(9.1,8,8.5);if(scenario==9)cart.e(9.1,8,8.5);if(scenario==10)item.e(9.1,8,8.5);
            if(scenario==12)input.d=true;if(scenario==13){p.aP=.5;input.a=.5f;input.b=1;}
            if(scenario==14){p.a=null;p.j=true;p.k=2;}
            if(scenario==15){p.an=2;p.as=Double.POSITIVE_INFINITY;}
            if(scenario==16){p.an=2;p.ar=Double.longBitsToDouble(0x7ff8123456789abcL);}
            if(scenario==17||scenario==18||scenario==23||scenario==26)other.e(9.1,8,8.9);
            if(scenario==19||scenario==20||scenario==21||scenario==22||scenario==27)cart.e(9.1,8,8.9);
            if(scenario==18)other.br=1;if(scenario==20)cart.br=.5f;if(scenario==21)world.B=true;
            if(scenario==22)cart.be=true;if(scenario==23)other.be=true;
            if(scenario==24)other.e(8.50001,8,8.5);if(scenario==25)cart.e(8.50001,8,8.5);
            if(scenario==26)other.br=Float.intBitsToFloat(0x7fc12345);if(scenario==27)cart.br=Float.intBitsToFloat(0x7fc12345);
            boolean expected=!(scenario==12||scenario==13||scenario==14),complete=false;
            try{action.invoke(p);complete=true;}catch(NullPointerException failure){if(expected)throw failure;nullAt(failure,scenario==14?"f_":"a");}
            if(complete!=expected)throw new AssertionError("completion differs "+scenario);row(scenario,complete);
        }
        System.out.println("Original Player action: 36 states");
    }
}
