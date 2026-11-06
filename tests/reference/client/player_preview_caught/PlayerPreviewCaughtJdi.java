import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only Source catch/print/escape and genuine frame/warm continuation observer. */
public final class PlayerPreviewCaughtJdi {
    static final Set<String> WATCH=Set.of("ue.a(IIF)V","id.a(IIF)V","ue.a(F)V","th.a(Lsn;DDDFF)V","th.a(Lfd;Lji;Lsj;Lls;Lkv;F)V","th.a(Lfd;)V",
        "ds.a(Lsn;DDDFF)V","ds.a(Lgs;DDDFF)V","gv.a(Lls;DDDFF)V","bw.a(Ljava/lang/String;Ljava/lang/String;)Z","bw.b(Lsn;DDDFF)V",
        "ds.a(Lls;DDD)V","ds.a(Lgs;DDD)V","gv.a(Lls;DDD)V","fh.a(FFFFFF)V","ps.a(F)V","ps.d(F)V","dc.t()Z");
    static final Set<String> JDK=Set.of("java.lang.Throwable.printStackTrace()V","java.lang.Throwable.printStackTrace(Ljava/io/PrintStream;)V",
        "java.lang.Throwable.printStackTrace(Ljava/lang/Throwable$PrintStreamOrWriter;)V","java.lang.NullPointerException.getMessage()Ljava/lang/String;");
    static final Set<String> CLASSES=Set.of("ue","id","th","ds","gv","bw","fh","ps","dc","java.lang.Throwable","java.lang.NullPointerException");
    static final Map<String,Object> codes=new TreeMap<>();
    static final Map<Long,ObjectReference> known=new HashMap<>();
    static final Map<Long,Integer> profiles=new HashMap<>();
    static final Map<Integer,ObjectReference> caught=new HashMap<>();
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> values=vm.classesByName(name);return values.isEmpty()?null:values.get(0);}
    static ObjectReference root(VirtualMachine vm,String name){ReferenceType helper=type(vm,"PlayerPreviewCaughtTarget");return (ObjectReference)helper.getValue(helper.fieldByName(name));}
    static int active(VirtualMachine vm){ReferenceType helper=type(vm,"PlayerPreviewCaughtTarget");return helper==null?-1:((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();}
    static Field field(ReferenceType owner,String declaring,String name){for(Field value:owner.allFields())if(value.name().equals(name)&&(declaring==null||value.declaringType().name().equals(declaring)))return value;return null;}
    static Value get(ObjectReference owner,String name){Field value=field(owner.referenceType(),null,name);require(value!=null,"actual field "+name);return owner.getValue(value);}
    static Value optional(ObjectReference owner,String declaring,String name){Field value=field(owner.referenceType(),declaring,name);return value==null?null:owner.getValue(value);}
    static String key(Method method){return method.declaringType().name()+"."+method.name()+method.signature();}
    static Object value(Value input){
        if(input==null||input instanceof VoidValue)return null;if(input instanceof BooleanValue)return ((BooleanValue)input).value();
        if(input instanceof IntegerValue)return ((IntegerValue)input).value();if(input instanceof LongValue)return ((LongValue)input).value();
        if(input instanceof ByteValue)return ((ByteValue)input).value();if(input instanceof ShortValue)return ((ShortValue)input).value();
        if(input instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value()));
        if(input instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value()));
        if(input instanceof StringReference)return ((StringReference)input).value();
        ObjectReference object=(ObjectReference)input;return ConstructorJson.map("class",object.referenceType().name(),"id",object.uniqueID());
    }
    static Map<String,Object> where(Location location){String source=null;try{source=location.sourceName();}catch(AbsentInformationException absent){}
        return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex(),"line",location.lineNumber(),"file",source);}
    static List<Object> frames(ThreadReference thread)throws Exception {List<Object> result=new ArrayList<>();for(StackFrame frame:thread.frames())result.add(where(frame.location()));return result;}
    static Map<String,Object> pose(VirtualMachine vm){ObjectReference actor=root(vm,"player"),manager=root(vm,"manager"),screen=root(vm,"screen");Map<String,Object> result=new LinkedHashMap<>();for(String name:new String[]{"H","aS","aT","bE"})result.put(name,value(get(actor,name)));result.put("manager-i",value(get(manager,"i")));result.put("mouse-x",value(get(screen,"l")));result.put("mouse-y",value(get(screen,"m")));for(String[] edge:new String[][]{{"e","engine"},{"g","world"},{"h","camera"},{"k","settings"},{"p","font"}})result.put("manager-"+edge[1]+"-null",get(manager,edge[0])==null);return result;}
    static List<Object> models(VirtualMachine vm){ObjectReference renderer=root(vm,"renderer");List<Object> result=new ArrayList<>();for(String role:new String[]{"a","g","h"}){ObjectReference model=(ObjectReference)get(renderer,role);result.add(ConstructorJson.map("role",role,"class",model.referenceType().name(),"held",value(get(model,"k")),"sneak",value(get(model,"l"))));}return result;}
    static Map<String,Object> throwable(ObjectReference object){
        try{
            Map<String,Object> result=ConstructorJson.map("class",object.referenceType().name(),"id",object.uniqueID(),"detail-message",value(optional(object,"java.lang.Throwable","detailMessage")),"extended-message",value(optional(object,"java.lang.NullPointerException","extendedMessage")),"cause-self",object.equals(optional(object,"java.lang.Throwable","cause")));
            Value raw=optional(object,"java.lang.Throwable","stackTrace");List<Object> trace=new ArrayList<>();
            if(raw instanceof ArrayReference)for(Value item:((ArrayReference)raw).getValues()){
                ObjectReference element=(ObjectReference)item;Map<String,Object> row=new LinkedHashMap<>();
                for(String name:new String[]{"declaringClass","methodName","fileName","lineNumber","classLoaderName","moduleName","moduleVersion","format"})row.put(name,value(optional(element,"java.lang.StackTraceElement",name)));
                trace.add(row);
            }
            result.put("stack-trace",trace);result.put("stack-field-null",raw==null);return result;
        }catch(ObjectCollectedException collected){return ConstructorJson.map("id",object.uniqueID(),"collected",true);}
    }
    static String printOrigin(ThreadReference thread)throws Exception {for(StackFrame frame:thread.frames()){Location l=frame.location();if(key(l.method()).equals("gv.a(Lls;DDDFF)V")&&l.codeIndex()==747)return "source-GV747";if(l.declaringType().name().equals("java.lang.ThreadGroup")&&l.method().name().equals("uncaughtException"))return "JVM-uncaught-handler";}return "other-caller";}
    static boolean primaryPart(VirtualMachine vm,ObjectReference object){ObjectReference model=root(vm,"primary");for(String name:new String[]{"a","b","c","d","e","f","g"})if(object.equals(get(model,name)))return true;return false;}
    static void pin(Method method)throws Exception {
        String name=key(method);if(codes.containsKey(name))return;List<Object> lines=new ArrayList<>();try{for(Location l:method.allLineLocations())lines.add(where(l));}catch(AbsentInformationException absent){}
        Map<String,Object> row=ConstructorJson.map("native",method.isNative(),"bootstrap-loader",method.declaringType().classLoader()==null,"lines",lines);
        if(!method.isNative()&&!method.isAbstract()){byte[] bytes=method.bytecodes();row.put("code_bytes",bytes.length);row.put("code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));}codes.put(name,row);
    }
    static void breakpoint(EventRequestManager manager,Method method,long pc){Location location=pc==0?method.location():method.locationOfCodeIndex(pc);require(location!=null,"actual instruction boundary "+key(method)+"@"+pc);BreakpointRequest request=manager.createBreakpointRequest(location);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static void prepare(EventRequestManager manager,ReferenceType owner){
        for(Method method:owner.methods())if(WATCH.contains(key(method))||JDK.contains(key(method)))breakpoint(manager,method,0);
        if(owner.name().equals("ue"))for(Method method:owner.methodsByName("a","(F)V"))for(int pc:new int[]{347,350,361,397,400,403,408})breakpoint(manager,method,pc);
        if(owner.name().equals("gv"))for(Method method:owner.methodsByName("a","(Lls;DDDFF)V"))for(int pc:new int[]{743,745,747,750,753,756,759,766,769})breakpoint(manager,method,pc);
        if(owner.name().equals("ds"))for(Method method:owner.methodsByName("a","(Lgs;DDDFF)V"))for(int pc:new int[]{115,139,163})breakpoint(manager,method,pc);
        if(owner.name().equals("th")){
            for(Method method:owner.methodsByName("a","(Lsn;DDDFF)V"))for(int pc:new int[]{27,39,42})breakpoint(manager,method,pc);
            for(Method method:owner.methodsByName("a","(Lfd;Lji;Lsj;Lls;Lkv;F)V"))for(int pc:new int[]{7,10,27})breakpoint(manager,method,pc);
        }
    }
    static final class Drain extends Thread {
        final InputStream input;final Path path;Throwable failure;Drain(InputStream input,Path path){this.input=input;this.path=path;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] bytes=new byte[8192];int count,total=0;while((count=in.read(bytes))!=-1){total+=count;require(total<=4*1048576,"bounded target streams");out.write(bytes,0,count);}}catch(Throwable error){failure=error;}}
    }
    static String quote(String text){require(!text.contains("\"")&&!text.contains("\n")&&!text.contains("\r"),"literal command path");return "\""+text+"\"";}
    public static void main(String[] args)throws Exception {
        require(args.length==6,"output jdk-home natives classpath jar mode");Path output=Path.of(args[0]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[1]);options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+quote(args[2])+" -classpath "+quote(args[3]));
        options.get("main").setValue("PlayerPreviewCaughtTarget "+quote(args[4])+" "+quote(output.resolve("game").toString())+" "+quote(args[2])+" "+args[5]);options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"read-only live Code/return capability");EventRequestManager manager=vm.eventRequestManager();
            for(String name:CLASSES){ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter(name);prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.enable();ReferenceType existing=type(vm,name);if(existing!=null&&existing.isPrepared())prepare(manager,existing);}
            ExceptionRequest exception=manager.createExceptionRequest(null,true,true);exception.setSuspendPolicy(EventRequest.SUSPEND_ALL);exception.enable();
            boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);
            while(!done){require(System.nanoTime()<deadline,"bounded original observer");EventSet events;try{events=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}
                    if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){done=true;continue;}
                    int profile=active(vm);
                    if(event instanceof ExceptionEvent){ExceptionEvent item=(ExceptionEvent)event;ObjectReference object=item.exception();if(profile<0)profile=profiles.getOrDefault(object.uniqueID(),-1);if(profile<0)continue;
                        known.put(object.uniqueID(),object);profiles.put(object.uniqueID(),profile);boolean sourceCaught=item.catchLocation()!=null&&key(item.catchLocation().method()).equals("gv.a(Lls;DDDFF)V")&&item.catchLocation().codeIndex()==743;if(sourceCaught)caught.put(profile,object);
                        journal.println(ConstructorJson.json(ConstructorJson.map("event","exception","profile",profile,"throw",where(item.location()),"catch",item.catchLocation()==null?null:where(item.catchLocation()),"source-GV-caught",sourceCaught,"throwable",throwable(object),"frames",frames(item.thread()))));continue;
                    }
                    if(event instanceof BreakpointEvent){BreakpointEvent item=(BreakpointEvent)event;Method method=item.location().method();String name=key(method);ObjectReference receiver=item.thread().frame(0).thisObject();if(profile<0&&receiver!=null&&JDK.contains(name))profile=profiles.getOrDefault(receiver.uniqueID(),-1);if(profile<0)continue;pin(method);long pc=item.location().codeIndex();
                        if(WATCH.contains(name)&&pc==0){List<Object> arguments=new ArrayList<>();for(Value argument:item.thread().frame(0).getArgumentValues())arguments.add(value(argument));journal.println(ConstructorJson.json(ConstructorJson.map("event","entry","profile",profile,"method",name,"receiver",receiver.uniqueID(),"arguments",arguments,"primary-part",name.startsWith("ps.")&&primaryPart(vm,receiver),"frames",frames(item.thread()))));}
                        if(JDK.contains(name)){journal.println(ConstructorJson.json(ConstructorJson.map("event","jdk-entry","profile",profile,"method",name,"origin",printOrigin(item.thread()),"throwable",throwable(receiver),"frames",frames(item.thread()))));}
                        else if(name.equals("ue.a(F)V")||name.equals("gv.a(Lls;DDDFF)V")||name.equals("ds.a(Lgs;DDDFF)V")||name.startsWith("th.a(")){
                            Map<String,Object> row=ConstructorJson.map("event","phase","profile",profile,"source",where(item.location()),"pose",pose(vm),"models",models(vm));if(caught.containsKey(profile))row.put("caught",throwable(caught.get(profile)));journal.println(ConstructorJson.json(row));
                        }
                    }else if(event instanceof MethodExitEvent){MethodExitEvent item=(MethodExitEvent)event;String name=key(item.method());ObjectReference receiver=item.thread().frame(0).thisObject();if(profile<0&&receiver!=null&&JDK.contains(name))profile=profiles.getOrDefault(receiver.uniqueID(),-1);if(profile<0)continue;
                        if(WATCH.contains(name))journal.println(ConstructorJson.json(ConstructorJson.map("event","return","profile",profile,"method",name,"receiver",receiver.uniqueID(),"primary-part",name.startsWith("ps.")&&primaryPart(vm,receiver),"return-value",name.equals("dc.t()Z")?value(item.returnValue()):null,"pose",pose(vm),"models",models(vm))));
                        else if(JDK.contains(name))journal.println(ConstructorJson.json(ConstructorJson.map("event","jdk-return","profile",profile,"method",name,"origin",printOrigin(item.thread()),"return-value",value(item.returnValue()),"throwable",throwable(receiver))));
                    }
                }
                journal.flush();require(!journal.checkError(),"complete journal");try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS),"original target terminal");int exit=child.exitValue();require(args[5].equals("cold")?(exit==0||exit==1):exit==0,"recorded cold outcome or actual warm return");stdout.join(10000);stderr.join(10000);require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained target streams");
            Files.writeString(output.resolve("live-Code.json"),ConstructorJson.json(codes)+"\n");Files.writeString(output.resolve("target-exit.json"),ConstructorJson.json(ConstructorJson.map("mode",args[5],"exit",exit))+"\n");
        }finally{if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
