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

/** Read-only live Code, full root returns, constructor entropy and reusable FW observations. */
public final class NightmareJdi {
    final VirtualMachine vm;
    final PrintWriter out;
    final List<EventRequest> requests=new ArrayList<>(),clocks=new ArrayList<>();
    final Set<String> pinned=new HashSet<>();
    final Map<Long,String> identities=new HashMap<>();
    final List<ObjectReference> held=new ArrayList<>();
    final List<Object> events=new ArrayList<>(),constructors=new ArrayList<>(),searches=new ArrayList<>();
    final Map<String,Integer> counters=new LinkedHashMap<>();
    ReferenceType helper;
    ObjectReference world,finder,lastConstructed,candidate,path;
    int active=-1,completed,sequence,fresh,constructorDepth;
    boolean returned;
    Object result;
    long bytes;
    List<Object> constructorEntropy;
    Object constructorMathBefore;

    NightmareJdi(VirtualMachine vm,PrintWriter out){this.vm=vm;this.out=out;}
    static Map<String,Object> map(Object... values){return NightmareObservation.map(values);}
    static Object site(Location where){return map("class",where.declaringType().name(),"method",where.method().name(),"descriptor",where.method().signature(),"pc",where.codeIndex());}
    void require(boolean condition,String message){if(!condition)throw new IllegalStateException(message);}
    void emit(Object value){String text=NightmareObservation.json(value);bytes+=text.length()+1;require(bytes<=128L*1024*1024,"bounded JDI journal");out.println(text);out.flush();require(!out.checkError(),"complete journal write");}
    void event(Object value){require(events.size()<32768,"bounded passive phase events");events.add(map("sequence",sequence++,"value",value));}
    void count(String name){counters.put(name,counters.getOrDefault(name,0)+1);}
    static Value field(ObjectReference receiver,String owner,String name){if(receiver==null)return null;for(Field f:receiver.referenceType().allFields())if(f.declaringType().name().equals(owner)&&f.name().equals(name))return receiver.getValue(f);throw new IllegalStateException("field "+owner+"."+name);}
    Value stat(String owner,String name){List<ReferenceType> types=vm.classesByName(owner);if(types.isEmpty()||!types.get(0).isPrepared())return null;return types.get(0).getValue(types.get(0).fieldByName(name));}
    ObjectReference object(Value value){return value instanceof ObjectReference?(ObjectReference)value:null;}
    String identity(ObjectReference value){if(value==null)return null;return identities.computeIfAbsent(value.uniqueID(),key->"reference-"+fresh++);}
    void root(ObjectReference value,String name){if(value==null)return;identities.putIfAbsent(value.uniqueID(),name);value.disableCollection();held.add(value);}
    Object value(Value input){
        if(input==null||input instanceof VoidValue)return null;
        if(input instanceof BooleanValue)return((BooleanValue)input).value();
        if(input instanceof ByteValue)return(int)((ByteValue)input).value();if(input instanceof ShortValue)return(int)((ShortValue)input).value();if(input instanceof IntegerValue)return((IntegerValue)input).value();
        if(input instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)input).value()));
        if(input instanceof FloatValue)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value())));
        if(input instanceof DoubleValue)return map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value())));
        if(input instanceof StringReference){List<Integer> units=new ArrayList<>();for(char c:((StringReference)input).value().toCharArray())units.add((int)c);return map("utf16",units);}
        ObjectReference reference=(ObjectReference)input;return map("class",reference.referenceType().name(),"identity",identity(reference));
    }
    Object fields(ObjectReference receiver){if(receiver==null)return null;List<Field> fs=new ArrayList<>();for(Field f:receiver.referenceType().allFields())if(!f.isStatic())fs.add(f);fs.sort(Comparator.comparing(f->f.declaringType().name()+"."+f.name()));Map<Field,Value> captured=receiver.getValues(fs);Map<String,Object> result=new LinkedHashMap<>();for(Field f:fs)result.put(f.declaringType().name()+"."+f.name(),value(captured.get(f)));return result;}
    Object random(ObjectReference receiver){if(receiver==null)return null;ObjectReference seed=object(field(receiver,"java.util.Random","seed"));return List.of(String.format(Locale.ROOT,"%016x",((LongValue)field(seed,"java.util.concurrent.atomic.AtomicLong","value")).value()),value(field(receiver,"java.util.Random","haveNextNextGaussian")),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)field(receiver,"java.util.Random","nextNextGaussian")).value())));}
    Object shared(){return random(object(stat("java.lang.Math$RandomNumberGeneratorHolder","randomNumberGenerator")));}
    Object entity(ObjectReference receiver){return receiver==null?null:map("identity",identity(receiver),"class",receiver.referenceType().name(),"fields",fields(receiver),"bounds",fields(object(field(receiver,"sn","aW"))),"random",random(object(field(receiver,"sn","bs"))));}
    Object array(ArrayReference array){if(array==null)return null;List<Object> result=new ArrayList<>();for(Value v:array.getValues())result.add(v==null?null:map("identity",identity((ObjectReference)v)));return map("identity",identity(array),"component",array.referenceType().name(),"length",array.length(),"values",result);}
    Object finder(){
        if(finder==null)return null;
        ObjectReference heap=object(field(finder,"fw","b")),table=object(field(finder,"fw","c"));
        ArrayReference heapArray=heap==null?null:(ArrayReference)field(heap,"lf","a"),mapArray=table==null?null:(ArrayReference)field(table,"jx","a"),neighbors=(ArrayReference)field(finder,"fw","d");
        List<Object> pathPoints=new ArrayList<>();if(path!=null){ArrayReference points=(ArrayReference)field(path,"dh","b");for(Value p:points.getValues())pathPoints.add(fields(object(p)));}
        return map("identity",identity(finder),"XP",value(field(finder,"fw","a")),"heap",fields(heap),"heap_array",array(heapArray),"map",fields(table),"map_array",array(mapArray),"neighbors",array(neighbors),"path",fields(path),"path_points",pathPoints);
    }
    void pin(Method method)throws Exception{String key=method.declaringType().name()+"."+method.name()+method.signature();if(!pinned.add(key))return;byte[] code=method.bytecodes();emit(map("event","code","class",method.declaringType().name(),"method",method.name(),"descriptor",method.signature(),"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));}
    void breakpoint(ReferenceType type,String name,String descriptor,long... pcs)throws Exception{List<Method> found=type.methodsByName(name,descriptor);require(found.size()==1,"unique method "+type.name()+name+descriptor);Method method=found.get(0);pin(method);for(long pc:pcs){require(pc>=0&&pc<method.bytecodes().length,"source BCI bound");BreakpointRequest request=vm.eventRequestManager().createBreakpointRequest(method.locationOfCodeIndex(pc));request.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(request);if(active>=0)request.enable();}}
    void prepare(ReferenceType type)throws Exception{
        String name=type.name();EventRequestManager manager=vm.eventRequestManager();
        if(name.equals("NightmareObservation")){helper=type;ModificationWatchpointRequest request=manager.createModificationWatchpointRequest(type.fieldByName("active"));request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();return;}
        if(name.equals("cq")){
            breakpoint(type,"a","(Lfd;Ljava/util/List;)Z",41,78,92,102,125,135,155,165,200,322,367,382,392,403,415,429,502,533,588,591,595,625,630,633,635,638,643,652);
            breakpoint(type,"a","(Lls;Lfd;FFF)V",13);
            MethodEntryRequest enter=manager.createMethodEntryRequest();enter.addClassFilter("cq");enter.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(enter);if(active>=0)enter.enable();
            MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter("cq");exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exit);if(active>=0)exit.enable();
        }
        if(name.equals("fw")){breakpoint(type,"<init>","(Lxp;)V",0,40);breakpoint(type,"a","(Lsn;Lsn;F)Ldh;",0,21);breakpoint(type,"a","(Lsn;DDDF)Ldh;",4,11,14);}
        if(name.equals("dh"))breakpoint(type,"<init>","([Ld;)V",15);
        if(Set.of("cn","uz","fr").contains(name)){Method method=type.methodsByName("<init>","(Lfd;)V").get(0);breakpoint(type,"<init>","(Lfd;)V",0,method.bytecodes().length-1);}
    }
    void begin(int next)throws Exception{
        if(active>=0){require(constructorDepth==0,"constructor not left active");emit(map("event","call","case",active,"returned",returned,"result",result,"counters",new LinkedHashMap<>(counters),"constructors",new ArrayList<>(constructors),"searches",new ArrayList<>(searches),"finder",finder(),"candidate",entity(candidate),"events",new ArrayList<>(events)));++completed;}
        for(ObjectReference value:held)try{value.enableCollection();}catch(ObjectCollectedException ignored){}held.clear();
        active=next;sequence=0;fresh=0;constructorDepth=0;identities.clear();events.clear();constructors.clear();searches.clear();counters.clear();finder=null;lastConstructed=null;candidate=null;path=null;returned=false;result=null;
        world=next<0?null:object(helper.getValue(helper.fieldByName("world")));if(world!=null)root(world,"world");
        for(EventRequest request:requests)request.setEnabled(next>=0);for(EventRequest request:clocks)request.disable();
    }
    void at(BreakpointEvent event)throws Exception{
        Location where=event.location();String owner=where.declaringType().name(),descriptor=where.method().signature();int pc=(int)where.codeIndex();ObjectReference self=event.thread().frame(0).thisObject();
        if(Set.of("cn","uz","fr").contains(owner)&&where.method().name().equals("<init>")){
            if(pc==0){++constructorDepth;constructorEntropy=new ArrayList<>();constructorMathBefore=shared();for(EventRequest request:clocks)request.enable();}
            else{lastConstructed=self;root(self,"constructor"+constructors.size());constructors.add(map("class",owner,"entity",entity(self),"math_before",constructorMathBefore,"math_after",shared(),"entropy",constructorEntropy));--constructorDepth;for(EventRequest request:clocks)request.setEnabled(constructorDepth>0);}
            return;
        }
        if(owner.equals("fw")){
            if(where.method().name().equals("<init>")){finder=self;root(self,"finder");if(pc==40){count("finder_constructors");event(map("kind","FW_CONSTRUCTED","site",site(where),"finder",finder()));}return;}
            if(descriptor.equals("(Lsn;Lsn;F)Ldh;")){
                if(pc==0){count("paths");path=null;event(map("kind","FW_ENTER","site",site(where),"finder",finder()));}
                else{Object snapshot=finder();searches.add(snapshot);event(map("kind","FW_RETURN","site",site(where),"finder",snapshot));}return;
            }
            event(map("kind","FW_CLEAR","site",site(where),"finder",finder()));return;
        }
        if(owner.equals("dh")){path=self;root(self,"path"+searches.size());return;}
        if(owner.equals("cq")&&descriptor.equals("(Lls;Lfd;FFF)V")){count("random_calls");event(map("kind","POSTSPAWN_DRAW","site",site(where),"world_random",random(object(field(world,"fd","r")))));return;}
        if(owner.equals("cq")){
            if(pc==41)count("players_started");
            if(pc==78)count("attempts_started");
            if(Set.of(92,102,125,135,155,165,200,382).contains(pc))count("random_calls");
            if(pc==367){candidate=lastConstructed;count("constructors");}
            if(pc==392)count("predicates");
            if(pc==591)count("publications");
            if(pc==595)count("publication_returns");
            if(pc==625)count("postspawn_returns");
            if(pc==630)count("wakes");
            if(pc==633)count("wake_returns");
            if(pc==635)count("ambient");
            if(pc==638)count("ambient_returns");
            if(pc==643)count("attempts_returned");
            if(Set.of(322,367,392,403,415,429,502,533,588,591,595,625,633,635,638,643,652).contains(pc))event(map("kind","CQ_PHASE","site",site(where),"world_random",random(object(field(world,"fd","r"))),"candidate",entity(candidate)));
        }
    }
    boolean rootMethod(Method method){return method.declaringType().name().equals("cq")&&method.name().equals("a")&&method.signature().equals("(Lfd;Ljava/util/List;)Z");}
    void run(int expected)throws Exception{
        EventRequestManager manager=vm.eventRequestManager();for(String name:new String[]{"NightmareObservation","cq","fw","dh","cn","uz","fr"}){ClassPrepareRequest request=manager.createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
        ExceptionRequest exceptions=manager.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exceptions);
        for(String name:new String[]{"java.util.Random","java.lang.System"}){MethodExitRequest request=manager.createMethodExitRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);clocks.add(request);}
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(60000);require(set!=null,"live event deadline");for(Event event:set){
            if(event instanceof ClassPrepareEvent)prepare(((ClassPrepareEvent)event).referenceType());
            else if(event instanceof ModificationWatchpointEvent){ModificationWatchpointEvent changed=(ModificationWatchpointEvent)event;begin(((IntegerValue)changed.valueToBe()).value());}
            else if(active>=0&&event instanceof BreakpointEvent)at((BreakpointEvent)event);
            else if(active>=0&&event instanceof MethodEntryEvent){MethodEntryEvent entered=(MethodEntryEvent)event;if(rootMethod(entered.method())){pin(entered.method());count("root_calls");List<Object> arguments=new ArrayList<>();for(Value v:entered.thread().frame(0).getArgumentValues())arguments.add(value(v));event(map("kind","ROOT_ENTER","site",site(entered.location()),"arguments",arguments));}else if(entered.method().signature().equals("(Llk;Lfd;III)Z"))count("position_queries");else if(entered.method().signature().equals("(Lls;Lfd;FFF)V"))count("postspawn");}
            else if(active>=0&&event instanceof MethodExitEvent){MethodExitEvent exited=(MethodExitEvent)event;if(rootMethod(exited.method())){returned=true;result=value(exited.returnValue());count("root_returns");event(map("kind","ROOT_RETURN","site",site(exited.location()),"result",result));}else if(constructorDepth>0&&((exited.method().declaringType().name().equals("java.util.Random")&&exited.method().name().equals("seedUniquifier"))||(exited.method().declaringType().name().equals("java.lang.System")&&exited.method().name().equals("nanoTime"))))constructorEntropy.add(map("method",exited.method().declaringType().name()+"."+exited.method().name(),"value",value(exited.returnValue())));}
            else if(active>=0&&event instanceof ExceptionEvent){ExceptionEvent failure=(ExceptionEvent)event;event(map("kind","EXCEPTION","site",site(failure.location()),"class",failure.exception().referenceType().name().replace('.','/'),"message",value(field(failure.exception(),"java.lang.Throwable","detailMessage")),"catch",failure.catchLocation()==null?null:site(failure.catchLocation())));}
            else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException ended){alive=false;}}
        require(completed==expected&&active==-1,"whole completed target call inventory");
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=7)throw new IllegalArgumentException("jdk classpath archive profiles classnames output count");Path output=Path.of(args[5]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();String q="\"";
        options.get("home").setValue(args[0]);options.get("main").setValue("NightmareObservation "+q+args[2]+q+" "+q+args[3]+q+" "+q+args[4]+q);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+q+args[1]+q);options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options);Process process=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();Thread stdout=drain(process.getInputStream(),output.resolve("target.stdout"),process,failure),stderr=drain(process.getErrorStream(),output.resolve("target.stderr"),process,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8))){new NightmareJdi(vm,writer).run(Integer.parseInt(args[6]));done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}process.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        process.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||process.exitValue()!=0)throw new IllegalStateException("complete original target/drain",failure.get());System.out.println("Read-only whole nightmare JDI: "+args[6]+" calls");
    }
    static Thread drain(InputStream input,Path path,Process process,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream output=Files.newOutputStream(path)){byte[] bytes=new byte[8192];long total=0;for(int count;(count=input.read(bytes))>=0;){total+=count;if(total>8L*1024*1024)throw new IOException("bounded target stream");output.write(bytes,0,count);}}catch(Throwable error){failure.compareAndSet(null,error);process.destroyForcibly();}});}
}
