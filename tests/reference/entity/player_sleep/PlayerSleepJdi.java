import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicReference;

/** Read-only field/method/exception/breakpoint observations; never target invocation/write. */
public final class PlayerSleepJdi {
    final VirtualMachine vm;final PrintWriter output;
    final Map<Long,List<EventRequest>> local=new HashMap<>();
    final List<EventRequest> methods=new ArrayList<>();final Set<String> pins=new HashSet<>();
    final List<Map<String,Object>> prefix=new ArrayList<>(),completion=new ArrayList<>();
    ReferenceType helper,cw;ObjectReference actor,world,provider;Object initial,initialEnum,frontierState,frontierEnum;
    int active=-1,rows;long bytes,currentLoader;boolean frontier;
    PlayerSleepJdi(VirtualMachine v,PrintWriter o){vm=v;output=o;}
    static Map<String,Object> map(Object... x){return PlayerSleepObservation.map(x);}
    void emit(Object v){String s=PlayerSleepObservation.json(v);bytes+=s.length()+1;if(bytes>8L*1024*1024)throw new IllegalStateException("JDI stream limit");output.println(s);output.flush();if(output.checkError())throw new IllegalStateException("JDI output");}
    void event(Map<String,Object> e){List<Map<String,Object>> list=frontier?completion:prefix;if(list.size()>=128)throw new IllegalStateException("event bound");list.add(e);}
    static Map<String,Object> site(Location l){return map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",(int)l.codeIndex());}
    void pin(Method m)throws Exception{String k=m.declaringType().name()+"."+m.name()+m.signature();if(!pins.add(k))return;byte[] b=m.bytecodes();emit(map("event","code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b))));}
    static Object raw(Value v){if(v==null)return null;if(v instanceof VoidValue)return "VOID";if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof ByteValue)return (int)((ByteValue)v).value();if(v instanceof ShortValue)return (int)((ShortValue)v).value();if(v instanceof CharValue)return (int)((CharValue)v).value();if(v instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));if(v instanceof FloatValue)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));if(v instanceof DoubleValue)return map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));if(v instanceof StringReference)return map("string",((StringReference)v).value());return map("identity",((ObjectReference)v).uniqueID());}
    static Object objectState(ObjectReference v){if(v==null)return null;Map<String,Object> r=new TreeMap<>();List<Field> fs=new ArrayList<>();for(Field f:v.referenceType().allFields())if(!f.isStatic())fs.add(f);for(Map.Entry<Field,Value> e:v.getValues(fs).entrySet())r.put(e.getKey().declaringType().name()+"."+e.getKey().name(),raw(e.getValue()));return r;}
    Object state(){List<Object> coords=new ArrayList<>();for(String n:new String[]{"v","b","bN"}){Field f=null;for(Field p:actor.referenceType().allFields())if(p.declaringType().name().equals("gs")&&p.name().equals(n))f=p;coords.add(objectState((ObjectReference)actor.getValue(f)));}return map("actor",objectState(actor),"bounds",objectState((ObjectReference)actor.getValue(actor.referenceType().fieldByName("aW"))),"coordinates",coords,"world",objectState(world),"provider",objectState(provider));}
    static Object enumValue(Value v){if(v==null)return null;ObjectReference o=(ObjectReference)v;StringReference n=(StringReference)o.getValue(o.referenceType().fieldByName("name"));IntegerValue ordinal=(IntegerValue)o.getValue(o.referenceType().fieldByName("ordinal"));return map("name",n==null?null:n.value(),"ordinal",ordinal.value());}
    Object enumState(){List<Object> cells=new ArrayList<>(),ids=new ArrayList<>();for(String n:new String[]{"a","b","c","d","e"}){Value v=cw.getValue(cw.fieldByName(n));cells.add(enumValue(v));ids.add(v==null?null:((ObjectReference)v).uniqueID());}ArrayReference a=(ArrayReference)cw.getValue(cw.fieldByName("f"));List<Object> same=null;if(a!=null){same=new ArrayList<>();for(int i=0;i<a.length();++i){Value v=a.getValue(i);same.add(v!=null&&Objects.equals(((ObjectReference)v).uniqueID(),ids.get(i)));}}List<Object> matrix=new ArrayList<>();for(Object x:ids){List<Boolean> row=new ArrayList<>();for(Object y:ids)row.add(Objects.equals(x,y));matrix.add(row);}return map("initialized",cw.isInitialized(),"statics",cells,"identity_matrix",matrix,"values_same",same);}
    Object semantic(Value v){if(v==null)return null;if(v instanceof ObjectReference){ObjectReference o=(ObjectReference)v;if(o.equals(world))return "WORLD";if(o.equals(provider))return "PROVIDER";if(o.referenceType().name().equals("cw"))return enumValue(o);if(o instanceof ArrayReference){List<Object> r=new ArrayList<>();for(Value x:((ArrayReference)o).getValues())r.add(enumValue(x));return r;}}return raw(v);}
    void begin(int next)throws Exception{
        if(active>=0){Object ended=state(),endedEnum=enumState();emit(map("event","journal","case",active,"cw_before",initialEnum,"cw_prefix_after",frontier?frontierEnum:endedEnum,"preserved",initial.equals(frontier?frontierState:ended),"frontier119",frontier,"prefix",new ArrayList<>(prefix),"completion",new ArrayList<>(completion),"completion_preserved",initial.equals(ended),"cw_completion_after",endedEnum));++rows;
            for(EventRequest r:local.getOrDefault(currentLoader,List.of()))r.disable();for(EventRequest r:methods)r.disable();}
        active=next;prefix.clear();completion.clear();frontier=false;frontierState=null;frontierEnum=null;
        if(next>=0){actor=(ObjectReference)helper.getValue(helper.fieldByName("currentActor"));world=(ObjectReference)helper.getValue(helper.fieldByName("currentWorld"));provider=(ObjectReference)helper.getValue(helper.fieldByName("currentProvider"));cw=((ClassObjectReference)helper.getValue(helper.fieldByName("currentCW"))).reflectedType();currentLoader=actor.referenceType().classLoader().uniqueID();initial=state();initialEnum=enumState();if(cw.isInitialized())throw new IllegalStateException("CW was not cold");for(EventRequest r:local.getOrDefault(currentLoader,List.of()))r.enable();for(EventRequest r:methods)r.enable();}
    }
    boolean relevant(Location l){String c=l.declaringType().name(),n=l.method().name(),s=l.method().signature();return (c.equals("gs")&&(n.equals("b")&&s.equals("(III)Lcw;")||n.equals("N")&&s.equals("()Z")))||(c.equals("ls")&&n.equals("W")&&s.equals("()Z"))||(c.equals("fd")&&n.equals("f")&&s.equals("()Z"))||c.equals("cw");}
    void prepared(ReferenceType t)throws Exception{
        EventRequestManager m=vm.eventRequestManager();String n=t.name();
        if(n.equals("PlayerSleepObservation")){helper=t;ModificationWatchpointRequest r=m.createModificationWatchpointRequest(t.fieldByName("active"));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();return;}
        long loader=t.classLoader().uniqueID();List<EventRequest> requests=local.computeIfAbsent(loader,k->new ArrayList<>());
        Map<String,Set<String>> selected=Map.of("sn",Set.of("aI","be","aM","aN","aO"),"gs",Set.of("u"),"ls",Set.of("Y"),"fd",Set.of("B","t","f"),"xa",Set.of("c"),"cw",Set.of("a","b","c","d","e","f"));
        for(Field f:t.fields())if(selected.getOrDefault(n,Set.of()).contains(f.name())){AccessWatchpointRequest r=m.createAccessWatchpointRequest(f);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(r);if(n.equals("cw")){ModificationWatchpointRequest w=m.createModificationWatchpointRequest(f);w.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(w);}}
        if(n.equals("gs")){Method target=t.methodsByName("b","(III)Lcw;").get(0);BreakpointRequest r=m.createBreakpointRequest(target.locationOfCodeIndex(119));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(r);}
    }
    void run(int count)throws Exception{
        EventRequestManager m=vm.eventRequestManager();for(String n:new String[]{"PlayerSleepObservation","sn","gs","ls","fd","xa","cw"}){ClassPrepareRequest p=m.createClassPrepareRequest();p.addClassFilter(n);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();}
        for(String n:new String[]{"gs","ls","fd","cw"}){MethodEntryRequest e=m.createMethodEntryRequest();e.addClassFilter(n);e.setSuspendPolicy(EventRequest.SUSPEND_ALL);methods.add(e);MethodExitRequest x=m.createMethodExitRequest();x.addClassFilter(n);x.setSuspendPolicy(EventRequest.SUSPEND_ALL);methods.add(x);}
        ExceptionRequest fault=m.createExceptionRequest(null,true,true);fault.addClassFilter("gs");fault.setSuspendPolicy(EventRequest.SUSPEND_ALL);fault.enable();boolean alive=true;
        while(alive){EventSet set=vm.eventQueue().remove(30000);if(set==null)throw new IllegalStateException("JDI event timeout");for(Event e:set){
            if(e instanceof ClassPrepareEvent)prepared(((ClassPrepareEvent)e).referenceType());
            else if(e instanceof ModificationWatchpointEvent){ModificationWatchpointEvent w=(ModificationWatchpointEvent)e;if(w.field().declaringType().name().equals("PlayerSleepObservation"))begin(((IntegerValue)w.valueToBe()).value());else if(active>=0&&relevant(w.location()))event(map("kind","STATIC_WRITE","site",site(w.location()),"field",w.field().name(),"before",semantic(w.valueCurrent()),"value",semantic(w.valueToBe()),"statics_before",enumState()));}
            else if(e instanceof AccessWatchpointEvent&&active>=0){AccessWatchpointEvent w=(AccessWatchpointEvent)e;if(relevant(w.location()))event(map("kind","FIELD_READ","site",site(w.location()),"owner",w.field().declaringType().name(),"field",w.field().name(),"value",semantic(w.valueCurrent())));}
            else if(e instanceof MethodEntryEvent&&active>=0){MethodEntryEvent x=(MethodEntryEvent)e;if(relevant(x.location())){pin(x.method());event(map("kind","ENTER","site",site(x.location()),"arguments",x.thread().frame(0).getArgumentValues().stream().map(PlayerSleepJdi::raw).toList()));}}
            else if(e instanceof MethodExitEvent&&active>=0){MethodExitEvent x=(MethodExitEvent)e;if(relevant(x.location()))event(map("kind","RETURN","site",site(x.location()),"value",semantic(x.returnValue())));}
            else if(e instanceof BreakpointEvent&&active>=0){BreakpointEvent b=(BreakpointEvent)e;if(frontier)throw new IllegalStateException("duplicate frontier");frontierState=state();frontierEnum=enumState();event(map("kind","FRONTIER119","site",site(b.location()),"preserved",initial.equals(frontierState),"cw_initialized",cw.isInitialized()));frontier=true;}
            else if(e instanceof ExceptionEvent&&active>=0){ExceptionEvent x=(ExceptionEvent)e;event(map("kind","EXCEPTION","site",site(x.location()),"class",x.exception().referenceType().name().replace('.','/')));}
            else if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException done){alive=false;}}
        if(rows!=count||active!=-1)throw new IllegalStateException("complete declared rows "+rows);
    }
    public static void main(String[] a)throws Exception{
        if(a.length!=10)throw new IllegalArgumentException("jar work profiles names bindings sdk output jdk helpercp count");Path output=Path.of(a[6]).toAbsolutePath();Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();StringJoiner main=new StringJoiner(" ");main.add("PlayerSleepObservation");for(int i=0;i<6;++i)main.add("\""+a[i]+"\"");options.get("main").setValue(main.toString());options.get("home").setValue(a[7]);options.get("options").setValue("-Xint -Xverify:all -Xmx512m -XX:MaxMetaspaceSize=512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath \""+a[8]+"\"");options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options);Process p=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();Thread stdout=drain(p.getInputStream(),output.resolve("target.stdout"),p,failure),stderr=drain(p.getErrorStream(),output.resolve("target.stderr"),p,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter w=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8))){new PlayerSleepJdi(vm,w).run(Integer.parseInt(a[9]));done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}p.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        p.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||p.exitValue()!=0)throw new IllegalStateException("target/drain failure",failure.get());System.out.println("Read-only Player sleep JDI: "+a[9]+" profiles");
    }
    static Thread drain(InputStream stream,Path path,Process p,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];long n=0;for(int c;(c=stream.read(b))>=0;){n+=c;if(n>8L*1024*1024)throw new IOException("stream bound");out.write(b,0,c);}}catch(Throwable e){failure.compareAndSet(null,e);p.destroyForcibly();}});}
}
