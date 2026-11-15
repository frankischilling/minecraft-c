import java.lang.reflect.Field;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Declared operands and passive observations around unchanged Fire calls. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class FireObservation {
    static final String[] KEYS = {"mode","operation","xyz","entry_id","source_id","random_seed","rain","biome","cell_error_at","range_error_at","added_error_at","sound_error_at","prequeue_delay","random_player","g","cooldown","null_encouragement","null_flammability","null_provider","cells","heights"};
    static final String[] DATA = {
        "fire-added-solid|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0|",
        "fire-added-flammable-0|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|16,64,15,5,0|",
        "fire-added-flammable-1|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,64,15,5,0|",
        "fire-added-flammable-2|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,5,0|",
        "fire-added-flammable-3|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,65,15,5,0|",
        "fire-added-flammable-4|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,64,14,5,0|",
        "fire-added-flammable-5|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,64,16,5,0|",
        "fire-added-unsupported|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false||",
        "fire-added-obsidian-no-frame|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,49,0|",
        "fire-added-portal-x|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "fire-added-portal-z|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,49,0;15,63,16,49,0;15,64,14,49,0;15,64,17,49,0;15,65,14,49,0;15,65,17,49,0;15,66,14,49,0;15,66,17,49,0;15,67,15,49,0;15,67,16,49,0|",
        "fire-added-portal-broken|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,64,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "fire-added-portal-ambiguous|added|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,64,15,49,0;15,63,15,49,0;15,64,14,49,0|",
        "fire-added-portal-cell-error-third|added|15,64,15|51|1|1|false|i|3|0|0|0|-1|false|17|6001|false|false|false|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "fire-neighbor-solid|neighbor|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0|",
        "fire-neighbor-flammable|neighbor|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,64,15,51,0;16,64,15,5,0|",
        "fire-neighbor-unsupported|neighbor|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,64,15,51,0|",
        "fire-neighbor-cell-error|neighbor|15,64,15|51|1|1|false|i|1|0|0|0|-1|false|17|6001|false|false|false|15,64,15,51,0|",
        "fire-tick-isolated-age0|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0|",
        "fire-tick-isolated-age3|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,3|",
        "fire-tick-isolated-age4|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,4|",
        "fire-tick-isolated-age15|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,15|",
        "fire-tick-unsupported|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,64,15,51,0|",
        "fire-tick-age15-extinguish|tick|15,64,15|51|1|4096|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,15;16,64,15,5,0|",
        "fire-tick-age15-continue|tick|15,64,15|51|1|0|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,15;16,64,15,5,0|",
        "fire-tick-netherrack-rain|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,87,0;15,64,15,51,15|",
        "fire-tick-rain-self|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0|",
        "fire-tick-rain-west|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;15,65,15,1,0|",
        "fire-tick-rain-east|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|14,65,15,1,0;15,63,15,1,0;15,64,15,51,0;15,65,15,1,0|",
        "fire-tick-rain-north|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|14,65,15,1,0;15,63,15,1,0;15,64,15,51,0;15,65,15,1,0;16,65,15,1,0|",
        "fire-tick-rain-south|tick|15,64,15|51|1|1|true|i|0|0|0|0|-1|false|17|6001|false|false|false|14,65,15,1,0;15,63,15,1,0;15,64,15,51,0;15,65,14,1,0;15,65,15,1,0;16,65,15,1,0|",
        "fire-tick-rain-desert|tick|15,64,15|51|1|1|true|h|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;16,64,15,5,0|",
        "fire-tick-rain-tundra|tick|15,64,15|51|1|1|true|k|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;16,64,15,5,0|",
        "fire-tick-burn-5-remove|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|",
        "fire-tick-burn-5-replace|tick|15,64,15|51|1|69|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|",
        "fire-tick-burn-85-remove|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,85,0|",
        "fire-tick-burn-85-replace|tick|15,64,15|51|1|69|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,85,0|",
        "fire-tick-burn-53-remove|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,53,0|",
        "fire-tick-burn-53-replace|tick|15,64,15|51|1|69|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,53,0|",
        "fire-tick-burn-17-remove|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,17,0|",
        "fire-tick-burn-17-replace|tick|15,64,15|51|1|114|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,17,0|",
        "fire-tick-burn-18-remove|tick|15,64,15|51|1|19|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,18,0|",
        "fire-tick-burn-18-replace|tick|15,64,15|51|1|4|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,18,0|",
        "fire-tick-burn-47-remove|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,47,0|",
        "fire-tick-burn-47-replace|tick|15,64,15|51|1|69|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,47,0|",
        "fire-tick-burn-46-remove|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-tick-burn-46-replace|tick|15,64,15|51|1|4|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-tick-burn-31-remove|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,2,0;16,64,15,31,0|",
        "fire-tick-burn-31-replace|tick|15,64,15|51|1|4|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,2,0;16,64,15,31,0|",
        "fire-tick-burn-35-remove|tick|15,64,15|51|1|19|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,35,0|",
        "fire-tick-burn-35-replace|tick|15,64,15|51|1|4|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,35,0|",
        "fire-tick-spread-seed1|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;16,63,15,2,0;16,64,15,31,0|",
        "fire-tick-spread-seed8|tick|15,64,15|51|1|8|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;16,63,15,2,0;16,64,15,31,0|",
        "fire-tick-tnt-added-error|tick|15,64,15|51|1|4|false|i|0|0|1|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-tick-tnt-sound-error|tick|15,64,15|51|1|4|false|i|0|0|0|1|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-tick-tnt-cell-error-before-prime|tick|15,64,15|51|1|4|false|i|1|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-tick-height-normal|tick|15,64,15|51|1|49|false|i|0|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|16,15,65",
        "fire-tick-height-range-error|tick|15,64,15|51|1|49|false|i|0|1|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|16,15,65",
        "fire-tick-height-cell-error|tick|15,64,15|51|1|49|false|i|1|0|0|0|-1|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|16,15,65",
        "fire-world-tick|world-tick|15,64,15|51|1|1|false|i|0|0|0|0|0|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,0;16,64,15,5,0|",
        "fire-world-tick-cell-error|world-tick|15,64,15|51|1|49|false|i|1|0|0|0|0|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|",
        "fire-lava-ignition-5|lava|15,64,15|11|1|2|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,66,16,5,0;15,63,15,1,0;15,64,15,11,0|",
        "fire-lava-ignition-58|lava|15,64,15|11|1|2|false|i|0|0|0|0|-1|false|17|6001|false|false|false|14,66,16,58,0;15,63,15,1,0;15,64,15,11,0|",
        "fire-world-tick-lava|world-tick|15,64,15|11|1|2|false|i|0|0|0|0|0|false|17|6001|false|false|false|14,66,16,5,0;15,63,15,1,0;15,64,15,11,0|",
        "fire-neighbor-a-null|neighbor|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|true|false|false|15,64,15,51,0;16,64,15,5,0|",
        "fire-tick-b-null|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|false|true|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,5,0|",
        "fire-tick-a-null|tick|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|true|false|false|15,63,15,1,0;15,64,15,51,0|",
        "fire-neighbor-a-null-provider-null|neighbor|15,64,15|51|1|1|false|i|0|0|0|0|-1|false|17|6001|true|false|true|15,64,15,51,0|",
        "fire-world-random-player|world-tick|-57,64,-56|51|1|1|false|i|0|0|0|0|-1|true|-332372929|6001|false|false|false|-57,63,-56,87,0;-57,64,-56,51,0|",
        "fire-world-tick-tnt-added-error|world-tick|15,64,15|51|1|4|false|i|0|0|1|0|0|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|",
        "fire-world-tick-tnt-sound-error|world-tick|15,64,15|51|1|4|false|i|0|0|0|1|0|false|17|6001|false|false|false|15,63,15,1,0;15,64,15,51,5;16,63,15,1,0;16,64,15,46,0|"
    };
    static final class Profile {
        final Map<String,Object> values = new LinkedHashMap<>();
        Profile(String text) {
            String[] fields=text.split("\\|",-1);
            PlayerDamageWakeObservation.need(fields.length==KEYS.length,"declared Fire operands");
            for(int i=0;i<KEYS.length;i++) {
                String key=KEYS[i], value=fields[i]; Object parsed;
                if(key.equals("mode")||key.equals("operation")||key.equals("biome"))parsed=value;
                else if(key.equals("cells")||key.equals("heights")) {
                    List<Object> rows=new ArrayList<>();
                    if(!value.isEmpty())for(String row:value.split(";"))rows.add(numbers(row));parsed=rows;
                }else if(key.equals("xyz"))parsed=numbers(value);
                else if(key.equals("rain")||key.equals("random_player")||key.startsWith("null_"))parsed=Boolean.valueOf(value);
                else parsed=Integer.valueOf(value);
                values.put(key,parsed);
            }
        }
        String text(String key){return (String)values.get(key);}
        int number(String key){return (Integer)values.get(key);}
        boolean flag(String key){return (Boolean)values.get(key);}
        int coordinate(int i){return (Integer)((List<?>)values.get("xyz")).get(i);}
        static List<Integer> numbers(String text){List<Integer> out=new ArrayList<>();for(String word:text.split(","))out.add(Integer.valueOf(word));return out;}
    }
    static fd world;
    static final List<lm> chunks=new ArrayList<>();
    static Random math;
    static Profile profile;
    static Object preparedProvider;
    static Object get(Object value,Class<?> owner,String name)throws Exception{return PlayerDamageWakeObservation.get(value,owner,name);}
    static void put(Object value,Class<?> owner,String name,Object data)throws Exception{PlayerDamageWakeObservation.put(value,owner,name,data);}
    static String d(double value){return PlayerDamageWakeObservation.d(value);}
    static String f(float value){return PlayerDamageWakeObservation.f(value);}
    static Object random(Random value)throws Exception{return PlayerDamageWakeObservation.random(value);}
    static Map<String,Object> map(Object... values){return ConstructorJson.map(values);}
    static Object classField(Class<?> owner,String name,boolean isLong)throws Exception {
        Class<?> type=Class.forName("sun.misc.Unsafe");Object unsafe=get(null,type,"theUnsafe");
        Field field=owner.getDeclaredField(name);
        boolean initialized=!(Boolean)type.getMethod("shouldBeInitialized",Class.class).invoke(unsafe,owner);
        Object base=type.getMethod("staticFieldBase",Field.class).invoke(unsafe,field);
        long offset=(Long)type.getMethod("staticFieldOffset",Field.class).invoke(unsafe,field);
        Object value=type.getMethod(isLong?"getLong":"getInt",Object.class,long.class).invoke(unsafe,base,offset);
        return isLong?map("initialized",initialized,"sequence",String.format(Locale.ROOT,"%016x",(Long)value)):
            map("initialized",initialized,"next_id",value);
    }
    static Object preparedClass()throws Exception{return map("scheduled",classField(qy.class,"f",true),"entity",classField(sn.class,"a",false));}
    static lm chunk(int x,int z){for(lm c:chunks)if(c.j==(x>>4)&&c.k==(z>>4))return c;throw new AssertionError("undeclared acquired Fire footprint");}
    static List<Integer> cell(int x,int y,int z){if(y<0||y>=128)return List.of(0,0);lm c=chunk(x,z);return List.of(c.b[((x&15)<<11)|((z&15)<<7)|y]&255,c.e.a(x&15,y,z&15));}
    static void cell(int x,int y,int z,int id,int meta){lm c=chunk(x,z);c.b[((x&15)<<11)|((z&15)<<7)|y]=(byte)id;c.e.a(x&15,y,z&15,meta);}
    static Object target(){return cell(profile.coordinate(0),profile.coordinate(1),profile.coordinate(2));}
    static Object baseEntity(sn e) {
        return map("class",e.getClass().getName(),"id",e.aD,
            "position",List.of(d(e.aM),d(e.aN),d(e.aO)),"motion",List.of(d(e.aP),d(e.aQ),d(e.aR)),
            "previous",List.of(d(e.aJ),d(e.aK),d(e.aL)),"dimensions",List.of(f(e.bg),f(e.bh)),"offset",f(e.bf),
            "bounds",List.of(d(e.aW.a),d(e.aW.b),d(e.aW.c),d(e.aW.d),d(e.aW.e),d(e.aW.f)),
            "removed",e.be,"on_ground",e.aX,"world_same",e.aI==world);
    }
    static Object entity(qw e){Map<String,Object> r=(Map<String,Object>)baseEntity(e);r.put("fuse",e.a);r.put("prevent_spawning",e.aF);return r;}
    static Object entities(){List<Object> out=new ArrayList<>();for(Object e:world.b){PlayerDamageWakeObservation.need(e instanceof qw||e instanceof dc,"declared Entity kinds");if(e instanceof qw)out.add(entity((qw)e));}return out;}
    static Object players(){List<Object> out=new ArrayList<>();for(Object value:world.d){dc e=(dc)value;Map<String,Object> r=(Map<String,Object>)baseEntity(e);r.remove("previous");r.remove("world_same");r.put("health",e.Y);r.put("sleeping",e.u);out.add(r);}return out;}
    static Object arrays()throws Exception{return map("encouragement",array(get(uu.as,yq.class,"a")),"flammability",array(get(uu.as,yq.class,"b")));}
    static Object array(Object data){if(data==null)return null;List<Integer> out=new ArrayList<>();for(int n:(int[])data)out.add(n);return out;}
    static Object state()throws Exception {
        Map<Integer,lm> cache=(Map<Integer,lm>)get(preparedProvider,ok.class,"e");
        PlayerDamageWakeObservation.need(cache.size()==chunks.size(),"complete unchanged prepared cache footprint");
        for(lm c:chunks)PlayerDamageWakeObservation.need(cache.get(yy.a(c.j,c.k))==c,"same prepared Chunk receiver");
        PlayerDamageWakeObservation.need((get(world,fd.class,"v")==null)==profile.flag("null_provider"),"actual declared provider field");
        List<Object> planes=new ArrayList<>(),regions=new ArrayList<>(),order=new ArrayList<>();
        for(lm c:chunks)planes.add(map("xyz",List.of(c.j,c.k),"planes",map("blocks",PlayerDamageWakeObservation.hash(c.b),
            "metadata",PlayerDamageWakeObservation.hash(c.e.a),"sky",PlayerDamageWakeObservation.hash(c.f.a),
            "block_light",PlayerDamageWakeObservation.hash(c.g.a),"height",PlayerDamageWakeObservation.hash(c.h))));
        for(Object value:(List<?>)get(world,fd.class,"C")){st r=(st)value;regions.add(map("kind",r.a.name(),"bounds",List.of(r.b,r.c,r.d,r.e,r.f,r.g)));}
        for(Object value:(java.util.Set<?>)get(world,fd.class,"P")){yy r=(yy)value;order.add(List.of(r.a,r.b));}
        return map("target",target(),"world_random",random(world.r),"math_random",random(math),"time",String.format(Locale.ROOT,"%016x",world.x.f()),
            "world_o",world.o,"sky_seen",lm.a,"scheduled",UprightPlantsWakeObservation.scheduled(),
            "entity_class",classField(sn.class,"a",false),"lighting",map("depth",fd.A,"regions",regions),"fire_arrays",arrays(),
            "entities",entities(),"players",players(),"entity_count",world.b.size(),"player_count",world.d.size(),
            "weather",map("rain_previous",f((Float)get(world,fd.class,"i")),"rain_current",f((Float)get(world,fd.class,"j")),
                "thunder_previous",f((Float)get(world,fd.class,"k")),"thunder_current",f((Float)get(world,fd.class,"l")),
                "rain_time",world.x.p(),"thunder_time",world.x.n(),"raining",world.x.o(),"thundering",world.x.m()),
            "random_work",map("g",get(world,fd.class,"g"),"cooldown",get(world,fd.class,"Q"),"candidates",order),"chunk_planes",planes);
    }
    static final class Observer implements pm {
        final List<Object> events=new ArrayList<>();int cells,ranges,added,sounds;
        Map<String,Object> event(String kind)throws Exception {
            PlayerDamageWakeObservation.need(events.size()<1024,"bounded genuine Fire callbacks");
            return map("kind",kind,"target",target(),"world_o",world.o,"world_random",random(world.r),"math_random",random(math),
                "entities",entities(),"stack",UprightPlantsWakeObservation.stack(Thread.currentThread().getStackTrace()));
        }
        public void a(int x,int y,int z){try{Map<String,Object> e=event("cell");e.put("xyz",List.of(x,y,z));e.put("cell",cell(x,y,z));events.add(e);}catch(Exception e){throw new AssertionError(e);}if(++cells==profile.number("cell_error_at"))throw new IllegalStateException("Fire cell observer interruption");}
        public void b(int x,int y,int z,int a,int b,int c){try{Map<String,Object> e=event("range");e.put("xyz",List.of(x,y,z,a,b,c));events.add(e);}catch(Exception e){throw new AssertionError(e);}if(++ranges==profile.number("range_error_at"))throw new IllegalStateException("Fire range observer interruption");}
        public void a(sn value){PlayerDamageWakeObservation.need(value instanceof qw,"actual TNT publication");try{Map<String,Object> e=event("added");e.put("entity",entity((qw)value));events.add(e);}catch(Exception e){throw new AssertionError(e);}if(++added==profile.number("added_error_at"))throw new IllegalStateException("Fire added observer interruption");}
        public void a(String name,double x,double y,double z,float volume,float pitch){try{Map<String,Object> e=event("sound");e.put("name",name);e.put("position",List.of(d(x),d(y),d(z)));e.put("volume",f(volume));e.put("pitch",f(pitch));events.add(e);}catch(Exception e){throw new AssertionError(e);}if(++sounds==profile.number("sound_error_at"))throw new IllegalStateException("Fire sound observer interruption");}
        public void e(){try{events.add(event("sky"));}catch(Exception e){throw new AssertionError(e);}}
        public void b(sn value){throw new AssertionError("unexpected Entity removal");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow value){throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static Object setup(Profile p,int index)throws Exception {
        profile=p;chunks.clear();
        String name="Fire"+index;
        mx handler=new mx(PlayerDamageWakeObservation.directory.toFile(),name,false);
        ei info=new ei(23L,"Fire");info.a(8,64,8);info.d(19132);info.e(30);info.f(20);info.b(p.flag("rain"));handler.a(info);
        world=new fd(handler,"Fire",-99L,xa.a(0));world.a(false,false);world.B=false;preparedProvider=world.w();
        PlayerDamageWakeObservation.world=world;
        world.t.b=new hd((kd)PlayerDamageWakeObservation.field(kd.class,p.text("biome")).get(null),.5,.5);
        Map<Integer,lm> cache=(Map<Integer,lm>)get(world.w(),ok.class,"e");List loaded=(List)get(world.w(),ok.class,"f");
        PlayerDamageWakeObservation.need(cache.isEmpty()&&loaded.isEmpty(),"genuine saved World has no prepared cache entries");
        int minX=p.flag("random_player")?-10:0,maxX=p.flag("random_player")?8:1;
        int minZ=p.flag("random_player")?-7:0,maxZ=p.flag("random_player")?11:1;
        for(int x=minX;x<=maxX;x++)for(int z=minZ;z<=maxZ;z++){lm c=new lm(world,new byte[32768],x,z);c.n=true;chunks.add(c);cache.put(yy.a(x,z),c);loaded.add(c);}
        for(Object value:(List<?>)p.values.get("cells")){List<Integer> c=(List<Integer>)value;cell(c.get(0),c.get(1),c.get(2),c.get(3),c.get(4));}
        for(Object value:(List<?>)p.values.get("heights")){List<Integer> c=(List<Integer>)value;chunk(c.get(0),c.get(1)).h[((c.get(1)&15)<<4)|(c.get(0)&15)]=(byte)(int)c.get(2);}
        put(world,fd.class,"i",p.flag("rain")?1f:0f);put(world,fd.class,"j",p.flag("rain")?1f:0f);
        put(world,fd.class,"k",0f);put(world,fd.class,"l",0f);put(world,fd.class,"g",p.number("g"));put(world,fd.class,"Q",p.number("cooldown"));
        world.f=p.flag("rain")?5:0;
        Object classes=preparedClass();
        if(p.flag("random_player")){
            kq app=PlayerDamageWakeObservation.app;dc player=new dc(app,world,app.k,0);app.f=world;app.h=player;app.i=player;
            player.e(-.5,64,32.5);PlayerDamageWakeObservation.need(world.b(player),"genuine Player publication");
        }
        world.r.setSeed(p.number("random_seed"));math.setSeed(0);
        if(p.number("prequeue_delay")>=0)world.c(p.coordinate(0),p.coordinate(1),p.coordinate(2),p.number("entry_id"),p.number("prequeue_delay"));
        return classes;
    }
    static void run(Profile p,int index)throws Exception {
        Object classes=setup(p,index);Observer observer=new Observer();world.a(observer);
        Object a=get(uu.as,yq.class,"a"),b=get(uu.as,yq.class,"b"),provider=get(world,fd.class,"v");
        if(p.flag("null_encouragement"))put(uu.as,yq.class,"a",null);
        if(p.flag("null_flammability"))put(uu.as,yq.class,"b",null);
        if(p.flag("null_provider"))put(world,fd.class,"v",null);
        Object before=state(),result=null;String exception="NONE",message=null;List<Object> stack=new ArrayList<>();
        try {
            uu entry=uu.m[p.number("entry_id")];int x=p.coordinate(0),y=p.coordinate(1),z=p.coordinate(2);
            switch(p.text("operation")){
                case "added":result=world.f(x,y,z,p.number("entry_id"));break;
                case "neighbor":entry.b(world,x,y,z,p.number("source_id"));break;
                case "tick":case "lava":entry.a(world,x,y,z,world.r);break;
                case "world-tick":world.l();break;
                default:throw new AssertionError("declared entry");
            }
        }catch(RuntimeException failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();stack=UprightPlantsWakeObservation.stack(failure.getStackTrace());}
        Object after=state();
        System.out.println("FIRE "+ConstructorJson.json(map("mode",p.text("mode"),"prepared",p.values,"prepared_class",classes,
            "receiver",map("class",uu.m[p.number("entry_id")].getClass().getName(),"fire_class",uu.as.getClass().getName()),
            "before",before,"after",after,"result",result,"exception",exception,"exception_message",message,"exception_stack",stack,"events",observer.events)));
        put(uu.as,yq.class,"a",a);put(uu.as,yq.class,"b",b);put(world,fd.class,"v",provider);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        PlayerDamageWakeObservation.need(args.length==3,"archive, isolated directory, class names");Locale.setDefault(Locale.ROOT);
        PlayerDamageWakeObservation.archive=Path.of(args[0]).toRealPath();PlayerDamageWakeObservation.directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new PlayerDamageWakeObservation.Guard());PlayerDamageWakeObservation.bind(Path.of(args[2]));
        kq app=new kq(null,null,null,854,480,false,null);PlayerDamageWakeObservation.app=app;
        try {
            app.k=new gr("FireComponent","-");put(app,net.minecraft.client.Minecraft.class,"Z",PlayerDamageWakeObservation.directory.toFile());
            PlayerDamageWakeObservation.writer=new xi(app.k,PlayerDamageWakeObservation.directory.toFile());app.I=PlayerDamageWakeObservation.writer;
            ep.f.a(new kh(app));PlayerDamageWakeObservation.workers();
            math=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");PlayerDamageWakeObservation.shared=math;
            for(int i=0;i<DATA.length;i++)run(new Profile(DATA[i]),i);
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.network==0&&PlayerDamageWakeObservation.nativeLinks==0&&PlayerDamageWakeObservation.rejectedWrites==0&&PlayerDamageWakeObservation.execs==0,"effect guards");
        }finally{if(PlayerDamageWakeObservation.writer!=null)PlayerDamageWakeObservation.workers();app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);PlayerDamageWakeObservation.need(!t.isAlive(),"Timer worker retirement");}}
        System.out.println("FIRE_COMPLETE cases="+DATA.length+" network=0 native=0 rejected_writes=0 exec=0 GUI=0");
    }
}
