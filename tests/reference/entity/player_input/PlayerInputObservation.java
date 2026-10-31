import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.StringJoiner;

/** Authored input-field assignments and calls on genuine original dc owners. */
public final class PlayerInputObservation {
    private static void npe(dc player, int operation, String method) {
        try {
            if (operation == 0) player.a(17, true);
            else if (operation == 1) player.o_();
            else player.t();
            throw new AssertionError("missing source NPE");
        } catch (NullPointerException expected) {
            StackTraceElement first=expected.getStackTrace()[0];
            if (!first.getClassName().equals("dc") || !first.getMethodName().equals(method))
                throw new AssertionError("wrong source null receiver", expected);
        }
    }
    private static void row(dc player, String label, String status) throws Exception {
        uo input=player.a;
        int mask=0;
        if(input instanceof lr) {
            Field field=lr.class.getDeclaredField("f");
            field.setAccessible(true);
            boolean[] keys=(boolean[])field.get(input);
            if(keys.length!=10) throw new AssertionError("actual input array");
            for(int i=0;i<keys.length;i++) if(keys[i]) mask|=1<<i;
        }
        int sneaking=input==null ? -1 : (player.t() ? 1 : 0);
        System.out.printf(Locale.ROOT,
            "INPUT\t%s\t%s\t%d\t%d\t%08x\t%08x\t%d\t%d\t%d\t%d\t%d\t%d%n",
            label,status,input==null ? 0 : 1,input==null ? -1 : (input instanceof lr ? 1 : 0),
            input==null ? 0 : Float.floatToRawIntBits(input.a),
            input==null ? 0 : Float.floatToRawIntBits(input.b),
            input!=null && input.c ? 1 : 0,input!=null && input.d ? 1 : 0,
            input!=null && input.e ? 1 : 0,mask,player.u ? 1 : 0,sneaking);
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=0) throw new IllegalArgumentException("no arguments");
        Path jar=Path.of(dc.class.getProtectionDomain().getCodeSource().getLocation().toURI());
        StringJoiner digest=new StringJoiner("");
        for(byte value:MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)))
            digest.add(String.format(Locale.ROOT,"%02x",value&255));
        if(!digest.toString().equals("af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"))
            throw new AssertionError("original runtime JAR origin");
        fd world=new fd(new wy(),"PlayerInput",xa.a(0),5143L);
        world.a(new br(2,64,2));
        dc p=new dc(null,world,new gr("Input",null),0);
        dc q=new dc(null,world,new gr("Input",null),0);
        if(p.a!=null || q.a!=null) throw new AssertionError("real constructor input");
        double x=p.aM,y=p.aN,z=p.aO;
        npe(p,0,"a"); npe(p,1,"o_"); npe(p,2,"t");
        row(p,"fresh","NPE");
        uo base=new uo();
        base.a=-0.0f; base.b=1.0f; base.c=true; base.d=true; base.e=true;
        p.a=base; p.a=base;
        p.a(Integer.MIN_VALUE,true); p.o_();
        row(p,"base","OK");
        // Authored source field control, not a claim of bed-entry gameplay.
        p.u=true; row(p,"base-asleep","OK"); p.u=false;
        kv settings=new kv();
        lr options=new lr(settings);
        p.a=options;
        row(p,"options-fresh","OK");
        for(int code:new int[]{17,30,57,42}) p.a(code,true);
        row(p,"options-keys","OK");
        options.a((gs)p); row(p,"options-updated","OK");
        q.a=options;
        if(p.a!=q.a) throw new AssertionError("shared exact source input");
        row(q,"shared","OK");
        q.o_(); row(p,"shared-reset","OK");
        q.a.a((gs)q); row(p,"shared-updated","OK");
        p.a=base;
        if(p.a!=base || q.a!=options) throw new AssertionError("source replacement ownership");
        row(p,"replacement-base","OK");
        q.a(17,true); q.a.a((gs)q); row(q,"remaining-options","OK");
        p.a=null; npe(p,0,"a"); npe(p,1,"o_"); npe(p,2,"t");
        row(p,"cleared","NPE");
        lr nullOptions=new lr(null);
        q.a=nullOptions;
        row(q,"null-settings-fresh","OK");
        try { q.a(17,true); throw new AssertionError("missing settings NPE"); }
        catch(NullPointerException expected) {
            if(!expected.getStackTrace()[0].getClassName().equals("lr"))
                throw new AssertionError("wrong nested settings receiver",expected);
        }
        row(q,"null-settings-key","NPE");
        q.o_(); row(q,"null-settings-reset","OK");
        nullOptions.a((gs)q); row(q,"null-settings-update","OK");
        lr later=new lr(settings);
        q.a=later;
        qb forward=settings.m;
        settings.m=null;
        try { q.a(17,true); throw new AssertionError("missing binding NPE"); }
        catch(NullPointerException expected) {
            if(!expected.getStackTrace()[0].getClassName().equals("lr"))
                throw new AssertionError("wrong nested binding receiver",expected);
        }
        row(q,"null-binding","NPE");
        settings.m=forward;
        q.a(17,true); later.a((gs)q); row(q,"restored","OK");
        forward.b=Integer.MIN_VALUE;
        q.a(17,false); row(q,"rebind-stale","OK");
        q.a(Integer.MIN_VALUE,false); row(q,"rebind-event","OK");
        later.a((gs)q); row(q,"rebind-updated","OK");
        if(p.aM!=x || p.aN!=y || p.aO!=z)
            throw new AssertionError("input operations moved actual Player");
    }
}
