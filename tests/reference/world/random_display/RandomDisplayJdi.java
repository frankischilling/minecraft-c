import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Standard read-only JDI observations: no target invocation, fields writes, or source replacement. */
public final class RandomDisplayJdi {
    final VirtualMachine vm;final PrintWriter output;final Set<String> installed=new HashSet<>();
    final List<BreakpointRequest> details=new ArrayList<>();final Map<Long,BreakpointRequest> sampler=new HashMap<>();
    final Map<String,Integer> entries=new TreeMap<>();final List<MethodExitRequest> exits=new ArrayList<>();
    MethodExitRequest randomExit,systemExit,worldExit;int active=-1,samples,queries,selected,draws;ObjectReference local;String localInitial;
    static final Set<String> BODIES=Set.of("uu","ss","bs","rp","vm","db","tc","sm","wo","ak","yq");
    static final Set<String> QUERIES=Set.of("a(III)I","e(III)I","f(III)Lln;","g(III)Z","h(III)Z");
    RandomDisplayJdi(VirtualMachine vm,PrintWriter output){this.vm=vm;this.output=output;}
    ReferenceType type(String n){List<ReferenceType>a=vm.classesByName(n);return a.isEmpty()?null:a.get(0);}
    Value stat(String c,String n){ReferenceType t=type(c);return t==null||!t.isPrepared()?null:t.getValue(t.fieldByName(n));}
    static Value field(ObjectReference r,String c,String n){if(r==null)return null;for(Field f:r.referenceType().allFields())if(f.name().equals(n)&&f.declaringType().name().equals(c))return r.getValue(f);throw new IllegalArgumentException(c+"."+n);}
    static int integer(Value v){return((IntegerValue)v).value();}
    static Object value(Value v){
        if(v==null)return null;if(v instanceof IntegerValue)return integer(v);if(v instanceof BooleanValue)return((BooleanValue)v).value();
        if(v instanceof FloatValue)return ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof StringReference)return((StringReference)v).value();if(v instanceof VoidValue)return null;
        ObjectReference r=(ObjectReference)v;return ConstructorJson.map("class",r.referenceType().name(),"oid",r.uniqueID());
    }
    String seed(ObjectReference r){if(r==null)return null;return String.format(Locale.ROOT,"%012x",((LongValue)field((ObjectReference)field(r,"java.util.Random","seed"),"java.util.concurrent.atomic.AtomicLong","value")).value());}
    ObjectReference world(){return(ObjectReference)stat("RandomDisplayTarget","world");}
    void emit(Map<String,Object> r){output.println(ConstructorJson.json(r));output.flush();}
    static Map<String,Object> location(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    List<Object> arguments(StackFrame f)throws Exception{List<Object>a=new ArrayList<>();for(Value v:f.getArgumentValues())a.add(value(v));return a;}
    Method pin(ReferenceType t,String n,String d)throws Exception{List<Method>a=t.methodsByName(n,d);if(a.size()!=1)throw new IllegalStateException(t.name()+"."+n+d);Method m=a.get(0);byte[] code=m.bytecodes();emit(ConstructorJson.map("event","code-pin","class",t.name(),"method",n,"descriptor",d,"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));return m;}
    BreakpointRequest point(Method m,long pc){BreakpointRequest r=vm.eventRequestManager().createBreakpointRequest(m.locationOfCodeIndex(pc));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);return r;}
    void install(ReferenceType t)throws Exception{
        if(!t.isPrepared()||!installed.add(t.name()))return;
        String n=t.name();if(n.equals("RandomDisplayTarget")){ModificationWatchpointRequest r=vm.eventRequestManager().createModificationWatchpointRequest(t.fieldByName("active"));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
        else if(n.equals("fd")){
            Method m=pin(t,"q","(III)V");for(long pc:new long[]{0,13,31,41,54,64,77,87,100,125,128,134})sampler.put(pc,point(m,pc));
            pin(t,"c","(II)Llm;");
            for(String[] s:new String[][]{{"a","(III)I"},{"e","(III)I"},{"f","(III)Lln;"},{"g","(III)Z"},{"h","(III)Z"},{"a","(DDDLjava/lang/String;FF)V"},{"a","(Ljava/lang/String;DDDDDD)V"}})details.add(point(pin(t,s[0],s[1]),0));
        }else if(BODIES.contains(n)){
            details.add(point(pin(t,"b","(Lfd;IIILjava/util/Random;)V"),0));
            if(n.equals("bs"))details.add(point(pin(t,"i","(Lfd;III)V"),0));
            if(n.equals("yq"))details.add(point(pin(t,"c","(Lxp;III)Z"),0));
        }else if(n.equals("lm")||n.equals("li"))pin(t,"a","(III)I");
    }
    void begin(int next){
        if(active>=0)emit(ConstructorJson.map("event","summary","case",active,"completed_samples",samples,"query_sites",queries,"selected_sites",selected,"world_random_sites",draws,"local_oid",local==null?null:local.uniqueID(),"local_seed_initial",localInitial,"local_seed_final",seed(local),"detail_scope",active<1000?"first16samples":"whole-direct-call","entries",new TreeMap<>(entries)));
        active=next;samples=queries=selected=draws=0;entries.clear();local=null;localInitial=null;
        for(BreakpointRequest r:sampler.values())r.setEnabled(next>=0&&next<1000);for(BreakpointRequest r:details)r.setEnabled(next>=0);
        randomExit.setEnabled(next>=0);systemExit.setEnabled(next>=0&&next<1000);worldExit.setEnabled(next>=1000);
    }
    void breakpoint(BreakpointEvent e)throws Exception{
        Location l=e.location();String owner=l.declaringType().name(),name=l.method().name(),descriptor=l.method().signature();long pc=l.codeIndex();
        if(owner.equals("fd")&&name.equals("q")){
            if(pc==31||pc==41||pc==54||pc==64||pc==77||pc==87){draws++;return;}if(pc==100){queries++;return;}if(pc==125){selected++;return;}
            if(pc==128){samples++;if(samples==16)for(BreakpointRequest r:details)r.disable();return;}
            if(pc==13){randomExit.disable();systemExit.disable();}
            emit(ConstructorJson.map("event","sampler-checkpoint","case",active,"location",location(l),"world_seed",seed((ObjectReference)field(world(),"fd","r")),"local_seed",seed(local)));return;
        }
        StackFrame f=e.thread().frame(0);String key=owner+"."+name+descriptor;entries.merge(key,1,Integer::sum);
        Map<String,Object> r=ConstructorJson.map("event","entry","case",active,"location",location(l),"arguments",arguments(f));
        if(BODIES.contains(owner)&&name.equals("b")){
            ObjectReference supplied=(ObjectReference)f.getArgumentValues().get(4);r.put("receiver_class",f.thisObject().referenceType().name());r.put("argument_oid",supplied==null?null:supplied.uniqueID());r.put("argument_seed",seed(supplied));r.put("is_sampler_local",active<1000&&Objects.equals(local,supplied));r.put("is_direct_argument",active>=1000&&Objects.equals(supplied,stat("RandomDisplayTarget","argument")));r.put("is_current_world_random",supplied!=null&&Objects.equals(supplied,field(world(),"fd","r")));
        }
        emit(r);
    }
    void returned(MethodExitEvent e)throws Exception{
        Method m=e.method();String n=m.name(),owner=m.declaringType().name(),descriptor=m.signature();
        if(owner.equals("fd")){
            if(QUERIES.contains(n+descriptor))emit(ConstructorJson.map("event","query-return","case",active,"location",location(e.location()),"arguments",arguments(e.thread().frame(0)),"result",value(e.returnValue())));return;
        }
        if(active<1000){
            if(n.equals("seedUniquifier")||n.equals("nanoTime"))emit(ConstructorJson.map("event","clock","case",active,"kind",n,"value",String.format(Locale.ROOT,"%016x",((LongValue)e.returnValue()).value())));
            if(owner.equals("java.util.Random")&&n.equals("<init>")&&descriptor.equals("()V")){local=e.thread().frame(0).thisObject();localInitial=seed(local);emit(ConstructorJson.map("event","local-constructor","case",active,"oid",local.uniqueID(),"seed",localInitial,"is_world_random",Objects.equals(local,field(world(),"fd","r"))));}return;
        }
        if(!(n.equals("nextInt")&&descriptor.equals("(I)I"))&&!(n.equals("nextFloat")&&descriptor.equals("()F")))return;
        StackFrame f=e.thread().frame(0);ObjectReference r=f.thisObject();String role=Objects.equals(r,stat("RandomDisplayTarget","argument"))?"argument":Objects.equals(r,stat("RandomDisplayTarget","original"))?"original":Objects.equals(r,stat("RandomDisplayTarget","replacement"))?"replacement":"other";
        emit(ConstructorJson.map("event","random","case",active,"role",role,"oid",r.uniqueID(),"method",n,"arguments",arguments(f),"result",value(e.returnValue()),"seed",seed(r)));
    }
    void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();
        randomExit=manager.createMethodExitRequest();randomExit.addClassFilter("java.util.Random");randomExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        systemExit=manager.createMethodExitRequest();systemExit.addClassFilter("java.lang.System");systemExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        worldExit=manager.createMethodExitRequest();worldExit.addClassFilter("fd");worldExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        for(String n:new String[]{"RandomDisplayTarget","fd","uu","ss","bs","rp","vm","db","tc","sm","wo","ak","yq","lm","li"}){ClassPrepareRequest r=manager.createClassPrepareRequest();r.addClassFilter(n);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();ReferenceType t=type(n);if(t!=null)install(t);}
        ExceptionRequest errors=manager.createExceptionRequest(null,true,true);errors.setSuspendPolicy(EventRequest.SUSPEND_ALL);errors.enable();
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(60000);if(set==null)throw new IllegalStateException("event timeout");for(Event event:set){
            if(event instanceof ClassPrepareEvent)install(((ClassPrepareEvent)event).referenceType());
            else if(event instanceof ModificationWatchpointEvent)begin(integer(((ModificationWatchpointEvent)event).valueToBe()));
            else if(event instanceof BreakpointEvent&&active>=0)breakpoint((BreakpointEvent)event);
            else if(event instanceof MethodExitEvent&&active>=0)returned((MethodExitEvent)event);
            else if(event instanceof ExceptionEvent&&active>=0){ExceptionEvent e=(ExceptionEvent)event;List<Object> frames=new ArrayList<>();for(StackFrame f:e.thread().frames()){String n=f.location().declaringType().name();if(n.equals("fd")||BODIES.contains(n)||n.startsWith("RandomDisplayTarget"))frames.add(location(f.location()));}if(!frames.isEmpty())emit(ConstructorJson.map("event","exception","case",active,"location",location(e.location()),"class",e.exception().referenceType().name(),"source_frames",frames));}
            else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException expected){alive=false;}}
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=5)throw new IllegalArgumentException("jar classes output jdk-home source-dir");
        Path out=Path.of(args[2]).toAbsolutePath();LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument>a=connector.defaultArguments();a.get("home").setValue(args[3]);a.get("main").setValue("RandomDisplayTarget target.ndjson \""+args[0]+"\" \""+args[4]+"\"");a.get("options").setValue("-Xint -Xverify:all -Xmx256m --add-opens=java.base/java.util=ALL-UNNAMED -Ddisplay.classes=\""+out.resolve("classes.txt")+"\" -Xlog:class+init=info:file=class-init.log -classpath \""+args[1]+File.pathSeparator+args[0]+"\"");a.get("suspend").setValue("true");VirtualMachine vm=connector.launch(a);Process process=vm.process();
        java.util.concurrent.atomic.AtomicReference<IOException> io=new java.util.concurrent.atomic.AtomicReference<>();
        Thread stdout=new Thread(()->{try(OutputStream sink=Files.newOutputStream(out.resolve("target.stdout"))){process.getInputStream().transferTo(sink);}catch(IOException e){io.compareAndSet(null,e);}}),stderr=new Thread(()->{try(OutputStream sink=Files.newOutputStream(out.resolve("target.stderr"))){process.getErrorStream().transferTo(sink);}catch(IOException e){io.compareAndSet(null,e);}});stdout.start();stderr.start();boolean observed=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(out.resolve("checkpoints.ndjson")))){new RandomDisplayJdi(vm,writer).run();if(writer.checkError())throw new IOException("JDI output");observed=true;}finally{try{vm.dispose();}catch(VMDisconnectedException expected){}if(!observed)process.destroyForcibly();}
        if(!process.waitFor(10,java.util.concurrent.TimeUnit.SECONDS)){process.destroyForcibly();process.waitFor();throw new IllegalStateException("target exit timeout");}stdout.join();stderr.join();if(io.get()!=null)throw io.get();if(process.exitValue()!=0)throw new IllegalStateException("target "+process.exitValue());
    }
}
