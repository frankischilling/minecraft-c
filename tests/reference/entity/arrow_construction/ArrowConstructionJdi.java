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

/** Passive observations of actual original receivers, Code and source events. */
public final class ArrowConstructionJdi {
    final VirtualMachine vm;
    final PrintWriter out;
    final List<EventRequest> requests=new ArrayList<>();
    final List<Object> events=new ArrayList<>();
    final Set<String> pinned=new HashSet<>();
    final Map<Long,Deque<ObjectReference>> gaussian=new HashMap<>();
    ReferenceType helper;
    ObjectReference arrow;
    Object entryArrow,exceptionArrow,returnArrow;
    boolean mathBefore;
    int active=-1,completed,sequence;
    long bytes;
    static final Map<String,Set<String>> SELECTED=Map.ofEntries(
        Map.entry("sl",Set.of("<init>(Lfd;)V","<init>(Lfd;DDD)V","<init>(Lfd;Lls;)V","a(DDDFF)V","b()V")),
        Map.entry("sn",Set.of("<init>(Lfd;)V","b(FF)V","e(DDD)V","c(DDDFF)V")),
        Map.entry("ls",Set.of("w()F")),Map.entry("gs",Set.of("w()F")),Map.entry("gi",Set.of("w()F")),
        Map.entry("in",Set.of("<clinit>()V","a(D)F","a(F)F","b(F)F")),
        Map.entry("eq",Set.of("a(DDDDDD)Leq;","c(DDDDDD)Leq;")),
        Map.entry("ud",Set.of("<init>()V","a(ILjava/lang/Object;)V")),
        Map.entry("java.util.Random",Set.of("nextGaussian()D")));
    ArrowConstructionJdi(VirtualMachine vm,PrintWriter out){this.vm=vm;this.out=out;}
    static Map<String,Object> map(Object... words){return ConstructorJson.map(words);}
    static void need(boolean value,String message){if(!value)throw new IllegalStateException(message);}
    static Object site(Location value){return map("class",value.declaringType().name(),"method",value.method().name(),"descriptor",value.method().signature(),"pc",value.codeIndex());}
    static Field find(ObjectReference value,String owner,String name){for(Field f:value.referenceType().allFields())if(f.declaringType().name().equals(owner)&&f.name().equals(name))return f;return null;}
    static Value field(ObjectReference value,String owner,String name){if(value==null)return null;Field f=find(value,owner,name);need(f!=null,"actual field "+owner+"."+name);return value.getValue(f);}
    ObjectReference ref(String name){return(ObjectReference)helper.getValue(helper.fieldByName(name));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    Object random(ObjectReference value){return value==null?null:List.of(String.format(Locale.ROOT,"%016x",((LongValue)field((ObjectReference)field(value,"java.util.Random","seed"),"java.util.concurrent.atomic.AtomicLong","value")).value()),
        ((BooleanValue)field(value,"java.util.Random","haveNextNextGaussian")).value(),d(((DoubleValue)field(value,"java.util.Random","nextNextGaussian")).value()));}
    Object scalar(Value value){
        if(value==null||value instanceof VoidValue)return null;
        if(value instanceof BooleanValue)return((BooleanValue)value).value();
        if(value instanceof ByteValue)return(int)((ByteValue)value).value();if(value instanceof ShortValue)return(int)((ShortValue)value).value();
        if(value instanceof IntegerValue)return((IntegerValue)value).value();
        if(value instanceof FloatValue)return map("F",f(((FloatValue)value).value()));if(value instanceof DoubleValue)return map("D",d(((DoubleValue)value).value()));
        if(value instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)value).value()));
        if(value instanceof StringReference){List<Integer> units=new ArrayList<>();for(char c:((StringReference)value).value().toCharArray())units.add((int)c);return map("utf16",units);}
        ObjectReference object=(ObjectReference)value;return map("class",object.referenceType().name(),"same_arrow",same(object,arrow),
            "same_shooter",same(object,ref("shooter")),"same_world_a",same(object,ref("worldA")),"same_world_b",same(object,ref("worldB")));
    }
    static boolean same(ObjectReference a,ObjectReference b){return a!=null&&b!=null&&a.uniqueID()==b.uniqueID();}
    Object payload(ObjectReference value){if(value==null)return map("kind","Null","value",null);String name=value.referenceType().name();
        if(name.equals("java.lang.Byte"))return map("kind","Byte","value",((ByteValue)field(value,name,"value")).value());
        if(name.equals("java.lang.Integer"))return map("kind","Integer","value",((IntegerValue)field(value,name,"value")).value());
        if(value instanceof StringReference){List<Integer> units=new ArrayList<>();for(char c:((StringReference)value).value().toCharArray())units.add((int)c);return map("kind","String","value",units);}
        throw new IllegalStateException("actual unexpected watcher payload "+name);
    }
    Object watcher(ObjectReference value){if(value==null)return null;ObjectReference hash=(ObjectReference)field(value,"ud","b");TreeMap<Integer,Object> records=new TreeMap<>();
        if(hash!=null){ArrayReference table=(ArrayReference)field(hash,"java.util.HashMap","table");if(table!=null)for(Value head:table.getValues()){
            ObjectReference node=(ObjectReference)head;int traversed=0;while(node!=null){need(++traversed<=256,"bounded genuine HashMap chain");
                ObjectReference key=(ObjectReference)field(node,"java.util.HashMap$Node","key"),record=(ObjectReference)field(node,"java.util.HashMap$Node","value");
                int index=((IntegerValue)field(key,"java.lang.Integer","value")).value();need(record!=null&&!records.containsKey(index),"actual unique watcher entry");
                records.put(index,map("key",index,"type",((IntegerValue)field(record,"ma","a")).value(),"id",((IntegerValue)field(record,"ma","b")).value(),
                    "value",payload((ObjectReference)field(record,"ma","c")),"dirty",((BooleanValue)field(record,"ma","d")).value()));
                node=(ObjectReference)field(node,"java.util.HashMap$Node","next");}}}
        return map("map_null",hash==null,"dirty",((BooleanValue)field(value,"ud","c")).value(),"records",new ArrayList<>(records.values()));
    }
    List<String> doubles(ObjectReference value,String... names){List<String> result=new ArrayList<>();for(String name:names)result.add(d(((DoubleValue)field(value,"sn",name)).value()));return result;}
    List<String> floats(ObjectReference value,String... names){List<String> result=new ArrayList<>();for(String name:names)result.add(f(((FloatValue)field(value,"sn",name)).value()));return result;}
    Object snapshot(ObjectReference value){if(value==null)return null;Object own=null;if(value.referenceType().name().equals("sl")){
        Map<String,Object> fields=new LinkedHashMap<>();for(String name:new String[]{"a","b","c","d","e","f","g","h","i","j","k"}){
            Value word=field(value,"sl",name);fields.put(name,name.equals("c")?(word==null?null:same((ObjectReference)word,ref("shooter"))?"shooter":"UNDECLARED_SHOOTER"):scalar(word));}own=fields;}
        ObjectReference world=(ObjectReference)field(value,"sn","aI"),bounds=(ObjectReference)field(value,"sn","aW");List<String> box=null;
        if(bounds!=null){box=new ArrayList<>();for(String name:new String[]{"a","b","c","d","e","f"})box.add(d(((DoubleValue)field(bounds,"eq",name)).value()));}
        return map("class",value.referenceType().name(),"id",((IntegerValue)field(value,"sn","aD")).value(),"world",world==null?null:same(world,ref("worldA"))?"world-a":same(world,ref("worldB"))?"world-b":"UNDECLARED_WORLD",
            "arrow",own,"position",doubles(value,"aM","aN","aO"),"motion",doubles(value,"aP","aQ","aR"),"history",doubles(value,"aJ","aK","aL","bl","bm","bn"),
            "angles",floats(value,"aS","aT","aU","aV"),"dimensions",floats(value,"bg","bh","bf"),"bounds",box,
            "dead",((BooleanValue)field(value,"sn","be")).value(),"random",random((ObjectReference)field(value,"sn","bs")),"watcher",watcher((ObjectReference)field(value,"sn","bD")));
    }
    boolean mathInitialized(){List<ReferenceType> types=vm.classesByName("in");return !types.isEmpty()&&types.get(0).isInitialized();}
    void emit(Object value){String line=ConstructorJson.json(value);bytes+=line.length()+1;need(bytes<=32L*1024*1024,"bounded source journal");out.println(line);out.flush();need(!out.checkError(),"complete source journal");}
    void event(Object value){need(events.size()<32768,"bounded active event inventory");events.add(map("sequence",sequence++,"value",value));}
    boolean selected(Method method){return SELECTED.getOrDefault(method.declaringType().name(),Set.of()).contains(method.name()+method.signature());}
    void pin(Method method)throws Exception{String key=method.declaringType().name()+"."+method.name()+method.signature();if(!pinned.add(key)||method.declaringType().name().startsWith("java."))return;
        byte[] code=method.bytecodes();emit(map("event","code","class",method.declaringType().name(),"method",method.name(),"descriptor",method.signature(),"bytes",code.length,
            "sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));}
    void begin(int next){
        if(active>=0){emit(map("event","call","case",active,"entry_arrow",entryArrow,"exception_arrow",exceptionArrow,"return_arrow",returnArrow,
            "math_initialized_before",mathBefore,"math_initialized_after",mathInitialized(),"events",new ArrayList<>(events)));++completed;}
        if(arrow!=null){arrow.enableCollection();arrow=null;}active=next;entryArrow=exceptionArrow=returnArrow=null;events.clear();gaussian.clear();sequence=0;
        mathBefore=mathInitialized();for(EventRequest request:requests)request.setEnabled(next>=0);
    }
    void addWatchers(ReferenceType type){EventRequestManager manager=vm.eventRequestManager();for(Field field:type.fields()){
        if(field.isStatic())continue;if(type.name().equals("sn")&&!Set.of("aI","aM","aN","aO","aP","aQ","aR","aJ","aK","aL","bl","bm","bn","aS","aT","aU","aV","bg","bh","bf","aW","bs","bD").contains(field.name()))continue;
        ModificationWatchpointRequest request=manager.createModificationWatchpointRequest(field);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(request);request.setEnabled(active>=0);}}
    void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();for(String name:new String[]{"ArrowConstructionObservation","sl","sn"}){ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter(name);prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();}
        for(String name:SELECTED.keySet()){MethodEntryRequest entry=manager.createMethodEntryRequest();entry.addClassFilter(name);entry.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(entry);
            MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exit);}
        ExceptionRequest exceptions=manager.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exceptions);
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(60000);need(set!=null,"live source event deadline");for(Event value:set){
            if(value instanceof ClassPrepareEvent){ReferenceType type=((ClassPrepareEvent)value).referenceType();if(type.name().equals("ArrowConstructionObservation")){
                helper=type;ModificationWatchpointRequest request=manager.createModificationWatchpointRequest(type.fieldByName("active"));request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}else addWatchers(type);}
            else if(value instanceof ModificationWatchpointEvent){ModificationWatchpointEvent write=(ModificationWatchpointEvent)value;if(write.field().declaringType().name().equals("ArrowConstructionObservation"))begin(((IntegerValue)write.valueToBe()).value());
                else if(active>=0)event(map("kind","STORE","site",site(write.location()),"receiver",scalar(write.object()),"field",write.field().declaringType().name()+"."+write.field().name(),"value",scalar(write.valueToBe())));}
            else if(active>=0&&value instanceof MethodEntryEvent){MethodEntryEvent entry=(MethodEntryEvent)value;if(!selected(entry.method()))continue;pin(entry.method());StackFrame frame=entry.thread().frame(0);
                boolean root=entry.method().declaringType().name().equals("sl")&&(entry.method().name().equals("<init>")||entry.method().name().equals("a"));
                if(root&&arrow==null){arrow=frame.thisObject();need(arrow!=null,"actual Arrow receiver");arrow.disableCollection();entryArrow=snapshot(arrow);}
                List<Object> args=new ArrayList<>();for(Value argument:frame.getArgumentValues())args.add(scalar(argument));Object randomBefore=null;
                if(entry.method().declaringType().name().equals("java.util.Random")){ObjectReference receiver=frame.thisObject();gaussian.computeIfAbsent(entry.thread().uniqueID(),key->new ArrayDeque<>()).push(receiver);randomBefore=random(receiver);}
                event(map("kind","ENTRY","site",site(entry.location()),"receiver",scalar(frame.thisObject()),"arguments",args,"random",randomBefore));}
            else if(active>=0&&value instanceof MethodExitEvent){MethodExitEvent exit=(MethodExitEvent)value;if(!selected(exit.method()))continue;Object randomAfter=null;
                if(exit.method().declaringType().name().equals("java.util.Random")){Deque<ObjectReference> stack=gaussian.get(exit.thread().uniqueID());need(stack!=null&&!stack.isEmpty(),"actual Gaussian return receiver");randomAfter=random(stack.pop());}
                if(exit.method().declaringType().name().equals("sl")&&(exit.method().name().equals("<init>")||exit.method().name().equals("a")))returnArrow=snapshot(arrow);
                event(map("kind","RETURN","site",site(exit.location()),"result",scalar(exit.returnValue()),"random",randomAfter));}
            else if(active>=0&&value instanceof ExceptionEvent){ExceptionEvent failure=(ExceptionEvent)value;if(exceptionArrow==null)exceptionArrow=snapshot(arrow);
                event(map("kind","EXCEPTION","site",site(failure.location()),"class",failure.exception().referenceType().name().replace('.','/'),"message",scalar(field(failure.exception(),"java.lang.Throwable","detailMessage")),"catch",failure.catchLocation()==null?null:site(failure.catchLocation())));}
            else if(value instanceof VMDeathEvent||value instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException ended){alive=false;}}
        need(completed==40&&active==-1,"all complete original calls");
    }
    static Thread drain(InputStream input,Path output,Process target,AtomicReference<Throwable> failure){return new Thread(()->{
        try(OutputStream stream=Files.newOutputStream(output)){long bytes=0;byte[] block=new byte[8192];for(int count;(count=input.read(block))>=0;){bytes+=count;need(bytes<=16L*1024*1024,"bounded source stream");stream.write(block,0,count);}}
        catch(Throwable error){failure.compareAndSet(null,error);target.destroyForcibly();}});}
    public static void main(String[] args)throws Exception{
        need(args.length==7,"jdk classpath jar work profiles classes output");Path output=Path.of(args[6]);LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();String quote="\"";options.get("home").setValue(args[0]);
        options.get("main").setValue("ArrowConstructionObservation "+quote+args[2]+quote+" "+quote+args[3]+quote+" "+quote+args[4]+quote+" "+quote+args[5]+quote);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+quote+args[1]+quote);
        options.get("suspend").setValue("true");VirtualMachine vm=connector.launch(options);Process target=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread stdout=drain(target.getInputStream(),output.resolve("target.stdout"),target,failure),stderr=drain(target.getErrorStream(),output.resolve("target.stderr"),target,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8))){new ArrowConstructionJdi(vm,writer).run();done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}target.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        target.waitFor();stdout.join(5000);stderr.join(5000);need(!stdout.isAlive()&&!stderr.isAlive()&&failure.get()==null&&target.exitValue()==0,"complete target and drains");
        System.out.println("Read-only original Arrow construction: 40 complete calls");
    }
}
