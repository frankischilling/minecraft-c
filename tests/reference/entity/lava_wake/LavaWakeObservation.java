import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Predeclared operands around unchanged Player wake, Lava mixing and fizz. */
public final class LavaWakeObservation {
    static final class Profile {
        final String mode;
        final int id, metadata, x, y, z, waterDirection, waterId, worldSeed;
        final int cellErrorAt, soundErrorAt, particleErrorAt, rangeErrorAt;
        Profile(String mode,int id,int metadata,int x,int y,int z,int waterDirection,int waterId,int worldSeed,
                int cellErrorAt,int soundErrorAt,int particleErrorAt,int rangeErrorAt) {
            this.mode=mode;this.id=id;this.metadata=metadata;this.x=x;this.y=y;this.z=z;
            this.waterDirection=waterDirection;this.waterId=waterId;this.worldSeed=worldSeed;
            this.cellErrorAt=cellErrorAt;this.soundErrorAt=soundErrorAt;
            this.particleErrorAt=particleErrorAt;this.rangeErrorAt=rangeErrorAt;
        }
        Object input() {
            return ConstructorJson.map("mode",mode,"id",id,"metadata",metadata,"x",x,"y",y,"z",z,
                "water_direction",waterDirection,"water_id",waterId,"world_seed",worldSeed,
                "cell_error_at",cellErrorAt,"sound_error_at",soundErrorAt,
                "particle_error_at",particleErrorAt,"range_error_at",rangeErrorAt);
        }
    }
    static final Profile[] PROFILES = {
        new Profile("lava-obsidian",11,0,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-cobblestone-meta1",11,1,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-cobblestone-meta2",11,2,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-cobblestone-meta3",11,3,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-cobblestone-meta4",11,4,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-hot-meta5",11,5,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-hot-meta15",11,15,7,64,8,0,8,1,0,0,0,0),
        new Profile("lava-no-contact-meta0",11,0,7,64,8,-1,0,1,0,0,0,0),
        new Profile("lava-no-contact-meta5",11,5,7,64,8,-1,0,1,0,0,0,0),
        new Profile("lava-obsidian-east-water",11,0,8,64,9,1,8,1,0,0,0,0),
        new Profile("lava-obsidian-north-water",11,0,7,64,8,2,8,1,0,0,0,0),
        new Profile("lava-obsidian-south-water",11,0,7,64,8,3,8,1,0,0,0,0),
        new Profile("lava-obsidian-above-water",11,0,7,64,8,4,8,1,0,0,0,0),
        new Profile("lava-obsidian-still-water",11,0,7,64,8,0,9,1,0,0,0,0),
        new Profile("lava-hot-still-water",11,5,7,64,8,0,9,1,0,0,0,0),
        new Profile("lava-obsidian-cell-error",11,0,7,64,8,0,8,1,2,0,0,0),
        new Profile("lava-obsidian-sound-error",11,0,7,64,8,0,8,1,0,1,0,0),
        new Profile("lava-obsidian-particle-error-first",11,0,7,64,8,0,8,1,0,0,1,0),
        new Profile("lava-obsidian-particle-error-fourth",11,0,7,64,8,0,8,1,0,0,4,0),
        new Profile("lava-hot-sound-error-first",11,5,7,64,8,0,8,1,0,1,0,0),
        new Profile("lava-hot-sound-error-second",11,5,7,64,8,0,8,1,0,2,0,0),
        new Profile("lava-hot-particle-error-third",11,5,7,64,8,0,8,1,0,0,3,0),
        new Profile("lava-hot-particle-error-ninth",11,5,7,64,8,0,8,1,0,0,9,0),
        new Profile("lava-hot-range-error",11,5,7,64,8,0,8,1,0,0,0,1),
        new Profile("lava-no-contact-range-error",11,5,7,64,8,-1,0,1,0,0,0,1),
        new Profile("lava-obsidian-height-range-error",11,0,7,64,8,0,8,1,0,0,0,1),
        new Profile("lava-hot-late-range-error",11,5,7,64,8,0,8,1,0,0,0,2),
        new Profile("lava-no-contact-late-range-error",11,5,7,64,8,-1,0,1,0,0,0,2),
        new Profile("lava-obsidian-still-water-late-range-error",11,0,7,64,8,0,9,1,0,0,0,3),
    };
    static Object cell(int x,int y,int z) {
        return UprightPlantsWakeObservation.cellAt(x,y,z);
    }
    static Object lava(Profile p) {return cell(p.x,p.y,p.z);}
    static int[] waterPosition(Profile p) {
        int[][] offsets={{-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,1,0}};
        if(p.waterDirection==-1)return null;
        int[] delta=offsets[p.waterDirection];
        return new int[]{p.x+delta[0],p.y+delta[1],p.z+delta[2]};
    }
    static Object water(Profile p) {
        int[] xyz=waterPosition(p);if(xyz==null)return null;
        List<?> value=(List<?>)cell(xyz[0],xyz[1],xyz[2]);
        return List.of(xyz[0],xyz[1],xyz[2],value.get(0),value.get(1));
    }
    static Object receiver(Profile p)throws Exception {
        uu block=uu.m[p.id];
        return ConstructorJson.map("class",block.getClass().getName(),
            "neighbor",UprightPlantsWakeObservation.method(block,"b",fd.class,int.class,int.class,int.class,int.class),
            "addition",UprightPlantsWakeObservation.method(block,"c",fd.class,int.class,int.class,int.class),
            "tick_rate",UprightPlantsWakeObservation.method(block,"e"),
            "material_class",block.bA.getClass().getName(),"actual_lava_material",block.bA==ln.h);
    }
    static Object state(Observer o)throws Exception {
        Map<String,Object> result=new LinkedHashMap<>();
        for(Map.Entry<?,?> entry:((Map<?,?>)PlayerDamageWakeObservation.state()).entrySet())
            result.put((String)entry.getKey(),entry.getValue());
        result.put("lava",lava(o.p));result.put("water_cell",water(o.p));
        result.put("world_o",PlayerDamageWakeObservation.world.o);
        result.put("scheduled",UprightPlantsWakeObservation.scheduled());
        List<Object> lights=new ArrayList<>();
        for(Object value:(List<?>)PlayerDamageWakeObservation.get(PlayerDamageWakeObservation.world,fd.class,"C")) {
            st request=(st)value;
            lights.add(ConstructorJson.map("kind",request.a.name(),"bounds",List.of(request.b,request.c,request.d,request.e,request.f,request.g)));
        }
        result.put("lighting",ConstructorJson.map("depth",fd.A,"regions",lights));
        result.put("sky_seen",lm.a);
        result.put("world_random",PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.world.r));
        result.put("player_random",PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.actor.bs));
        result.put("foot",cell(8,64,7));result.put("items",List.of());
        return result;
    }
    static final class Observer implements pm {
        final Profile p;
        final List<Object> events=new ArrayList<>();
        int cells,sounds,particles,ranges;
        Observer(Profile p){this.p=p;}
        Map<String,Object> event(String kind)throws Exception {
            PlayerDamageWakeObservation.need(events.size()<32,"bounded actual observer events");
            return ConstructorJson.map("kind",kind,"head",cell(8,64,8),"lava",lava(p),"water_cell",water(p),
                "sleeping",PlayerDamageWakeObservation.actor.u,"world_o",PlayerDamageWakeObservation.world.o,
                "world_random",PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.world.r),
                "math_random",PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.shared),
                "stack",UprightPlantsWakeObservation.stack(Thread.currentThread().getStackTrace()));
        }
        public void a(int x,int y,int z) {
            try{Map<String,Object> row=event("cell");row.put("xyz",List.of(x,y,z));events.add(row);}
            catch(Exception failure){throw new AssertionError(failure);}
            if(++cells==p.cellErrorAt)throw new IllegalStateException("Lava cell observer interruption");
        }
        public void b(int x,int y,int z,int a,int b,int c) {
            try{Map<String,Object> row=event("range");row.put("xyz",List.of(x,y,z,a,b,c));events.add(row);}
            catch(Exception failure){throw new AssertionError(failure);}
            if(++ranges==p.rangeErrorAt)throw new IllegalStateException("Lava range observer interruption");
        }
        public void a(String name,double x,double y,double z,float volume,float pitch) {
            try{Map<String,Object> row=event("sound");row.put("name",name);
                row.put("position",List.of(PlayerDamageWakeObservation.d(x),PlayerDamageWakeObservation.d(y),PlayerDamageWakeObservation.d(z)));
                row.put("volume",PlayerDamageWakeObservation.f(volume));row.put("pitch",PlayerDamageWakeObservation.f(pitch));events.add(row);}
            catch(Exception failure){throw new AssertionError(failure);}
            if(++sounds==p.soundErrorAt)throw new IllegalStateException("Lava sound observer interruption");
        }
        public void a(String name,double x,double y,double z,double vx,double vy,double vz) {
            try{Map<String,Object> row=event("particle");row.put("name",name);
                row.put("position",List.of(PlayerDamageWakeObservation.d(x),PlayerDamageWakeObservation.d(y),PlayerDamageWakeObservation.d(z)));
                row.put("motion",List.of(PlayerDamageWakeObservation.d(vx),PlayerDamageWakeObservation.d(vy),PlayerDamageWakeObservation.d(vz)));events.add(row);}
            catch(Exception failure){throw new AssertionError(failure);}
            if(++particles==p.particleErrorAt)throw new IllegalStateException("Lava particle observer interruption");
        }
        public void a(sn e){throw new AssertionError("unexpected Entity publication");}
        public void b(sn e){throw new AssertionError("unexpected Entity removal");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected event");}
    }
    static void run(Profile p)throws Exception {
        PlayerDamageWakeObservation.setup(new int[]{0,0,20,0,0});
        fd world=PlayerDamageWakeObservation.world;
        List<?> listeners=(List<?>)PlayerDamageWakeObservation.get(world,fd.class,"u");
        PlayerDamageWakeObservation.need(listeners.size()==1,"one setup observer");
        world.b((pm)listeners.get(0));Observer observer=new Observer(p);world.a(observer);
        PlayerDamageWakeObservation.need(listeners.size()==1&&listeners.get(0)==observer,"one actual observer");
        List<Object> assignments=new ArrayList<>();
        PlayerDamageWakeObservation.cell(p.x,p.y,p.z,p.id,p.metadata);
        assignments.add(List.of(p.x,p.y,p.z,p.id,p.metadata));
        int[] water=waterPosition(p);
        if(water!=null){PlayerDamageWakeObservation.cell(water[0],water[1],water[2],p.waterId,0);
            assignments.add(List.of(water[0],water[1],water[2],p.waterId,0));}
        world.r.setSeed(p.worldSeed);
        Object prepared=ConstructorJson.map("profile",p.input(),"shared_setup",List.of(0,0,20,0,0),
            "post_setup_cell_assignments",assignments,"world_random_reseed",p.worldSeed,
            "light_or_sky_getter_overrides",List.of());
        Object before=state(observer),result=null;String exception="NONE",message=null;
        List<Object> exceptionStack=new ArrayList<>();
        try{result=PlayerDamageWakeObservation.actor.a((sn)null,0);}
        catch(RuntimeException failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();
            exceptionStack=UprightPlantsWakeObservation.stack(failure.getStackTrace());}
        System.out.println("LAVA_WAKE "+ConstructorJson.json(ConstructorJson.map("mode",p.mode,
            "receiver",receiver(p),"prepared",prepared,"before",before,"after",state(observer),
            "result",result,"exception",exception,"exception_message",message,"exception_stack",exceptionStack,
            "events",observer.events)));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        PlayerDamageWakeObservation.need(args.length == 3, "archive, private directory, class names");
        Locale.setDefault(Locale.ROOT);
        PlayerDamageWakeObservation.archive = Path.of(args[0]).toRealPath();
        PlayerDamageWakeObservation.directory = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new PlayerDamageWakeObservation.Guard());
        PlayerDamageWakeObservation.bind(Path.of(args[2]));
        kq app = new kq(null, null, null, 854, 480, false, null);
        PlayerDamageWakeObservation.app = app;
        try {
            app.k = new gr("BedNightComponent", "-");
            PlayerDamageWakeObservation.put(app, net.minecraft.client.Minecraft.class, "Z",
                PlayerDamageWakeObservation.directory.toFile());
            PlayerDamageWakeObservation.writer = new xi(app.k, PlayerDamageWakeObservation.directory.toFile());
            app.I = PlayerDamageWakeObservation.writer;
            ep.f.a(new kh(app));
            PlayerDamageWakeObservation.workers();
            PlayerDamageWakeObservation.shared = (Random) PlayerDamageWakeObservation.get(null,
                Class.forName("java.lang.Math$RandomNumberGeneratorHolder"), "randomNumberGenerator");
            for (Profile profile : PROFILES) run(profile);
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.network == 0 &&
                PlayerDamageWakeObservation.nativeLinks == 0 && PlayerDamageWakeObservation.rejectedWrites == 0 &&
                PlayerDamageWakeObservation.execs == 0, "external effect guards");
        } finally {
            if (PlayerDamageWakeObservation.writer != null) PlayerDamageWakeObservation.workers();
            app.J = false;
            for (Thread thread : Thread.getAllStackTraces().keySet()) {
                if (thread.getName().equals("Timer hack thread")) {
                    thread.interrupt();
                    thread.join(2000);
                    PlayerDamageWakeObservation.need(!thread.isAlive(), "Timer worker retirement");
                }
            }
        }
        System.out.println("LAVA_WAKE_COMPLETE cases=29 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
    }
}
