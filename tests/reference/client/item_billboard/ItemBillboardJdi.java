import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only actual HL entropy, BB parent, Random, GL and NW observer. */
public final class ItemBillboardJdi {
    static final Set<String> WATCH=Set.of("bb.a(Lhl;DDDFF)V","hl.<init>(Lfd;DDDLiz;)V","iz.b()I","gm.a(I)I","gm.f(I)I","sn.a(F)F","in.a(F)F","bw.a(Ljava/lang/String;)V","ji.b(Ljava/lang/String;)I","ji.b(I)V","nw.b()V","nw.b(FFF)V","nw.a(DDDDD)V","nw.a()V","java.util.Random.setSeed(J)V","java.util.Random.nextFloat()F");
    static final Set<String> GL=Set.of("glTranslatef","glRotatef","glScalef","glPushMatrix","glPopMatrix","glEnable","glDisable","glColor4f","glDrawArrays");
    static final Set<String> CLASSES=Set.of("bb","hl","iz","gm","sn","in","bw","ji","nw","java.util.Random","org.lwjgl.opengl.GL11");
    static final Map<String,Object> codes=new TreeMap<>();
    static final List<Object> maths=new ArrayList<>();
    static ObjectReference actor;
    static List<String> prior;
    static int copy,icon=-1,draws,returns;
    static MethodExitRequest mathExit;
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> values=vm.classesByName(name);return values.isEmpty()?null:values.get(0);}
    static ObjectReference root(VirtualMachine vm,String name){ReferenceType helper=type(vm,"ItemBillboardTarget");return (ObjectReference)helper.getValue(helper.fieldByName(name));}
    static int active(VirtualMachine vm){ReferenceType helper=type(vm,"ItemBillboardTarget");return helper==null?-1:((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();}
    static Value get(ObjectReference owner,String name){Field field=owner.referenceType().fieldByName(name);require(field!=null,"actual field "+name);return owner.getValue(field);}
    static long seed(ObjectReference random){ObjectReference atomic=(ObjectReference)get(random,"seed");return ((LongValue)get(atomic,"value")).value();}
    static String seedWord(ObjectReference random){return String.format(Locale.ROOT,"%016x",seed(random));}
    static ObjectReference random(VirtualMachine vm){return (ObjectReference)get(root(vm,"renderer"),"f");}
    static String key(Method method){return method.declaringType().name()+"."+method.name()+method.signature();}
    static Object value(Value input){
        if(input==null||input instanceof VoidValue)return null;if(input instanceof BooleanValue)return ((BooleanValue)input).value();
        if(input instanceof IntegerValue)return ((IntegerValue)input).value();if(input instanceof LongValue)return ((LongValue)input).value();
        if(input instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value()));
        if(input instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value()));
        if(input instanceof StringReference)return ((StringReference)input).value();ObjectReference object=(ObjectReference)input;return ConstructorJson.map("class",object.referenceType().name(),"id",object.uniqueID());
    }
    static Map<String,Object> where(Location location){return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex());}
    static List<Object> frames(ThreadReference thread)throws Exception {List<Object> rows=new ArrayList<>();for(StackFrame frame:thread.frames())rows.add(where(frame.location()));return rows;}
    static boolean parent(ThreadReference thread)throws Exception {for(StackFrame frame:thread.frames())if(key(frame.location().method()).equals("bb.a(Lhl;DDDFF)V"))return true;return false;}
    static Map<String,Object> state(ObjectReference object){Map<String,Object> rows=new TreeMap<>();for(Field field:object.referenceType().allFields())if(!field.isStatic()&&Set.of("boolean","int","long","float","double").contains(field.typeName()))rows.put(field.declaringType().name()+"."+field.name(),value(object.getValue(field)));return rows;}
    static List<String> words(ObjectReference object,int count){ArrayReference raw=(ArrayReference)get(object,"g");List<String> rows=new ArrayList<>();for(Value item:raw.getValues(0,count))rows.add(String.format(Locale.ROOT,"%08x",((IntegerValue)item).value()));return rows;}
    static List<Object> vector(ObjectReference object,String[] fields){List<Object> rows=new ArrayList<>();for(String name:fields)rows.add(value(get(object,name)));return rows;}
    static void pin(Method method)throws Exception {String name=key(method);if(!codes.containsKey(name)&&!method.isNative()&&!method.isAbstract()){byte[] bytes=method.bytecodes();codes.put(name,ConstructorJson.map("code_bytes",bytes.length,"code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)),"bootstrap-loader",method.declaringType().classLoader()==null));}}
    static void breakpoint(EventRequestManager manager,Method method,long pc){Location location=pc==0?method.location():method.locationOfCodeIndex(pc);require(location!=null,"actual instruction boundary "+key(method)+"@"+pc);BreakpointRequest request=manager.createBreakpointRequest(location);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static void prepare(EventRequestManager manager,ReferenceType owner){
        for(Method method:owner.methods())if(WATCH.contains(key(method))||owner.name().equals("org.lwjgl.opengl.GL11")&&GL.contains(method.name()))breakpoint(manager,method,0);
        if(owner.name().equals("bb"))for(Method method:owner.methodsByName("a","(Lhl;DDDFF)V"))for(int pc:new int[]{7,10,35,44,113,124,127,361,367,372,375,400,403,474,506,509,556,559,577,580,590,655,668,673,681,703,726,749,771,776,779,782,788,790,793,796})breakpoint(manager,method,pc);
    }
    static final class Drain extends Thread{
        final InputStream input;final Path path;Throwable failure;Drain(InputStream input,Path path){this.input=input;this.path=path;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] bytes=new byte[8192];int count,total=0;while((count=in.read(bytes))!=-1){total+=count;require(total<=4*1048576,"bounded streams");out.write(bytes,0,count);}}catch(Throwable error){failure=error;}}
    }
    static String quote(String text){require(!text.contains("\"")&&!text.contains("\n")&&!text.contains("\r"),"literal command path");return "\""+text+"\"";}
    public static void main(String[] args)throws Exception {
        require(args.length==5,"output jdk natives classpath jar");Path output=Path.of(args[0]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();options.get("home").setValue(args[1]);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -Dorg.lwjgl.librarypath="+quote(args[2])+" -classpath "+quote(args[3]));
        options.get("main").setValue("ItemBillboardTarget "+quote(args[4])+" "+quote(output.resolve("game").toString())+" "+quote(args[2]));options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"read-only bytecode/return capability");EventRequestManager manager=vm.eventRequestManager();
            for(String name:CLASSES){ClassPrepareRequest request=manager.createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.enable();ReferenceType existing=type(vm,name);if(existing!=null&&existing.isPrepared())prepare(manager,existing);}
            mathExit=manager.createMethodExitRequest();mathExit.addClassFilter("java.lang.Math");mathExit.setSuspendPolicy(EventRequest.SUSPEND_ALL);
            ExceptionRequest exceptions=manager.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_ALL);exceptions.enable();
            boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);
            while(!done){require(System.nanoTime()<deadline,"bounded actual observer");EventSet events;try{events=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){done=true;continue;}
                    int profile=active(vm);if(profile<0)continue;
                    if(event instanceof ExceptionEvent){ExceptionEvent item=(ExceptionEvent)event;journal.println(ConstructorJson.json(ConstructorJson.map("event","exception","profile",profile,"class",item.exception().referenceType().name(),"throw",where(item.location()),"catch",item.catchLocation()==null?null:where(item.catchLocation()))));continue;}
                    if(event instanceof BreakpointEvent){BreakpointEvent item=(BreakpointEvent)event;Method method=item.location().method();String name=key(method);long pc=item.location().codeIndex();ObjectReference receiver=item.thread().frame(0).thisObject();
                        boolean constructor=name.equals("hl.<init>(Lfd;DDDLiz;)V");boolean inParent=parent(item.thread());if(!constructor&&!inParent)continue;
                        if(name.startsWith("java.util.Random.")&&!receiver.equals(random(vm)))continue;pin(method);
                        if(constructor){require(actor==null,"one actual full Item constructor");actor=receiver;maths.clear();mathExit.enable();}
                        if(name.equals("bb.a(Lhl;DDDFF)V")&&pc==0){copy=0;icon=-1;require(prior==null,"no unfinished prior batch");}
                        if(name.equals("bb.a(Lhl;DDDFF)V"))journal.println(ConstructorJson.json(ConstructorJson.map("event","phase","profile",profile,"copy",copy,"pc",pc,"renderer-random",seedWord(random(vm)),"manager-yaw",value(get(root(vm,"manager"),"i")))));
                        if(pc==0){List<Object> arguments=new ArrayList<>();for(Value argument:item.thread().frame(0).getArgumentValues())arguments.add(value(argument));journal.println(ConstructorJson.json(ConstructorJson.map("event",method.declaringType().name().equals("org.lwjgl.opengl.GL11")?"gl-entry":"entry","profile",profile,"copy",copy,"method",name,"receiver",receiver==null?null:receiver.uniqueID(),"arguments",arguments,"renderer-random",seedWord(random(vm)),"frames",frames(item.thread()))));}
                        if(name.equals("nw.b()V")){require(prior==null,"source begin has no pending batch");prior=words(receiver,64);journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-begin","profile",profile,"copy",copy,"state",state(receiver),"words",prior,"frames",frames(item.thread()))));}
                        if(name.equals("nw.a()V")){require(prior!=null&&((IntegerValue)get(receiver,"o")).value()==48,"actual complete converted quad");require(receiver.equals(receiver.referenceType().getValue(receiver.referenceType().fieldByName("a"))),"actual singleton NW");++draws;journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-draw","profile",profile,"copy",copy,"icon",icon,"state",state(receiver),"words",words(receiver,64),"prior",prior,"frames",frames(item.thread()))));}
                    }else if(event instanceof MethodExitEvent){MethodExitEvent item=(MethodExitEvent)event;String name=key(item.method());ObjectReference receiver=item.thread().frame(0).thisObject();
                        if(actor!=null&&name.equals("java.lang.Math.random()D")){maths.add(value(item.returnValue()));continue;}
                        if(name.equals("hl.<init>(Lfd;DDDLiz;)V")){require(actor!=null&&actor.equals(receiver)&&maths.size()==4,"actual full constructor and four shared Math returns");journal.println(ConstructorJson.json(ConstructorJson.map("event","constructor","profile",profile,"math-values",new ArrayList<>(maths),"phase",value(get(actor,"d")),"yaw",value(get(actor,"aS")),"position",vector(actor,new String[]{"aM","aN","aO"}),"velocity",vector(actor,new String[]{"aP","aQ","aR"}),"private-random",seedWord((ObjectReference)get(actor,"bs")))));actor=null;mathExit.disable();continue;}
                        if(!WATCH.contains(name)&&!item.method().declaringType().name().equals("org.lwjgl.opengl.GL11"))continue;if(!parent(item.thread()))continue;
                        if(name.startsWith("java.util.Random.")&&!receiver.equals(random(vm)))continue;
                        if(name.equals("iz.b()I"))icon=((IntegerValue)item.returnValue()).value();
                        journal.println(ConstructorJson.json(ConstructorJson.map("event",item.method().declaringType().name().equals("org.lwjgl.opengl.GL11")?"gl-return":"return","profile",profile,"copy",copy,"method",name,"receiver",receiver==null?null:receiver.uniqueID(),"return-value",value(item.returnValue()),"renderer-random",seedWord(random(vm)))));
                        if(name.equals("nw.a()V")){require(prior!=null,"actual matching draw return");++returns;prior=null;++copy;}
                    }
                }
                journal.flush();require(!journal.checkError(),"complete raw journal");try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"four actual parent returns");stdout.join(10000);stderr.join(10000);require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained streams");require(draws==10&&returns==10&&prior==null&&actor==null,"ten source completed quads and complete constructors");
            Files.writeString(output.resolve("live-Code.json"),ConstructorJson.json(codes)+"\n");
        }finally{if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
