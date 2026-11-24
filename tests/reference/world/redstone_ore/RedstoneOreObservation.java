import java.lang.reflect.Field;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Literal inputs and passive observations around unchanged Redstone Ore calls. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class RedstoneOreObservation {
    static final String[] KEYS = {"mode", "operation", "xyz", "entry_id", "random_seed", "cell_error_at", "range_error_at", "particle_error_at", "sound_error_at", "prequeue_delay", "random_player", "player", "g", "cooldown", "null_random", "null_tick_random", "null_world", "null_provider", "null_access", "null_unlit", "null_lit", "cells", "heights"};
    static final String[] DATA = {
        "ore-left-73|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-left-74|left|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-walk-73|walk|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-walk-74|walk|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-right-73|right|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-74|right|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-opaque-face-0|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;15,65,15,1,0|",
        "ore-opaque-face-1|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;15,63,15,1,0|",
        "ore-opaque-face-2|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;15,64,16,1,0|",
        "ore-opaque-face-3|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;15,64,14,1,0|",
        "ore-opaque-face-4|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;16,64,15,1,0|",
        "ore-opaque-face-5|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;14,64,15,1,0|",
        "ore-opaque-all|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5;15,65,15,1,0;15,63,15,1,0;15,64,16,1,0;15,64,14,1,0;16,64,15,1,0;14,64,15,1,0|",
        "ore-height-0|left|15,0,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,0,15,73,5|",
        "ore-height-127|left|15,127,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,127,15,73,5|",
        "ore-receiver-lit-cell-unlit|left|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-receiver-unlit-cell-lit|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-height-prepared|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|15,15,65",
        "ore-particle-error-1|left|15,64,15|73|1|0|0|1|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-particle-error-3|left|15,64,15|73|1|0|0|3|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-particle-error-5|left|15,64,15|73|1|0|0|5|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-cell-error|left|15,64,15|73|1|1|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-range-error|left|15,64,15|73|1|0|1|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-tick-73|tick|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-tick-null-argument-73|tick|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|true|false|false|false|false|false|15,64,15,73,5|",
        "ore-display-73|display|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-tick-74|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-tick-null-argument-74|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|true|false|false|false|false|false|15,64,15,74,5|",
        "ore-display-74|display|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-tick-cell-error|tick|15,64,15|74|1|1|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-tick-range-error|tick|15,64,15|74|1|0|1|0|0|-1|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-tick-world-random-null|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|true|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-world-random-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|true|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-provider-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|true|false|false|false|15,64,15,73,5|",
        "ore-random-provider-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|true|false|false|true|false|false|false|15,64,15,73,5|",
        "ore-unlit-static-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|true|false|15,64,15,73,5|",
        "ore-lit-static-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|true|15,64,15,73,5|",
        "ore-particle-before-lit-static-null|left|15,64,15|73|1|0|0|1|0|-1|false|false|17|6001|false|false|false|false|false|false|true|15,64,15,73,5|",
        "ore-provider-before-unlit-static-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|true|false|true|false|15,64,15,73,5|",
        "ore-tick-unlit-static-null|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|true|false|15,64,15,74,5|",
        "ore-tick-lit-static-null|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|true|15,64,15,74,5|",
        "ore-tick-unlit-receiver-lit-static-null|tick|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|false|false|true|15,64,15,73,5|",
        "ore-world-scheduled-73|world-tick|15,64,15|73|1|0|0|0|0|0|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-world-scheduled-74|world-tick|15,64,15|74|1|0|0|0|0|0|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-world-scheduled-cell-error|world-tick|15,64,15|74|1|1|0|0|0|0|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-world-scheduled-range-error|world-tick|15,64,15|74|1|0|1|0|0|0|false|false|17|6001|false|false|false|false|false|false|false|15,64,15,74,5|",
        "ore-world-random-player|world-tick|-57,64,-56|74|1|0|0|0|0|-1|true|false|-332372929|6001|false|false|false|false|false|false|false|-57,64,-56,74,5|",
        "ore-mining|mining|15,64,15|73|1|0|0|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-mining-particle-error|mining|15,64,15|73|1|0|0|1|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-mining-cell-error|mining|15,64,15|73|1|1|0|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-mining-range-error|mining|15,64,15|73|1|0|1|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-use|right-use|15,64,15|73|1|0|0|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-use-particle-error|right-use|15,64,15|73|1|0|0|1|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-use-cell-error|right-use|15,64,15|73|1|1|0|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-use-range-error|right-use|15,64,15|73|1|0|1|0|0|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-right-use-sound-error|right-use|15,64,15|73|1|0|0|0|1|-1|false|true|17|6001|false|false|false|false|false|false|false|15,64,15,73,5|",
        "ore-tick-null-world-73|tick|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,73,5|",
        "ore-tick-null-world-74|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,74,5|",
        "ore-left-null-world-73|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,73,5|",
        "ore-walk-null-world-73|walk|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,73,5|",
        "ore-right-null-world-73|right|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,73,5|",
        "ore-display-null-world-73|display|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,73,5|",
        "ore-display-null-world-74|display|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|true|false|false|false|false|15,64,15,74,5|",
        "ore-access-null|left|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|true|false|false|15,64,15,73,5|",
        "ore-tick-access-null-73|tick|15,64,15|73|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|true|false|false|15,64,15,73,5|",
        "ore-tick-access-null-74|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|true|false|false|15,64,15,74,5|",
        "ore-tick-access-null-cell-74|tick|15,64,15|74|1|0|0|0|0|-1|false|false|17|6001|false|false|false|false|true|false|false|15,64,15,74,5|15,15,65"
    };
    static final class Profile {
        final Map<String,Object> values = new LinkedHashMap<>();
        Profile(String text) {
            String[] parts=text.split("\\|",-1);
            PlayerDamageWakeObservation.need(parts.length==KEYS.length,"declared ore operands");
            for(int i=0;i<KEYS.length;i++) {
                String k=KEYS[i],v=parts[i];Object value;
                if(k.equals("mode")||k.equals("operation"))value=v;
                else if(k.equals("xyz"))value=FireObservation.Profile.numbers(v);
                else if(k.equals("cells")||k.equals("heights")) {
                    List<Object> rows=new ArrayList<>();
                    if(!v.isEmpty())for(String row:v.split(";"))rows.add(FireObservation.Profile.numbers(row));value=rows;
                }else if(k.equals("player")||k.equals("random_player")||k.startsWith("null_"))value=Boolean.valueOf(v);
                else value=Integer.valueOf(v);
                values.put(k,value);
            }
        }
        String text(String k){return (String)values.get(k);}
        int n(String k){return (Integer)values.get(k);}
        boolean flag(String k){return (Boolean)values.get(k);}
        int xyz(int i){return (Integer)((List<?>)values.get("xyz")).get(i);}
        FireObservation.Profile base() {
            List<String> fields=new ArrayList<>();
            for(String k:FireObservation.KEYS) {
                Object v=values.get(k);
                if(k.equals("rain")||k.equals("null_encouragement")||k.equals("null_flammability"))v=false;
                else if(k.equals("source_id"))v=1;
                else if(k.equals("added_error_at"))v=0;
                else if(k.equals("biome"))v="i";
                fields.add(encode(v));
            }
            return new FireObservation.Profile(String.join("|",fields));
        }
    }
    static String encode(Object value) {
        if(value instanceof List) {
            List<String> parts=new ArrayList<>();for(Object v:(List<?>)value)parts.add(encode(v));
            return String.join(!((List<?>)value).isEmpty()&&((List<?>)value).get(0) instanceof List?";":",",parts);
        }
        return String.valueOf(value);
    }
    static Profile profile;
    static os controller;
    static iz held;
    static dc player;
    static Object random()throws Exception{return FireObservation.world.r==null?null:FireObservation.random(FireObservation.world.r);}
    static Object target(){return FireObservation.cell(profile.xyz(0),profile.xyz(1),profile.xyz(2));}
    static Object controller()throws Exception {
        if(controller==null)return null;
        Map<String,Object> r=new LinkedHashMap<>();
        for(String n:new String[]{"c","d","e","f","g","h","i"}) {
            Object value=FireObservation.get(controller,os.class,n);r.put(n,value instanceof Float?FireObservation.f((Float)value):value);
        }
        return r;
    }
    static Object held()throws Exception{return held==null?null:FireObservation.map("id",held.c,"count",held.a,"damage",FireObservation.get(held,iz.class,"d"));}
    static Object state()throws Exception {
        fd w=FireObservation.world;
        Map<Integer,lm> cache=(Map<Integer,lm>)FireObservation.get(FireObservation.preparedProvider,ok.class,"e");
        PlayerDamageWakeObservation.need(cache.size()==FireObservation.chunks.size(),"complete actual prepared ore cache footprint");
        List<Object> planes=new ArrayList<>(),regions=new ArrayList<>(),order=new ArrayList<>();
        for(lm c:FireObservation.chunks) {
            PlayerDamageWakeObservation.need(cache.get(yy.a(c.j,c.k))==c,"same genuine Chunk receiver");
            planes.add(FireObservation.map("xyz",List.of(c.j,c.k),"planes",FireObservation.map(
                "blocks",PlayerDamageWakeObservation.hash(c.b),"metadata",PlayerDamageWakeObservation.hash(c.e.a),
                "sky",PlayerDamageWakeObservation.hash(c.f.a),"block_light",PlayerDamageWakeObservation.hash(c.g.a),"height",PlayerDamageWakeObservation.hash(c.h))));
        }
        for(Object value:(List<?>)FireObservation.get(w,fd.class,"C")){st r=(st)value;regions.add(FireObservation.map("kind",r.a.name(),"bounds",List.of(r.b,r.c,r.d,r.e,r.f,r.g)));}
        for(Object value:(java.util.Set<?>)FireObservation.get(w,fd.class,"P")){yy r=(yy)value;order.add(List.of(r.a,r.b));}
        return FireObservation.map("target",target(),"world_random",random(),"math_random",FireObservation.random(FireObservation.math),
            "time",String.format(Locale.ROOT,"%016x",w.x.f()),"world_o",w.o,"sky_seen",lm.a,"scheduled",UprightPlantsWakeObservation.scheduled(),
            "provider_null",FireObservation.get(w,fd.class,"v")==null,"world_access_null",FireObservation.get(w,fd.class,"u")==null,
            "entity_class",FireObservation.classField(sn.class,"a",false),"lighting",FireObservation.map("depth",fd.A,"regions",regions),
            "ore_statics",FireObservation.map("unlit",uu.aO==null?null:uu.aO.bn,"lit",uu.aP==null?null:uu.aP.bn),
            "entities",FireObservation.entities(),"players",FireObservation.players(),"entity_count",w.b.size(),"player_count",w.d.size(),
            "controller",controller(),"held_stack",held(),"item_use_stat",PlayerDamageWakeObservation.app.I.a(jl.E[4]),
            "weather",FireObservation.map("rain_previous",FireObservation.f((Float)FireObservation.get(w,fd.class,"i")),"rain_current",FireObservation.f((Float)FireObservation.get(w,fd.class,"j")),
                "thunder_previous",FireObservation.f((Float)FireObservation.get(w,fd.class,"k")),"thunder_current",FireObservation.f((Float)FireObservation.get(w,fd.class,"l")),
                "rain_time",w.x.p(),"thunder_time",w.x.n(),"raining",w.x.o(),"thundering",w.x.m()),
            "random_work",FireObservation.map("g",FireObservation.get(w,fd.class,"g"),"cooldown",FireObservation.get(w,fd.class,"Q"),"candidates",order),"chunk_planes",planes);
    }
    static final class Observer implements pm {
        final List<Object> events=new ArrayList<>();int cells,ranges,particles,sounds;
        Map<String,Object> event(String kind)throws Exception {
            PlayerDamageWakeObservation.need(events.size()<128,"bounded genuine ore callbacks");
            return FireObservation.map("kind",kind,"target",target(),"world_o",FireObservation.world.o,"world_random",random(),
                "math_random",FireObservation.random(FireObservation.math),"entities",FireObservation.entities(),
                "stack",UprightPlantsWakeObservation.stack(Thread.currentThread().getStackTrace()));
        }
        void interrupt(String kind,int count){if(count==profile.n(kind+"_error_at"))throw new IllegalStateException("Redstone Ore "+kind+" observer interruption");}
        public void a(int x,int y,int z){try{Map<String,Object> e=event("cell");e.put("xyz",List.of(x,y,z));e.put("cell",FireObservation.cell(x,y,z));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("cell",++cells);}
        public void b(int x,int y,int z,int a,int b,int c){try{Map<String,Object> e=event("range");e.put("xyz",List.of(x,y,z,a,b,c));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("range",++ranges);}
        public void a(String name,double x,double y,double z,double a,double b,double c){try{Map<String,Object> e=event("particle");e.put("name",name);e.put("position",List.of(FireObservation.d(x),FireObservation.d(y),FireObservation.d(z)));e.put("motion",List.of(FireObservation.d(a),FireObservation.d(b),FireObservation.d(c)));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("particle",++particles);}
        public void a(String name,double x,double y,double z,float v,float p){try{Map<String,Object> e=event("sound");e.put("name",name);e.put("position",List.of(FireObservation.d(x),FireObservation.d(y),FireObservation.d(z)));e.put("volume",FireObservation.f(v));e.put("pitch",FireObservation.f(p));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("sound",++sounds);}
        public void e(){try{events.add(event("sky"));}catch(Exception e){throw new AssertionError(e);}}
        public void a(sn value){throw new AssertionError("undeclared Entity publication");}
        public void b(sn value){throw new AssertionError("undeclared Entity removal");}
        public void a(String n,int x,int y,int z){throw new AssertionError("undeclared record");}
        public void a(int x,int y,int z,ow value){throw new AssertionError("undeclared TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("undeclared level event");}
    }
    static void staticField(String name,Object value)throws Exception {
        Class<?> type=Class.forName("sun.misc.Unsafe");Object unsafe=FireObservation.get(null,type,"theUnsafe");Field field=uu.class.getDeclaredField(name);
        Object base=type.getMethod("staticFieldBase",Field.class).invoke(unsafe,field);
        long offset=(Long)type.getMethod("staticFieldOffset",Field.class).invoke(unsafe,field);
        type.getMethod("putObject",Object.class,long.class,Object.class).invoke(unsafe,base,offset,value);
    }
    static void run(Profile p,int index)throws Exception {
        profile=p;controller=null;held=null;player=null;
        Map<String,Object> classes=(Map<String,Object>)FireObservation.setup(p.base(),index);
        classes.put("item_use_stat",PlayerDamageWakeObservation.app.I.a(jl.E[4]));
        fd w=FireObservation.world;kq app=PlayerDamageWakeObservation.app;
        if(p.flag("player")) {
            player=new dc(app,w,app.k,0);app.f=w;app.h=player;app.i=player;player.e(8.5,64,8.5);
            PlayerDamageWakeObservation.need(w.b(player),"actual caller Player publication");controller=new os(app);
            if(p.text("operation").equals("right-use"))held=new iz(4,1,0);
            w.r.setSeed(p.n("random_seed"));FireObservation.math.setSeed(0);
        }
        uu entry=uu.m[p.n("entry_id")],unlit=uu.aO,lit=uu.aP;
        Object provider=FireObservation.get(w,fd.class,"v");Random preparedRandom=w.r;
        if(p.flag("null_random"))w.r=null;
        if(p.flag("null_provider"))FireObservation.put(w,fd.class,"v",null);
        if(p.flag("null_unlit"))staticField("aO",null);
        if(p.flag("null_lit"))staticField("aP",null);
        Observer observer=new Observer();w.a(observer);Object access=FireObservation.get(w,fd.class,"u");
        if(p.flag("null_access"))FireObservation.put(w,fd.class,"u",null);
        Object before=state(),result=null;String exception="NONE",message=null;List<Object> stack=new ArrayList<>();
        int x=p.xyz(0),y=p.xyz(1),z=p.xyz(2);fd argument=p.flag("null_world")?null:w;
        try {
            switch(p.text("operation")) {
                case "left":entry.b(argument,x,y,z,(gs)null);break;
                case "walk":entry.b(argument,x,y,z,(sn)null);break;
                case "right":result=entry.a(argument,x,y,z,(gs)null);break;
                case "tick":entry.a(argument,x,y,z,p.flag("null_tick_random")?null:w.r);break;
                case "display":entry.b(argument,x,y,z,p.flag("null_tick_random")?null:w.r);break;
                case "world-tick":w.l();break;
                case "mining":controller.a(x,y,z,1);break;
                case "right-use":result=controller.a(player,w,held,x,y,z,1);break;
                default:throw new AssertionError("declared ore entry");
            }
        }catch(RuntimeException failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();stack=UprightPlantsWakeObservation.stack(failure.getStackTrace());}
        Object after=state();
        System.out.println("REDSTONE_ORE "+ConstructorJson.json(FireObservation.map("mode",p.text("mode"),"prepared",p.values,"prepared_class",classes,
            "receiver",FireObservation.map("class",entry.getClass().getName(),"id",entry.bn,"lit_flag",FireObservation.get(entry,bs.class,"a")),
            "before",before,"after",after,"result",result,"exception",exception,"exception_message",message,"exception_stack",stack,"events",observer.events)));
        staticField("aO",unlit);staticField("aP",lit);w.r=preparedRandom;FireObservation.put(w,fd.class,"v",provider);FireObservation.put(w,fd.class,"u",access);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        PlayerDamageWakeObservation.need(args.length==3,"archive, isolated directory, class names");Locale.setDefault(Locale.ROOT);
        PlayerDamageWakeObservation.archive=Path.of(args[0]).toRealPath();PlayerDamageWakeObservation.directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new PlayerDamageWakeObservation.Guard());PlayerDamageWakeObservation.bind(Path.of(args[2]));
        kq app=new kq(null,null,null,854,480,false,null);PlayerDamageWakeObservation.app=app;
        try {
            app.k=new gr("FireComponent","-");FireObservation.put(app,net.minecraft.client.Minecraft.class,"Z",PlayerDamageWakeObservation.directory.toFile());
            PlayerDamageWakeObservation.writer=new xi(app.k,PlayerDamageWakeObservation.directory.toFile());app.I=PlayerDamageWakeObservation.writer;
            ep.f.a(new kh(app));PlayerDamageWakeObservation.workers();
            FireObservation.math=(Random)FireObservation.get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");PlayerDamageWakeObservation.shared=FireObservation.math;
            for(int i=0;i<DATA.length;i++)run(new Profile(DATA[i]),i);
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.network==0&&PlayerDamageWakeObservation.nativeLinks==0&&PlayerDamageWakeObservation.rejectedWrites==0&&PlayerDamageWakeObservation.execs==0,"effect guards");
        }finally{if(PlayerDamageWakeObservation.writer!=null)PlayerDamageWakeObservation.workers();app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);PlayerDamageWakeObservation.need(!t.isAlive(),"Timer worker retirement");}}
        System.out.println("REDSTONE_ORE_COMPLETE cases="+DATA.length+" network=0 native=0 rejected_writes=0 exec=0 GUI=0");
    }
}
