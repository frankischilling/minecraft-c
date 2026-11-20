import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only original draw/pose/buffer observer; no invocation or target write. */
public final class InventoryDrawingJdi {
    static final Set<String> WATCH=Set.of("ue.a(IIF)V","id.a(IIF)V","ue.a(F)V","th.a(Lsn;DDDFF)V",
        "ds.a(Lsn;DDDFF)V","ds.a(Lgs;DDDFF)V","gv.a(Lls;DDDFF)V","bw.b(Lsn;DDDFF)V",
        "fh.a(FFFFFF)V","ps.a(F)V","ps.d(F)V","dc.t()Z");
    static final Set<Integer> PHASES=Set.of(0,347,350,361,397,400,403,408);
    static final Set<String> CLASSES=Set.of("ue","id","th","ds","gv","bw","fh","ps","nw","dc");
    static final Map<String,Object> codes=new TreeMap<>();
    static final Map<String,List<String>> prior=new HashMap<>();
    static final Map<String,Integer> entryCounts=new TreeMap<>(),returnCounts=new TreeMap<>();
    static final Set<String> partReturns=new HashSet<>(),compileReturns=new HashSet<>();
    static int selectedDraws,selectedReturns,otherDraws,exceptions;
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> values=vm.classesByName(name);return values.isEmpty()?null:values.get(0);}
    static ObjectReference root(VirtualMachine vm,String name){ReferenceType helper=type(vm,"InventoryDrawingTarget");return (ObjectReference)helper.getValue(helper.fieldByName(name));}
    static int active(VirtualMachine vm){ReferenceType helper=type(vm,"InventoryDrawingTarget");return helper==null?-1:((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();}
    static Field field(ReferenceType owner,String name){Field value=owner.fieldByName(name);require(value!=null,"actual field "+owner.name()+"."+name);return value;}
    static Value get(ObjectReference owner,String name){return owner.getValue(field(owner.referenceType(),name));}
    static String key(Method method){return method.declaringType().name()+"."+method.name()+method.signature();}
    static Object value(Value input){
        if(input==null)return null;if(input instanceof BooleanValue)return ((BooleanValue)input).value();
        if(input instanceof IntegerValue)return ((IntegerValue)input).value();if(input instanceof LongValue)return ((LongValue)input).value();
        if(input instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value()));
        if(input instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value()));
        if(input instanceof StringReference)return ((StringReference)input).value();
        ObjectReference object=(ObjectReference)input;return ConstructorJson.map("class",object.referenceType().name(),"id",object.uniqueID());
    }
    static Map<String,Object> where(Location location){return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex());}
    static List<Object> frames(ThreadReference thread)throws Exception {List<Object> result=new ArrayList<>();for(StackFrame frame:thread.frames())if(CLASSES.contains(frame.location().declaringType().name())||Set.of("da","ub","tz","u").contains(frame.location().declaringType().name()))result.add(where(frame.location()));return result;}
    static Map<String,Object> primitives(ObjectReference object){Map<String,Object> result=new TreeMap<>();for(Field f:object.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))result.put(f.declaringType().name()+"."+f.name(),value(object.getValue(f)));return result;}
    static Map<String,Object> pose(VirtualMachine vm){ObjectReference actor=root(vm,"player"),manager=root(vm,"manager"),screen=root(vm,"screen");Map<String,Object> result=new LinkedHashMap<>();for(String name:new String[]{"H","aS","aT","bE"})result.put(name,value(get(actor,name)));result.put("manager-i",value(get(manager,"i")));result.put("mouse-x",value(get(screen,"l")));result.put("mouse-y",value(get(screen,"m")));return result;}
    static List<String> words(ObjectReference object,int count){ArrayReference raw=(ArrayReference)get(object,"g");require(raw.length()>=count,"actual NW buffer extent");List<String> result=new ArrayList<>();for(Value item:raw.getValues(0,count))result.add(String.format(Locale.ROOT,"%08x",((IntegerValue)item).value()));return result;}
    static String kind(ThreadReference thread)throws Exception {
        boolean background=false,tile=false,gradient=false;
        for(StackFrame frame:thread.frames()){
            Method m=frame.location().method();String owner=m.declaringType().name();
            if(owner.equals("da")&&m.name().equals("a")&&m.signature().equals("(I)V"))background=true;
            if(owner.equals("ue")&&m.name().equals("a")&&m.signature().equals("(F)V"))tile=true;
            if(owner.equals("ub")&&m.name().equals("a")&&m.signature().equals("(IIIIII)V"))gradient=true;
        }
        if(background&&gradient)return "gradient";
        if(tile)for(StackFrame frame:thread.frames())if(key(frame.location().method()).equals("ub.b(IIIIII)V"))return "tile";
        return "other";
    }
    static boolean primaryPart(VirtualMachine vm,ObjectReference object){ObjectReference model=root(vm,"primary");for(String name:new String[]{"a","b","c","d","e","f","g"})if(object.equals(get(model,name)))return true;return false;}
    static void pin(Method method)throws Exception {String key=key(method);if(!codes.containsKey(key)){byte[] code=method.bytecodes();codes.put(key,ConstructorJson.map("code_bytes",code.length,"code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));}}
    static void breakpoint(EventRequestManager manager,Method method,long pc){Location location=pc==0?method.location():method.locationOfCodeIndex(pc);require(location!=null,"actual instruction boundary "+key(method)+"@"+pc);BreakpointRequest request=manager.createBreakpointRequest(location);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static void prepare(EventRequestManager manager,ReferenceType owner){
        for(Method method:owner.methods())if(WATCH.contains(key(method)))breakpoint(manager,method,0);
        if(owner.name().equals("nw"))for(Method method:owner.methods())if(Set.of("nw.b()V","nw.a()V").contains(key(method)))breakpoint(manager,method,0);
        if(owner.name().equals("ue"))for(Method method:owner.methodsByName("a","(F)V"))for(int pc:PHASES)if(pc!=0)breakpoint(manager,method,pc);
        if(owner.name().equals("gv"))for(Method method:owner.methodsByName("a","(Lls;DDDFF)V"))breakpoint(manager,method,743);
    }
    static final class Drain extends Thread {
        final InputStream input;final Path path;Throwable failure;
        Drain(InputStream input,Path path){this.input=input;this.path=path;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] bytes=new byte[8192];int count,total=0;while((count=in.read(bytes))!=-1){total+=count;require(total<=4*1048576,"bounded original output");out.write(bytes,0,count);}}catch(Throwable error){failure=error;}}
    }
    static String quote(String text){require(!text.contains("\"")&&!text.contains("\n")&&!text.contains("\r"),"literal command path");return "\""+text+"\"";}
    public static void main(String[] args)throws Exception {
        require(args.length==5,"output jdk-home natives classpath jar");Path output=Path.of(args[0]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[1]);options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+quote(args[2])+" -classpath "+quote(args[3]));
        options.get("main").setValue("InventoryDrawingTarget "+quote(args[4])+" "+quote(output.resolve("game").toString())+" "+quote(args[2]));options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"read-only live Code/return capability");EventRequestManager requests=vm.eventRequestManager();
            for(String name:CLASSES){ClassPrepareRequest request=requests.createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.enable();}
            ExceptionRequest exception=requests.createExceptionRequest(null,true,true);exception.setSuspendPolicy(EventRequest.SUSPEND_ALL);exception.enable();
            boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);
            while(!done){require(System.nanoTime()<deadline,"bounded original draw observer");EventSet events;try{events=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent){prepare(requests,((ClassPrepareEvent)event).referenceType());continue;}
                    if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){done=true;continue;}
                    int profile=active(vm);if(profile<0)continue;
                    if(event instanceof BreakpointEvent){BreakpointEvent item=(BreakpointEvent)event;Method method=item.location().method();String methodKey=key(method);long pc=item.location().codeIndex();pin(method);
                        if(methodKey.equals("gv.a(Lls;DDDFF)V")&&pc==743){++exceptions;journal.println(ConstructorJson.json(ConstructorJson.map("event","gv-caught-exception","profile",profile,"source",where(item.location()),"frames",frames(item.thread()))));continue;}
                        if(methodKey.equals("ue.a(F)V")&&PHASES.contains((int)pc))journal.println(ConstructorJson.json(ConstructorJson.map("event","ue-phase","profile",profile,"pc",pc,"pose",pose(vm),"frames",frames(item.thread()))));
                        if(WATCH.contains(methodKey)&&pc==0){ObjectReference receiver=item.thread().frame(0).thisObject();entryCounts.merge(profile+":"+methodKey,1,Integer::sum);List<Object> arguments=new ArrayList<>();for(Value argument:item.thread().frame(0).getArgumentValues())arguments.add(value(argument));journal.println(ConstructorJson.json(ConstructorJson.map("event","entry","profile",profile,"method",methodKey,"receiver",receiver.uniqueID(),"arguments",arguments,"primary-part",methodKey.startsWith("ps.")&&primaryPart(vm,receiver),"frames",frames(item.thread()))));}
                        if(methodKey.equals("nw.b()V")||methodKey.equals("nw.a()V")){
                            String kind=kind(item.thread());if(kind.equals("other")){if(methodKey.equals("nw.a()V"))++otherDraws;continue;}
                            ObjectReference receiver=item.thread().frame(0).thisObject();ReferenceType owner=receiver.referenceType();require(receiver.equals(owner.getValue(owner.fieldByName("a"))),"actual singleton NW receiver");String tag=profile+":"+kind;
                            if(methodKey.equals("nw.b()V")){require(!prior.containsKey(tag),"one selected begin per kind/profile");prior.put(tag,words(receiver,48));journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-begin","profile",profile,"kind",kind,"state",primitives(receiver),"words",prior.get(tag),"frames",frames(item.thread()))));}
                            else{int count=((IntegerValue)get(receiver,"o")).value();require(count==48&&prior.containsKey(tag),"actual selected quad/default triangle word count");++selectedDraws;journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-draw","profile",profile,"kind",kind,"state",primitives(receiver),"words",words(receiver,count),"prior",prior.get(tag),"frames",frames(item.thread()))));}
                        }
                    }else if(event instanceof MethodExitEvent){MethodExitEvent item=(MethodExitEvent)event;String methodKey=key(item.method());
                        if(WATCH.contains(methodKey)){ObjectReference receiver=item.thread().frame(0).thisObject();returnCounts.merge(profile+":"+methodKey,1,Integer::sum);boolean part=methodKey.startsWith("ps.")&&primaryPart(vm,receiver);if(part&&methodKey.equals("ps.a(F)V"))partReturns.add(profile+":"+receiver.uniqueID());if(part&&methodKey.equals("ps.d(F)V"))compileReturns.add(profile+":"+receiver.uniqueID());journal.println(ConstructorJson.json(ConstructorJson.map("event","return","profile",profile,"method",methodKey,"receiver",receiver.uniqueID(),"primary-part",part,"return-value",methodKey.equals("dc.t()Z")?value(item.returnValue()):null,"pose",pose(vm))));}
                        else if(methodKey.equals("nw.a()V")){String kind=kind(item.thread());if(!kind.equals("other")){++selectedReturns;journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-return","profile",profile,"kind",kind)));}}
                    }else if(event instanceof ExceptionEvent){ExceptionEvent item=(ExceptionEvent)event;boolean caught=item.catchLocation()!=null&&item.catchLocation().declaringType().name().equals("gv")&&item.catchLocation().codeIndex()==743;if(caught)++exceptions;journal.println(ConstructorJson.json(ConstructorJson.map("event","exception","profile",profile,"class",item.exception().referenceType().name(),"throw",where(item.location()),"catch",item.catchLocation()==null?null:where(item.catchLocation()),"gv-caught",caught)));}
                }
                journal.flush();require(!journal.checkError(),"observer journal complete");try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"actual target returned both draws");stdout.join(10000);stderr.join(10000);require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained target streams");
            require(selectedDraws==4&&selectedReturns==4&&prior.size()==4&&exceptions==0,"four actual selected draws and no swallowed GV exception");
            for(int profile=0;profile<2;++profile)for(String method:WATCH)if(!method.startsWith("ps.")&&!method.equals("dc.t()Z"))require(entryCounts.getOrDefault(profile+":"+method,0)==1&&returnCounts.getOrDefault(profile+":"+method,0)==1,"actual single body entry/return "+profile+":"+method);
            require(partReturns.size()==14&&compileReturns.stream().filter(s->s.startsWith("0:")).count()==7&&compileReturns.stream().noneMatch(s->s.startsWith("1:")),"all seven actual primary Parts return; compile once without field resets");
            Files.writeString(output.resolve("live-Code.json"),ConstructorJson.json(codes)+"\n");
        }finally{if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
