import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only unchanged-original Workbench draw, hook and actual NW observation. */
public final class WorkbenchDrawingJdi {
    static final Set<String> WATCH=Set.of("id.a(IIF)V","oo.a(F)V","oo.k()V","id.a(Lgp;)V","id.a(Lgp;II)Z",
        "bb.a(Lsj;Lji;Liz;II)V","bb.b(Lsj;Lji;Liz;II)V","ji.b(Ljava/lang/String;)I","ji.b(I)V",
        "sj.b(Ljava/lang/String;III)V","sj.a(Ljava/lang/String;III)V","iz.l()Ljava/lang/String;","nh.b(Ljava/lang/String;)Ljava/lang/String;");
    static final Set<String> CLASSES=Set.of("oo","id","bb","ji","sj","iz","nh","nw");
    static final Set<Integer> ID_PHASES=Set.of(0,32,206,305,329,395,468,478,490);
    static final Set<Integer> OO_PHASES=Set.of(0,9,12,20,28,31,70,73);
    static final Map<String,Object> codes=new TreeMap<>();
    static final Map<String,Integer> entryCounts=new TreeMap<>(),returnCounts=new TreeMap<>();
    static List<String> pendingWords;
    static Map<String,Object> pendingState;
    static String pendingKind;
    static int pendingProfile,pendingOrdinal,selectedDraws,selectedReturns,totalDraws,totalReturns,exceptions;
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> values=vm.classesByName(name);return values.isEmpty()?null:values.get(0);}
    static ObjectReference root(VirtualMachine vm,String name){ReferenceType helper=type(vm,"WorkbenchDrawingTarget");return (ObjectReference)helper.getValue(helper.fieldByName(name));}
    static int active(VirtualMachine vm){ReferenceType helper=type(vm,"WorkbenchDrawingTarget");return helper==null?-1:((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();}
    static Value get(ObjectReference owner,String name){Field field=owner.referenceType().fieldByName(name);require(field!=null,"actual field "+name);return owner.getValue(field);}
    static String key(Method method){return method.declaringType().name()+"."+method.name()+method.signature();}
    static Object value(Value input){
        if(input==null||input instanceof VoidValue)return null;if(input instanceof BooleanValue)return ((BooleanValue)input).value();
        if(input instanceof IntegerValue)return ((IntegerValue)input).value();if(input instanceof LongValue)return ((LongValue)input).value();
        if(input instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value()));
        if(input instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value()));
        if(input instanceof StringReference)return ((StringReference)input).value();
        ObjectReference object=(ObjectReference)input;return ConstructorJson.map("class",object.referenceType().name(),"id",object.uniqueID());
    }
    static Map<String,Object> where(Location location){return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex());}
    static List<Object> frames(ThreadReference thread)throws Exception {List<Object> result=new ArrayList<>();for(StackFrame frame:thread.frames())if(CLASSES.contains(frame.location().declaringType().name())||Set.of("da","ub","bw","nw").contains(frame.location().declaringType().name()))result.add(where(frame.location()));return result;}
    static Map<String,Object> primitives(ObjectReference object){Map<String,Object> result=new TreeMap<>();for(Field f:object.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))result.put(f.declaringType().name()+"."+f.name(),value(object.getValue(f)));return result;}
    static Map<String,Object> owners(VirtualMachine vm){ObjectReference actor=root(vm,"player"),screen=root(vm,"screen");ObjectReference active=(ObjectReference)get(actor,"e"),fallback=(ObjectReference)get(actor,"d");return ConstructorJson.map("active-screen-same",active.equals(get(screen,"j")),"default-active-same",active.equals(fallback),"input-null",get(actor,"a")==null);}
    static List<String> words(ObjectReference object,int count){ArrayReference raw=(ArrayReference)get(object,"g");require(raw.length()>=count,"actual NW backing extent");List<String> result=new ArrayList<>();for(Value item:raw.getValues(0,count))result.add(String.format(Locale.ROOT,"%08x",((IntegerValue)item).value()));return result;}
    static String kind(ThreadReference thread)throws Exception {
        boolean background=false,tile=false,gradient=false;
        for(StackFrame frame:thread.frames()){
            Method m=frame.location().method();String owner=m.declaringType().name();
            if(owner.equals("da")&&m.name().equals("a")&&m.signature().equals("(I)V"))background=true;
            if(owner.equals("oo")&&m.name().equals("a")&&m.signature().equals("(F)V"))tile=true;
            if(owner.equals("ub")&&m.name().equals("a")&&m.signature().equals("(IIIIII)V"))gradient=true;
        }
        if(background&&gradient)return "gradient";
        if(tile)for(StackFrame frame:thread.frames())if(key(frame.location().method()).equals("ub.b(IIIIII)V"))return "tile";
        return "other";
    }
    static void pin(Method method)throws Exception {String key=key(method);if(!codes.containsKey(key)){byte[] bytes=method.bytecodes();codes.put(key,ConstructorJson.map("code_bytes",bytes.length,"code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))));}}
    static void breakpoint(EventRequestManager manager,Method method,long pc){Location location=pc==0?method.location():method.locationOfCodeIndex(pc);require(location!=null,"actual instruction boundary "+key(method)+"@"+pc);BreakpointRequest request=manager.createBreakpointRequest(location);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static void prepare(EventRequestManager manager,ReferenceType owner){
        for(Method method:owner.methods())if(WATCH.contains(key(method)))breakpoint(manager,method,0);
        if(owner.name().equals("nw"))for(Method method:owner.methods())if(Set.of("nw.a(I)V","nw.a()V").contains(key(method)))breakpoint(manager,method,0);
        if(owner.name().equals("id"))for(Method method:owner.methodsByName("a","(IIF)V"))for(int pc:ID_PHASES)if(pc!=0)breakpoint(manager,method,pc);
        if(owner.name().equals("oo"))for(Method method:owner.methodsByName("a","(F)V"))for(int pc:OO_PHASES)if(pc!=0)breakpoint(manager,method,pc);
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
        options.get("main").setValue("WorkbenchDrawingTarget "+quote(args[4])+" "+quote(output.resolve("game").toString())+" "+quote(args[2]));options.get("suspend").setValue("true");
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
                        if(methodKey.equals("id.a(IIF)V")&&ID_PHASES.contains((int)pc)||methodKey.equals("oo.a(F)V")&&OO_PHASES.contains((int)pc))journal.println(ConstructorJson.json(ConstructorJson.map("event","phase","profile",profile,"method",methodKey,"pc",pc,"owners",owners(vm),"frames",frames(item.thread()))));
                        if(WATCH.contains(methodKey)&&pc==0){ObjectReference receiver=item.thread().frame(0).thisObject();entryCounts.merge(profile+":"+methodKey,1,Integer::sum);List<Object> arguments=new ArrayList<>();for(Value argument:item.thread().frame(0).getArgumentValues())arguments.add(value(argument));journal.println(ConstructorJson.json(ConstructorJson.map("event","entry","profile",profile,"method",methodKey,"receiver",receiver.uniqueID(),"arguments",arguments,"frames",frames(item.thread()))));}
                        if(methodKey.equals("nw.a(I)V")){
                            require(pendingWords==null,"source NW begin has no unfinished previous batch");ObjectReference receiver=item.thread().frame(0).thisObject();ReferenceType owner=receiver.referenceType();require(receiver.equals(owner.getValue(owner.fieldByName("a"))),"actual singleton NW receiver");
                            pendingKind=kind(item.thread());pendingProfile=profile;pendingOrdinal=totalDraws;pendingState=primitives(receiver);pendingWords=words(receiver,4096);
                            if(!pendingKind.equals("other"))journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-begin","profile",profile,"kind",pendingKind,"state",pendingState,"words",pendingWords.subList(0,48),"frames",frames(item.thread()))));
                        }else if(methodKey.equals("nw.a()V")){
                            ObjectReference receiver=item.thread().frame(0).thisObject();int count=((IntegerValue)get(receiver,"o")).value();require(pendingWords!=null&&pendingProfile==profile&&count>=0&&count<=4096,"actual bounded complete NW batch with own pre-begin backing");
                            String kind=kind(item.thread());require(kind.equals(pendingKind),"actual source NW begin/draw caller kind");if(!kind.equals("other")){require(count==48,"actual selected default triangle quad");++selectedDraws;}
                            journal.println(ConstructorJson.json(ConstructorJson.map("event",kind.equals("other")?"nw-other-draw":"nw-draw","profile",profile,"kind",kind,"ordinal",pendingOrdinal,"state",primitives(receiver),"words",words(receiver,count),"prior",pendingWords.subList(0,count),"begin-state",pendingState,"frames",frames(item.thread()))));++totalDraws;
                        }
                    }else if(event instanceof MethodExitEvent){MethodExitEvent item=(MethodExitEvent)event;String methodKey=key(item.method());
                        if(WATCH.contains(methodKey)){ObjectReference receiver=item.thread().frame(0).thisObject();returnCounts.merge(profile+":"+methodKey,1,Integer::sum);journal.println(ConstructorJson.json(ConstructorJson.map("event","return","profile",profile,"method",methodKey,"receiver",receiver.uniqueID(),"return-value",value(item.returnValue()))));}
                        else if(methodKey.equals("nw.a()V")){require(pendingWords!=null&&pendingProfile==profile,"actual matching NW return");if(!pendingKind.equals("other"))++selectedReturns;++totalReturns;journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-return","profile",profile,"kind",pendingKind,"ordinal",pendingOrdinal)));pendingWords=null;}
                    }else if(event instanceof ExceptionEvent){ExceptionEvent item=(ExceptionEvent)event;++exceptions;journal.println(ConstructorJson.json(ConstructorJson.map("event","exception","profile",profile,"class",item.exception().referenceType().name(),"throw",where(item.location()),"catch",item.catchLocation()==null?null:where(item.catchLocation()))));}
                }
                journal.flush();require(!journal.checkError(),"observer journal complete");try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"actual target returned both draws");stdout.join(10000);stderr.join(10000);require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained target streams");
            require(selectedDraws==4&&selectedReturns==4&&totalDraws==totalReturns&&pendingWords==null&&exceptions==0,"all actual NW draws return; four selected quads; no source caught exception");
            for(int profile=0;profile<2;++profile){
                for(String method:WATCH)require(entryCounts.getOrDefault(profile+":"+method,0).equals(returnCounts.getOrDefault(profile+":"+method,0)),"every entered source body returned "+method);
                for(String method:new String[]{"id.a(IIF)V","oo.a(F)V","oo.k()V"})require(entryCounts.getOrDefault(profile+":"+method,0)==1,"single inherited draw/OO hooks");
                for(String method:new String[]{"id.a(Lgp;)V","id.a(Lgp;II)Z"})require(entryCounts.getOrDefault(profile+":"+method,0)==46,"actual46 source Slot visits");
            }
            Files.writeString(output.resolve("live-Code.json"),ConstructorJson.json(codes)+"\n");
        }finally{if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
