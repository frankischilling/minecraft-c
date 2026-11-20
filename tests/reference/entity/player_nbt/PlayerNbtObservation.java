import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.TreeMap;
import java.util.concurrent.atomic.AtomicLong;

/** Genuine unchanged constructors/methods; authored NBT inputs, never answer tables. */
@SuppressWarnings("unchecked")
public final class PlayerNbtObservation {
    static final String[] CASES = {
        "write_fresh", "write_alias_narrow", "roundtrip", "base_only",
        "health_wrong", "hurt_wrong", "death_wrong", "attack_wrong",
        "inventory_wrong", "entry_wrong", "entry_null", "slot_wrong",
        "id_wrong", "count_wrong", "damage_wrong", "id_negative", "id_null",
        "outside_slot", "duplicate_slot", "armor_slots", "slot_255", "slot_missing",
        "partial_list", "dimension_wrong", "sleeping_wrong", "timer_wrong",
        "spawn_partial", "spawn_x_wrong", "spawn_y_wrong", "spawn_z_wrong",
        "spawn_good", "score_wrong", "score_absent"
    };
    static StringBuilder output = new StringBuilder();
    static Path actualJar;
    static kq app;
    static gr session;
    static fd world;
    static dc player;
    static iz[] sleepMain, sleepArmor;
    static int rows, completed, interrupted;

    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object get(Class<?> owner, String name, Object receiver) throws Exception {
        return field(owner, name).get(receiver);
    }
    static String hex(long value, int width) {
        if (width != 16) value &= (1L << (width * 4)) - 1;
        String text = Long.toHexString(value);
        return "0".repeat(width - text.length()) + text;
    }
    static String units(String value) {
        if (value == null || value.isEmpty()) return "-";
        StringBuilder s = new StringBuilder();
        for (int i = 0; i < value.length(); ++i) s.append(hex(value.charAt(i), 4));
        return s.toString();
    }
    static void row(Object... values) {
        for (int i = 0; i < values.length; ++i) {
            if (i != 0) output.append(' ');
            output.append(values[i]);
        }
        output.append('\n');
        need(output.length() <= 2 * 1024 * 1024, "finite original output bound");
        ++rows;
    }
    static void origin(Class<?> type) throws Exception {
        need(type.getProtectionDomain().getCodeSource() != null, "game origin present");
        Path path = Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
        need(Files.isSameFile(path, actualJar), "actual original origin " + type.getName());
    }
    @SuppressWarnings("unchecked")
    static Map<String,ij> map(nu value) throws Exception {
        return (Map<String,ij>) get(nu.class, "a", value);
    }
    @SuppressWarnings("unchecked")
    static void tags(String name, String stage, String path, ij value) throws Exception {
        need(value != null, "observed writer has no null tag");
        origin(value.getClass());
        int kind = value.a() & 255;
        String payload;
        if (kind == 10) payload = Integer.toString(map((nu)value).size());
        else if (kind == 9)
            payload = (((Byte)get(sp.class, "b", value)) & 255) + "," + ((List<?>)get(sp.class, "a", value)).size();
        else if (kind == 8) payload = units((String)get(value.getClass(), "a", value));
        else {
            Object scalar = get(value.getClass(), "a", value);
            if (kind == 5) payload = hex(Float.floatToRawIntBits((Float)scalar), 8);
            else if (kind == 6) payload = hex(Double.doubleToRawLongBits((Double)scalar), 16);
            else if (kind == 4) payload = hex((Long)scalar, 16);
            else if (kind >= 1 && kind <= 3) payload = scalar.toString();
            else throw new AssertionError("unexpected writer tag kind " + kind);
        }
        row("TAG", name, stage, path, kind, payload);
        if (kind == 10) {
            for (Map.Entry<String,ij> entry : new TreeMap<>(map((nu)value)).entrySet()) {
                need(entry.getKey().matches("[A-Za-z][A-Za-z0-9_]*"), "authored field path");
                tags(name, stage, path + "/" + entry.getKey(), entry.getValue());
            }
        } else if (kind == 9) {
            List<ij> children = (List<ij>)get(sp.class, "a", value);
            for (int i = 0; i < children.size(); ++i) tags(name, stage, path + "/" + i, children.get(i));
        }
    }
    static String coordinate(Object value) {
        if (value == null) return "-";
        br p = (br)value;
        return p.a + "," + p.b + "," + p.c;
    }
    static String random(Random value) throws Exception {
        return hex(((AtomicLong)get(Random.class, "seed", value)).get(), 12) + "," +
            (((Boolean)get(Random.class, "haveNextNextGaussian", value)) ? "1" : "0") + "," +
            hex(Double.doubleToRawLongBits((Double)get(Random.class, "nextNextGaussian", value)), 16);
    }
    static void state(String name, String outcome, dc p, iz[] oldMain, iz[] oldArmor) throws Exception {
        row("STATE", name, outcome, p.Y, p.aa, p.ad, p.ae, p.m,
            ((Boolean)get(gs.class,"u",p)) ? 1 : 0, get(gs.class,"a",p), p.g,
            coordinate(get(gs.class,"v",p)), coordinate(get(gs.class,"b",p)),
            oldMain != p.c.a ? 1 : 0, oldArmor != p.c.b ? 1 : 0, p.c.c, p.c.e ? 1 : 0,
            p.c.a[0] != null && p.c.a[0] == p.c.b[0] ? 1 : 0,
            p.c.a[0] == null ? "-" : Integer.toString(p.c.a[0].b),
            p.c.b[0] == null ? "-" : Integer.toString(p.c.b[0].b),
            hex(Double.doubleToRawLongBits(p.aM),16), hex(Double.doubleToRawLongBits(p.aN),16),
            hex(Double.doubleToRawLongBits(p.aO),16), hex(Double.doubleToRawLongBits(p.aP),16),
            hex(Double.doubleToRawLongBits(p.aQ),16), hex(Double.doubleToRawLongBits(p.aR),16),
            hex(Float.floatToRawIntBits(p.aS),8), hex(Float.floatToRawIntBits(p.aT),8),
            hex(Float.floatToRawIntBits(p.aU),8), hex(Float.floatToRawIntBits(p.aV),8),
            hex(Float.floatToRawIntBits(p.bk),8), p.bv, p.bz, p.aX ? 1 : 0,
            hex(Float.floatToRawIntBits(p.bo),8));
    }
    static sp doubles(double... values) {
        sp list = new sp();
        for (double value : values) { nu wrapper = new nu(); wrapper.a("v", value); list.a(uncheckedTag(wrapper,"v")); }
        return list;
    }
    static sp floats(float... values) {
        sp list = new sp();
        for (float value : values) { nu wrapper = new nu(); wrapper.a("v", value); list.a(uncheckedTag(wrapper,"v")); }
        return list;
    }
    static ij uncheckedTag(nu value, String name) {
        try { return map(value).get(name); }
        catch (Exception error) { throw new AssertionError(error); }
    }
    static nu stack(Integer slot, int id, int count, int damage) {
        nu value = new nu();
        if (slot != null) value.a("Slot", (byte)(int)slot);
        value.a("id", (short)id); value.a("Count", (byte)count); value.a("Damage", (short)damage);
        return value;
    }
    static nu baseOnly() {
        nu value = new nu();
        value.a("Pos", doubles(8.5,80.0,8.5)); value.a("Motion", doubles(.1,.2,.3));
        value.a("Rotation", floats(450.0f,-450.0f));
        return value;
    }
    static nu input(String name) throws Exception {
        nu value = baseOnly();
        if (name.equals("base_only")) return value;
        value.a("FallDistance",3.5f); value.a("Fire",(short)-19); value.a("Air",(short)298);
        value.a("OnGround",true); value.a("Health",(short)17); value.a("HurtTime",(short)2);
        value.a("DeathTime",(short)3); value.a("AttackTime",(short)4);
        sp list = new sp(); list.a(stack(0,1,3,4));
        value.a("Inventory",list); value.a("Dimension",-1); value.a("Sleeping",false);
        value.a("SleepTimer",(short)23); value.a("Score",123);
        value.a("SpawnX",5); value.a("SpawnY",66); value.a("SpawnZ",-7);
        if (name.equals("health_wrong")) value.a("Health",99);
        if (name.equals("hurt_wrong")) value.a("HurtTime",99);
        if (name.equals("death_wrong")) value.a("DeathTime",99);
        if (name.equals("attack_wrong")) value.a("AttackTime",99);
        if (name.equals("inventory_wrong")) value.a("Inventory",99);
        if (name.equals("entry_wrong")) { list=new sp();nu n=new nu();n.a("v",7);list.a(uncheckedTag(n,"v"));value.a("Inventory",list); }
        if (name.equals("entry_null")) { list=new sp();((List<?>)get(sp.class,"a",list)).clear();
            @SuppressWarnings("unchecked") List<ij> entries=(List<ij>)get(sp.class,"a",list);entries.add(null);value.a("Inventory",list); }
        nu entry = null;
        if (name.equals("slot_wrong")) {entry=stack(0,1,3,4);entry.a("Slot",(short)7);}
        if (name.equals("id_wrong")) {entry=stack(0,1,3,4);entry.a("id",7);}
        if (name.equals("count_wrong")) {entry=stack(0,1,3,4);entry.a("Count",7);}
        if (name.equals("damage_wrong")) {entry=stack(0,1,3,4);entry.a("Damage",7);}
        if (name.equals("id_negative")) entry=stack(0,-1,3,4);
        if (name.equals("id_null")) entry=stack(0,3200,3,4);
        if (name.equals("outside_slot")) entry=stack(50,1,3,4);
        if (name.equals("slot_255")) entry=stack(255,1,3,4);
        if (name.equals("slot_missing")) entry=stack(null,1,3,4);
        if (entry!=null) {list=new sp();list.a(entry);value.a("Inventory",list);}
        if (name.equals("duplicate_slot")) {list=new sp();list.a(stack(0,1,3,4));list.a(stack(0,3,-2,-5));value.a("Inventory",list);}
        if (name.equals("armor_slots")) {list=new sp();list.a(stack(100,1,3,4));list.a(stack(103,3,-2,-5));value.a("Inventory",list);}
        if (name.equals("partial_list")) {nu n=new nu();n.a("v",7);list.a(uncheckedTag(n,"v"));}
        if (name.equals("dimension_wrong")) value.a("Dimension",(short)3);
        if (name.equals("sleeping_wrong")) value.a("Sleeping",(short)1);
        if (name.equals("timer_wrong")) value.a("SleepTimer",1);
        if (name.equals("spawn_partial")) map(value).remove("SpawnZ");
        if (name.equals("spawn_x_wrong")) value.a("SpawnX",(short)1);
        if (name.equals("spawn_y_wrong")) value.a("SpawnY",(short)1);
        if (name.equals("spawn_z_wrong")) value.a("SpawnZ",(short)1);
        if (name.equals("spawn_good")) {value.a("SpawnX",Integer.MIN_VALUE);value.a("SpawnY",Integer.MAX_VALUE);value.a("SpawnZ",0);}
        if (name.equals("score_wrong")) value.a("Score",(short)3);
        if (name.equals("score_absent")) map(value).remove("Score");
        if (name.equals("sleep")) value.a("Sleeping",true);
        return value;
    }
    static fd create(Path root, String name, boolean prepared, nu savedPlayer) throws Exception {
        mx handler = new mx(root.toFile(),name,false);
        if (prepared) {
            ei info = new ei(23L,name);info.a(2,64,2);info.d(19132);info.a(savedPlayer);handler.a(info);
        }
        fd value = new fd(handler,name,23L,xa.a(0));
        need(value.s != prepared,"actual constructor new-world flag");
        return value;
    }
    static dc construct(fd value) {
        app.f=value;dc p=new dc(app,value,session,0);app.h=p;return p;
    }
    static void prepare(dc p, boolean spawn) throws Exception {
        nu own = new nu();
        if (spawn) {own.a("SpawnX",-7);own.a("SpawnY",65);own.a("SpawnZ",9);}
        p.a(own); /* Genuine dc -> gs -> ls own-data read and Inventory reset. */
        p.be=false;p.bo=.25f;p.e(8.5,80.25,8.5);p.aP=.25;p.aQ=-.5;p.aR=.75;
        p.aS=450;p.aT=-450;p.aU=12.5f;p.aV=-3.0f;p.bk=2.5f;p.bv=65535;p.bz=65537;p.aX=true;
        p.Y=20;p.aa=1;p.ad=2;p.ae=3;p.m=0;field(gs.class,"u").setBoolean(p,false);
        field(gs.class,"a").setInt(p,4);p.g=900;
        iz common=new iz(2,5,9);common.b=4;p.c.a(0,common);p.c.a(36,common);p.c.c=7;p.c.e=true;
        p.bs.setSeed(38);p.aI.r.setSeed(38);
    }
    static String outcome(Throwable error) {
        need(error instanceof ClassCastException || error instanceof NullPointerException ||
             error instanceof IndexOutOfBoundsException,"actual expected exception family: "+error);
        if (error instanceof ClassCastException) return "CAST";
        return error instanceof NullPointerException ? "NULL" : "INDEX";
    }
    static void cases() throws Exception {
        for (String name : CASES) {
            prepare(player,!name.equals("write_fresh"));
            iz[] beforeMain=player.c.a,beforeArmor=player.c.b;
            String entityRandom=random(player.bs),worldRandom=random(world.r),status="OK";
            if (name.startsWith("write_")) {
                if (name.equals("write_alias_narrow")) {
                    player.Y=40000;player.aa=32768;player.ad=-40000;player.ae=65535;
                    player.c.a[0].c=65537;player.c.a[0].a=511;
                    field(iz.class,"d").setInt(player.c.a[0],65538);player.g=Integer.MIN_VALUE;
                    player.be=true;
                }
                nu direct=new nu();player.d(direct);tags(name,"direct","$",direct);
                nu named=new nu();named.a("Marker",7);
                boolean emitted=player.c(named);
                need(!emitted&&map(named).size()==1&&named.e("Marker")==7,"normal unnamed Player Boolean save");
                row("NAMED",name,0,1);
            } else {
                nu tag;
                if (name.equals("roundtrip")) {tag=new nu();player.d(tag);tags(name,"input","$",tag);}
                else tag=input(name);
                try {player.e(tag);++completed;}
                catch (Throwable error) {status=outcome(error);++interrupted;
                    java.io.StringWriter trace=new java.io.StringWriter();error.printStackTrace(new java.io.PrintWriter(trace));
                    need(trace.getBuffer().length()<32768,"bounded original Throwable");
                    row("THROW",name,error.getClass().getName(),java.util.Base64.getEncoder().encodeToString(trace.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)));
                }
            }
            state(name,status,player,beforeMain,beforeArmor);
            nu after=new nu();player.d(after);tags(name,"after","$",after);
            need(entityRandom.equals(random(player.bs))&&worldRandom.equals(random(world.r)),"NBT preserves actual owner Randoms");
            row("RANDOM",name,random(player.bs),random(world.r));
            row("END",name,status);
        }
    }
    static void composition(Path root) throws Exception {
        fd loaded=create(root,"loaded",true,baseOnly());dc p=construct(loaded);
        prepare(p,true);iz[] oldMain=p.c.a,oldArmor=p.c.b;
        nu retained=loaded.x.h();need(retained!=null,"loaded actual Info.Player");
        loaded.a(p);
        need(loaded.x.h()==null&&loaded.d.size()==1&&loaded.d.get(0)==p&&p.Y==10,"base-only Info.Player actual success/publication");
        state("attach_base_only","OK",p,oldMain,oldArmor);
        row("ATTACH","base_only",0,loaded.d.size(),loaded.b.contains(p)?1:0);
        nu record=new nu();p.d(record);tags("attach_base_only","after","$",record);

        fd fresh=create(root,"fresh",false,null);p=construct(fresh);prepare(p,true);
        fresh.a(p);need(fresh.d.size()==1&&fresh.d.get(0)==p,"real fresh Player attachment");
        p.bs.setSeed(38);fresh.r.setSeed(38);
        String er=random(p.bs),wr=random(fresh.r);
        long before=System.currentTimeMillis();fresh.a(true,(yb)null);long after=System.currentTimeMillis();
        ei saved=((mx)fresh.w).c();need(saved!=null&&saved.h()!=null,"actual full-save persisted Player");
        long last=(Long)get(ei.class,"f",saved);need(last>=before&&last<=after,"actual LastPlayed clock bracket");
        need(er.equals(random(p.bs))&&wr.equals(random(fresh.r)),"full-save owner RNG continuation");
        tags("full_save","player","$",saved.h());
        row("SAVE","fresh",fresh.s?1:0,fresh.d.size(),saved.h().e("Dimension"),
            saved.h().e("Score"),get(ei.class,"k",saved),1);

        nu opaque=new nu();opaque.a("Dimension",99);opaque.a("Marker",7);fresh.x.a(opaque);
        List<gs> actual=fresh.d;
        List<gs> captured=new ArrayList<>(actual);
        actual.clear();long c0=System.currentTimeMillis();nu empty=fresh.x.a(actual);long c1=System.currentTimeMillis();
        need(empty.f("LastPlayed")>=c0&&empty.f("LastPlayed")<=c1,"empty List clock bracket");row("CLOCK","select_empty",1);
        need(!map(empty).containsKey("Player")&&fresh.x.h()==opaque,"empty live List omits retained Player");
        tags("select_empty","data","$",empty);
        actual.add(null);c0=System.currentTimeMillis();nu firstNull=fresh.x.a(actual);c1=System.currentTimeMillis();
        need(firstNull.f("LastPlayed")>=c0&&firstNull.f("LastPlayed")<=c1,"null Player clock bracket");row("CLOCK","select_null",1);
        need(!map(firstNull).containsKey("Player")&&fresh.x.h()==opaque,"selected null omits retained Player");
        tags("select_null","data","$",firstNull);
        actual.clear();actual.add(p);dc second=construct(fresh);ix secondInventory=second.c;second.c=null;
        actual.add(second);nu selected;
        try {c0=System.currentTimeMillis();selected=fresh.x.a(actual);c1=System.currentTimeMillis();} finally {second.c=secondInventory;}
        need(selected.f("LastPlayed")>=c0&&selected.f("LastPlayed")<=c1,"selected Player clock bracket");row("CLOCK","select_first_only",1);
        need(map(selected).containsKey("Player")&&fresh.x.h()==opaque,"only actual first Player selected");
        tags("select_first_only","data","$",selected);
        actual.clear();actual.addAll(captured);
        row("SELECTION",3,1);
    }
    public static String sleepingPrefix(dc p) throws Exception {
        StringBuilder prior=output;int previousRows=rows;output=new StringBuilder();
        try {
            state("sleep","PENDING_WAKE",p,sleepMain,sleepArmor);
            nu record=new nu();p.d(record);tags("sleep","prefix","$",record);
            row("RANDOM","sleep",random(p.bs),random(p.aI.r));
            row("ATTACH_PREFIX","sleep",p.aI.x.h()!=null?1:0,p.aI.d.size(),p.aI.b.size());
            return output.toString();
        } finally {output=prior;rows=previousRows;}
    }
    static void sleep() throws Exception {
        prepare(player,true);sleepMain=player.c.a;sleepArmor=player.c.b;
        world.x.a(input("sleep"));world.a(player);
        need(!((Boolean)get(gs.class,"u",player))&&((Integer)get(gs.class,"a",player))==0&&
             world.x.h()==null&&world.d.size()==1,
             "actual original wake/read/clear/publication completes after checkpoint");
        row("SLEEP_AFTER","OK",coordinate(get(gs.class,"v",player)),player.g,1,world.d.size());
    }
    public static void main(String[] args) throws Exception {
        need(args.length==3,"root jar mode");actualJar=Path.of(args[1]).toRealPath();
        Path root=Path.of(args[0]);Files.createDirectories(root);
        for(String name:new String[]{"br","dc","eq","fd","fm","gm","gr","gs","hn","ij","ix","iz","kq","lh","ls","mi","mx","net.minecraft.client.Minecraft","nu","ok","p","pp","qp","rh","sn","sp","sz","ul","wt","xa","xb"}) {
            Class<?> type=Class.forName(name,false,PlayerNbtObservation.class.getClassLoader());origin(type);
            row("ORIGIN",name,actualJar);
        }
        try {
            app=new kq(null,null,null,854,480,false,null);session=new gr("NBTPlayer","-");app.k=session;
            world=create(root,"method",true,null);player=construct(world);
            if(args[2].equals("sleep")) sleep();
            else {need(args[2].equals("normal"),"known mode");cases();composition(root);}
            System.out.print(output);
            System.out.println(args[2].equals("sleep")?"Original Player NBT sleep: 1 completed call":
                "Original Player NBT: "+CASES.length+" cases, "+completed+" complete reads, "+interrupted+" interrupted reads");
            need(!System.out.checkError(),"original stdout");
        } finally {
            if(app!=null)app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")) {
                thread.interrupt();thread.join(2000);need(!thread.isAlive(),"real timer thread retirement");
            }
        }
    }
}
