import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Read-only debugger: actual Code, frames, arrays and return values; no target invocation. */
public final class RandomTickJdi {
    private final VirtualMachine vm;
    private final PrintWriter output;
    private final Set<String> installed=new HashSet<>();
    private final Map<Long,BreakpointRequest> worldPoints=new HashMap<>();
    private final List<BreakpointRequest> leafPoints=new ArrayList<>();
    private MethodExitRequest chunkExit,randomExit;
    private int active=-1,acquisitions,completed,samples,ticks,ambient,lightning,climate;
    private ObjectReference held;
    private boolean pendingSound;
    private final Map<Integer,Integer> tickIds=new TreeMap<>();
    private RandomTickJdi(VirtualMachine vm,PrintWriter output){this.vm=vm;this.output=output;}
    private ReferenceType type(String n){List<ReferenceType> a=vm.classesByName(n);return a.isEmpty()?null:a.get(0);}
    private Value stat(String c,String n){ReferenceType t=type(c);return t==null||!t.isPrepared()?null:t.getValue(t.fieldByName(n));}
    private static Value field(ObjectReference r,String c,String n){if(r==null)return null;for(Field f:r.referenceType().allFields())if(f.name().equals(n)&&f.declaringType().name().equals(c))return r.getValue(f);throw new IllegalArgumentException(c+"."+n);}
    private static int number(Value v){return ((IntegerValue)v).value();}
    private static Object value(Value v){
        if(v==null)return null;
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof ByteValue)return (int)((ByteValue)v).value();
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof FloatValue)return ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof LongValue)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));
        ObjectReference r=(ObjectReference)v;return ConstructorJson.map("class",r.referenceType().name(),"oid",r.uniqueID());
    }
    private static String seed(ObjectReference r){if(r==null)return null;ObjectReference a=(ObjectReference)field(r,"java.util.Random","seed");return String.format(Locale.ROOT,"%012x",((LongValue)field(a,"java.util.concurrent.atomic.AtomicLong","value")).value());}
    private ObjectReference world(){return (ObjectReference)stat("RandomTickTarget","world");}
    private void emit(Map<String,Object> r){output.println(ConstructorJson.json(r));output.flush();}
    private static Map<String,Object> location(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    private Map<String,Object> state(String event,Location l){ObjectReference w=world();return ConstructorJson.map("event",event,"case",active,"location",l==null?null:location(l),"g",value(field(w,"fd","g")),"Q",value(field(w,"fd","Q")),"m",value(field(w,"fd","m")),"world_seed",seed((ObjectReference)field(w,"fd","r")),"parameter_seed",seed((ObjectReference)stat("RandomTickTarget","parameter")));}
    private Method pin(ReferenceType t,String n,String d)throws Exception{
        List<Method> methods=t.methodsByName(n,d);if(methods.size()!=1)throw new IllegalStateException(t.name()+"."+n+d);
        Method m=methods.get(0);byte[] code=m.bytecodes();emit(ConstructorJson.map("event","code-pin","class",t.name(),"method",n,"descriptor",d,"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));return m;
    }
    private BreakpointRequest point(Method m,long pc){Location l=m.locationOfCodeIndex(pc);if(l==null)throw new IllegalStateException("BCI "+pc);BreakpointRequest r=vm.eventRequestManager().createBreakpointRequest(l);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);return r;}
    private void install(ReferenceType t)throws Exception{
        if(!t.isPrepared()||!installed.add(t.name()))return;
        switch(t.name()){
            case "RandomTickTarget":{ModificationWatchpointRequest r=vm.eventRequestManager().createModificationWatchpointRequest(t.fieldByName("activeCase"));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();break;}
            case "fd":{Method n=pin(t,"n","()V");for(long pc:new long[]{0,152,207,232,443,464,503,608,951,982,991,994})worldPoints.put(pc,point(n,pc));pin(t,"c","(II)Llm;");pin(t,"e","(II)I");pin(t,"t","(III)Z");pin(t,"a","(DDDD)Lgs;");break;}
            case "wp":case "bk":case "nk":case "ac":{Method m=pin(t,"a","(Lfd;IIILjava/util/Random;)V");leafPoints.add(point(m,0));break;}
            case "jr":pin(t,"a","(Lfd;IIILjava/util/Random;)V");break;
            case "lm":case "li":pin(t,"a","(III)I");pin(t,"b","(III)I");break;
            case "hd":pin(t,"<init>","(Lkd;DD)V");break;
            case "c":pin(t,"<init>","(Lfd;DDD)V");break;
            case "uu":pin(t,"a","(Lfd;IIILjava/util/Random;)V");pin(t,"a","(Lfd;IIIIF)V");break;
            case "hl":pin(t,"<init>","(Lfd;DDDLiz;)V");break;
            default:throw new IllegalStateException(t.name());
        }
    }
    private void begin(int next){
        if(active>=0){Map<String,Object> r=state("summary",null);r.put("acquisitions",acquisitions);r.put("completed_chunks",completed);r.put("detailed_samples",samples);r.put("observed_tick_calls",ticks);Map<String,Integer> ids=new TreeMap<>();for(Map.Entry<Integer,Integer> e:tickIds.entrySet())ids.put(e.getKey().toString(),e.getValue());r.put("tick_ids",ids);r.put("tick_scope",active==9||active==10?"whole-method":"first-chunk-only");r.put("ambient_steps",ambient);r.put("lightning_steps",lightning);r.put("climate_steps",climate);emit(r);}
        active=next;acquisitions=completed=samples=ticks=ambient=lightning=climate=0;tickIds.clear();held=null;pendingSound=false;
        boolean frame=next>=0&&next<100,leaf=next>=100;
        for(Map.Entry<Long,BreakpointRequest> p:worldPoints.entrySet())p.getValue().setEnabled(frame);
        for(BreakpointRequest p:leafPoints)p.setEnabled(leaf);
        randomExit.setEnabled(next>=0);chunkExit.disable();
    }
    private void breakpoint(BreakpointEvent e)throws Exception{
        Location l=e.location();if(!l.declaringType().name().equals("fd")){emit(state("leaf-entry",l));return;}
        long pc=l.codeIndex();
        if(pc==207){++acquisitions;if(acquisitions==1||active==9||active==10)chunkExit.enable();return;}
        if(pc==232){++ambient;if(acquisitions>1)return;}
        if(pc==443)pendingSound=true;
        if(pc==464){if(!pendingSound)return;pendingSound=false;}
        if(pc==503)++lightning;
        if(pc==608)++climate;
        if(pc==991){++completed;if(completed==1){worldPoints.get(951L).disable();if(active!=9&&active!=10)worldPoints.get(982L).disable();randomExit.disable();}return;}
        Map<String,Object> r=state("checkpoint",l);
        if(pc==152&&active==1){
            ObjectReference set=(ObjectReference)field(world(),"fd","P"),map=(ObjectReference)field(set,"java.util.HashSet","map");
            ArrayReference table=(ArrayReference)field(map,"java.util.HashMap","table");List<Object> order=new ArrayList<>();
            for(Value start:table.getValues()){ObjectReference node=(ObjectReference)start;while(node!=null){ObjectReference key=(ObjectReference)node.getValue(node.referenceType().fieldByName("key"));order.add(List.of(value(field(key,"yy","a")),value(field(key,"yy","b"))));node=(ObjectReference)node.getValue(node.referenceType().fieldByName("next"));}}
            r.put("actual_P_order",order);r.put("actual_P_capacity",table.length());r.put("actual_P_size",value(field(map,"java.util.HashMap","size")));
        }
        if(pc==951||pc==982){
            int g=number(field(world(),"fd","g")),v=g>>2,x=v&15,z=(v>>8)&15,y=(v>>16)&127,packed=(x<<11)|(z<<7)|y;
            ArrayReference raw=(ArrayReference)field(held,"lm","b");int id=((ByteValue)raw.getValue(packed)).value()&255;
            r.put("xyz",List.of(x,y,z));r.put("packed",packed);r.put("raw_id",id);r.put("held_class",held.referenceType().name());r.put("held_coords",List.of(number(field(held,"lm","j")),number(field(held,"lm","k"))));
            if(pc==951){r.put("sample",samples++);}else{++ticks;tickIds.merge(id,1,Integer::sum);Value block=((ArrayReference)stat("uu","m")).getValue(id);r.put("receiver",value(block));if(acquisitions>1)return;}
        }
        emit(r);
    }
    private void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();
        randomExit=manager.createMethodExitRequest();randomExit.addClassFilter("java.util.Random");randomExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        chunkExit=manager.createMethodExitRequest();chunkExit.addClassFilter("fd");chunkExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        for(String n:new String[]{"RandomTickTarget","fd","wp","bk","nk","ac","jr","lm","li","hd","c","uu","hl"}){ClassPrepareRequest r=manager.createClassPrepareRequest();r.addClassFilter(n);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();ReferenceType t=type(n);if(t!=null)install(t);}
        ExceptionRequest exceptions=manager.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_ALL);exceptions.enable();
        boolean alive=true;
        while(alive){EventSet set=vm.eventQueue().remove(60000);if(set==null)throw new IllegalStateException("JDI event timeout");
            for(Event event:set){
                if(event instanceof ClassPrepareEvent)install(((ClassPrepareEvent)event).referenceType());
                else if(event instanceof ModificationWatchpointEvent)begin(number(((ModificationWatchpointEvent)event).valueToBe()));
                else if(event instanceof BreakpointEvent&&active>=0)breakpoint((BreakpointEvent)event);
                else if(event instanceof MethodExitEvent&&active>=0){
                    MethodExitEvent e=(MethodExitEvent)event;Method m=e.method();
                    if(m.declaringType().name().equals("fd")){if(m.name().equals("c")&&m.signature().equals("(II)Llm;")){held=(ObjectReference)e.returnValue();if(acquisitions==1){Map<String,Object> r=state("held-chunk",e.location());r.put("chunk",value(held));r.put("coords",List.of(value(field(held,"lm","j")),value(field(held,"lm","k"))));r.put("raw",value(field(held,"lm","b")));emit(r);}chunkExit.disable();}}
                    else if((m.name().equals("nextInt")&&m.signature().equals("(I)I"))||(m.name().equals("nextFloat")&&m.signature().equals("()F"))||(m.name().equals("nextDouble")&&m.signature().equals("()D"))){
                        StackFrame f=e.thread().frame(0);ObjectReference r=f.thisObject();ObjectReference w=(ObjectReference)field(world(),"fd","r"),p=(ObjectReference)stat("RandomTickTarget","parameter");
                        ObjectReference math=(ObjectReference)stat("java.lang.Math$RandomNumberGeneratorHolder","randomNumberGenerator");
                        if(Objects.equals(r,w)||Objects.equals(r,p)||Objects.equals(r,math)){List<Object> args=new ArrayList<>();for(Value a:f.getArgumentValues())args.add(value(a));emit(ConstructorJson.map("event","random","case",active,"role",Objects.equals(r,w)?"world":Objects.equals(r,p)?"parameter":"math","method",m.name(),"arguments",args,"result",value(e.returnValue()),"seed",seed(r)));}
                    }
                }
                else if(event instanceof ExceptionEvent&&active>=0){ExceptionEvent e=(ExceptionEvent)event;List<Object> frames=new ArrayList<>();boolean source=false;for(StackFrame f:e.thread().frames()){String n=f.location().declaringType().name();if(n.equals("fd")||n.equals("wp")||n.equals("bk")||n.equals("nk")||n.equals("ac")||n.equals("uu")||n.startsWith("RandomTickTarget")){source=true;frames.add(location(f.location()));}}if(source){Map<String,Object> r=state("source_exception",e.location());r.put("exception",e.exception().referenceType().name());r.put("source_frames",frames);emit(r);}}
                else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
            }
            try{set.resume();}catch(VMDisconnectedException expected){alive=false;}
        }
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=4)throw new IllegalArgumentException("jar classes output jdk-home");
        Path directory=Path.of(args[2]).toAbsolutePath();Files.createDirectories(directory);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[3]);options.get("main").setValue("RandomTickTarget target-observations.ndjson \""+Path.of(args[0]).toAbsolutePath()+"\"");
        String cp=Path.of(args[1]).toAbsolutePath()+File.pathSeparator+Path.of(args[0]).toAbsolutePath();
        options.get("options").setValue("-Xint -Xmx512m -Xverify:all --add-opens=java.base/java.util=ALL-UNNAMED -Drandom.tick.classes=\""+directory.resolve("classes.txt")+"\" -Xlog:class+init=info:file=class-init.log -classpath \""+cp+"\"");options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options);Process process=vm.process();
        Thread stdout=new Thread(()->{try(OutputStream sink=Files.newOutputStream(directory.resolve("target.stdout"))){process.getInputStream().transferTo(sink);}catch(IOException e){throw new UncheckedIOException(e);}});
        Thread stderr=new Thread(()->{try(OutputStream sink=Files.newOutputStream(directory.resolve("target.stderr"))){process.getErrorStream().transferTo(sink);}catch(IOException e){throw new UncheckedIOException(e);}});stdout.start();stderr.start();
        boolean observed=false;
        try(PrintWriter output=new PrintWriter(Files.newBufferedWriter(directory.resolve("checkpoints.ndjson"),StandardCharsets.UTF_8))){new RandomTickJdi(vm,output).run();if(output.checkError())throw new IOException("JDI output");observed=true;}
        finally{try{vm.dispose();}catch(VMDisconnectedException expected){}if(!observed)process.destroyForcibly();}
        if(!process.waitFor(10,java.util.concurrent.TimeUnit.SECONDS)){process.destroyForcibly();process.waitFor();throw new IllegalStateException("target exit timeout");}
        stdout.join();stderr.join();if(process.exitValue()!=0)throw new IllegalStateException("target exit "+process.exitValue());
    }
}
