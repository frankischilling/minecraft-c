import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only source entries, returns, instruction phases and independent entropy. */
public final class WorkbenchAutoCloseJdi {
    static final Set<String> TYPES=Set.of("gs","dc","ls","sn","it","dw","aa","oo","id","ob","ix","mq","hl","net.minecraft.client.Minecraft","java.util.Random","java.lang.Math","org.lwjgl.opengl.Display","vy");
    static final Set<String> WATCH=Set.of("gs.w_()V","ls.w_()V","sn.w_()V","ls.U()V","sn.U()V","dc.o()V","gs.o()V","ls.o()V",
        "it.b(Lgs;)Z","dc.r()V","gs.r()V","net.minecraft.client.Minecraft.a(Lda;)V","net.minecraft.client.Minecraft.g()V","oo.h()V","id.h()V","ob.a(ILgs;)V","aa.a(Lgs;)V","dw.a(Lgs;)V","it.a(Lgs;)V",
        "gs.a(Liz;)V","gs.a(Liz;Z)V","gs.a(Lhl;)V","ix.b(Liz;)V","mq.f_(I)Liz;","dc.a(Lvr;I)V","vy.a()V","org.lwjgl.opengl.Display.isActive()Z",
        "java.util.Random.nextInt(I)I","java.util.Random.nextFloat()F","java.lang.Math.random()D","java.lang.Math.cos(D)D","java.lang.Math.sin(D)D","hl.<init>(Lfd;DDDLiz;)V");
    static final Set<Integer> GS_PHASES=Set.of(0,111,114,136,139,143,146,151,154,382,390,402);
    static int active=-1,records;
    static Map<String,Object> codes=new TreeMap<>();
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static String key(Method method){return method.declaringType().name()+"."+method.name()+method.signature();}
    static Map<String,Object> map(Object... fields){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<fields.length;i+=2)result.put((String)fields[i],fields[i+1]);return result;}
    static String json(Object value){
        if(value==null)return "null";if(value instanceof String)return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r")+"\"";
        if(value instanceof Number||value instanceof Boolean)return value.toString();List<String> parts=new ArrayList<>();
        if(value instanceof Map){for(Map.Entry<?,?> entry:((Map<?,?>)value).entrySet())parts.add(json(entry.getKey())+":"+json(entry.getValue()));return "{"+String.join(",",parts)+"}";}
        for(Object entry:(Iterable<?>)value)parts.add(json(entry));return "["+String.join(",",parts)+"]";
    }
    static Field field(ReferenceType type,String owner,String name){for(Field value:type.allFields())if(value.declaringType().name().equals(owner)&&value.name().equals(name))return value;throw new AssertionError("actual field "+owner+"."+name);}
    static Value get(ObjectReference value,String owner,String name){return value.getValue(field(value.referenceType(),owner,name));}
    static ObjectReference root(VirtualMachine vm,String name){ReferenceType helper=vm.classesByName("WorkbenchAutoCloseTarget").get(0);return (ObjectReference)helper.getValue(helper.fieldByName(name));}
    static Object value(Value v){
        if(v==null||v instanceof VoidValue)return null;if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();
        if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));
        if(v instanceof StringReference)return ((StringReference)v).value();ObjectReference object=(ObjectReference)v;return map("class",object.referenceType().name(),"id",object.uniqueID());
    }
    static long seed(ObjectReference random){ObjectReference atom=(ObjectReference)get(random,"java.util.Random","seed");return ((LongValue)get(atom,"java.util.concurrent.atomic.AtomicLong","value")).value();}
    static Map<String,Object> state(VirtualMachine vm){
        ObjectReference player=root(vm,"player"),app=root(vm,"app"),screen=root(vm,"screen"),container=root(vm,"container");ObjectReference current=(ObjectReference)get(player,"gs","e"),fallback=(ObjectReference)get(player,"gs","d");
        Map<String,Object> smoothing=new LinkedHashMap<>();for(String name:new String[]{"o","p","q","r","s","t"})smoothing.put(name,value(get(player,"gs",name)));
        return map("active-it",Objects.equals(current,container),"active-default",Objects.equals(current,fallback),"app-screen-null",get(app,"net.minecraft.client.Minecraft","r")==null,"app-screen-same",Objects.equals(get(app,"net.minecraft.client.Minecraft","r"),screen),
            "player",player.uniqueID(),"default",fallback.uniqueID(),"active",current==null?null:current.uniqueID(),"screen",screen.uniqueID(),"it",container.uniqueID(),"smoothing",smoothing,
            "player-random-id",((ObjectReference)get(player,"sn","bs")).uniqueID(),"player-random",String.format(Locale.ROOT,"%016x",seed((ObjectReference)get(player,"sn","bs"))));
    }
    static void pin(Method method)throws Exception{String name=key(method);if(!method.isNative()&&!method.isAbstract()&&!codes.containsKey(name)){byte[] bytes=method.bytecodes();codes.put(name,map("code_bytes",bytes.length,"code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))));}}
    static void breakpoint(EventRequestManager manager,Method method,int pc){Location location=method.locationOfCodeIndex(pc);require(location!=null,"actual instruction "+key(method)+"@"+pc);BreakpointRequest request=manager.createBreakpointRequest(location);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static Map<String,Object> where(Location location){return map("method",key(location.method()),"pc",location.codeIndex());}
    static String quote(String text){require(!text.contains("\"")&&!text.contains("\n")&&!text.contains("\r"),"literal path");return "\""+text+"\"";}
    static final class Drain extends Thread{
        final InputStream stream;final Path path;Throwable failure;Drain(InputStream stream,Path path){this.stream=stream;this.path=path;setDaemon(true);}
        public void run(){try(InputStream input=stream;OutputStream output=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] buffer=new byte[8192];int count,total=0;while((count=input.read(buffer))!=-1){total+=count;require(total<8*1048576,"bounded target stream");output.write(buffer,0,count);}}catch(Throwable error){failure=error;}}
    }
    public static void main(String[] args)throws Exception{
        require(args.length==6,"output jdk natives classpath jar inputs");Path output=Path.of(args[0]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();options.get("home").setValue(args[1]);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -Dorg.lwjgl.librarypath="+quote(args[2])+" -classpath "+quote(args[3]));
        options.get("main").setValue("WorkbenchAutoCloseTarget "+quote(args[4])+" "+quote(output.resolve("game").toString())+" "+quote(args[2])+" "+quote(args[5]));options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"live original Code and return capability");EventRequestManager manager=vm.eventRequestManager();
            List<EventRequest> scoped=new ArrayList<>();
            for(String name:TYPES){ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter(name);prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();MethodEntryRequest entry=manager.createMethodEntryRequest();entry.addClassFilter(name);entry.setSuspendPolicy(EventRequest.SUSPEND_ALL);scoped.add(entry);MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);scoped.add(exit);}
            ClassPrepareRequest helper=manager.createClassPrepareRequest();helper.addClassFilter("WorkbenchAutoCloseTarget");helper.setSuspendPolicy(EventRequest.SUSPEND_ALL);helper.enable();
            ExceptionRequest exception=manager.createExceptionRequest(null,true,true);exception.setSuspendPolicy(EventRequest.SUSPEND_ALL);exception.enable();
            boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(240);
            while(!done){require(System.nanoTime()<deadline,"bounded read-only observer");EventSet set;try{set=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){ReferenceType owner=((ClassPrepareEvent)event).referenceType();if(owner.name().equals("WorkbenchAutoCloseTarget"))for(String name:new String[]{"begin","end"})for(Method method:owner.methodsByName(name))breakpoint(manager,method,0);if(owner.name().equals("gs"))for(Method method:owner.methodsByName("w_","()V"))for(int pc:GS_PHASES)breakpoint(manager,method,pc);continue;}
                    if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){done=true;continue;}
                    if(event instanceof BreakpointEvent){BreakpointEvent item=(BreakpointEvent)event;String method=key(item.location().method());
                        if(method.equals("WorkbenchAutoCloseTarget.begin(I)V")){require(active==-1,"separate source invocation");active=((IntegerValue)item.thread().frame(0).getArgumentValues().get(0)).value();require(active==records,"declared row order");journal.println(json(map("event","begin","index",active,"state",state(vm))));for(EventRequest request:scoped)request.enable();}
                        else if(method.equals("WorkbenchAutoCloseTarget.end()V")){require(active==records,"complete source invocation");for(EventRequest request:scoped)request.disable();journal.println(json(map("event","end","index",active,"state",state(vm))));++records;active=-1;}
                        else if(active>=0){pin(item.location().method());journal.println(json(map("event","phase","index",active,"method",method,"pc",item.location().codeIndex(),"state",state(vm))));}
                        continue;
                    }
                    if(active<0)continue;
                    if(event instanceof MethodEntryEvent){MethodEntryEvent item=(MethodEntryEvent)event;Method method=item.method();if(!WATCH.contains(key(method)))continue;pin(method);
                        StackFrame frame=item.thread().frame(0);List<Object> arguments=new ArrayList<>();for(Value argument:frame.getArgumentValues())arguments.add(value(argument));
                        Map<String,Object> entry=map("event","entry","index",active,"method",key(method),"receiver",value(frame.thisObject()),"arguments",arguments,"caller",where(item.thread().frame(1).location()));
                        if(method.declaringType().name().equals("java.util.Random"))entry.put("seed",String.format(Locale.ROOT,"%016x",seed(frame.thisObject())));journal.println(json(entry));
                    }else if(event instanceof MethodExitEvent){MethodExitEvent item=(MethodExitEvent)event;String method=key(item.method());if(!WATCH.contains(method))continue;
                        ObjectReference receiver=item.thread().frame(0).thisObject();Map<String,Object> entry=map("event","return","index",active,"method",method,"receiver",value(receiver),"value",value(item.returnValue()));
                        if(item.method().declaringType().name().equals("java.util.Random"))entry.put("seed",String.format(Locale.ROOT,"%016x",seed(receiver)));
                        if(method.equals("hl.<init>(Lfd;DDDLiz;)V"))entry.put("constructor",map("phase",value(get(receiver,"hl","d")),"yaw",value(get(receiver,"sn","aS")),"position",List.of(value(get(receiver,"sn","aM")),value(get(receiver,"sn","aN")),value(get(receiver,"sn","aO"))),"velocity",List.of(value(get(receiver,"sn","aP")),value(get(receiver,"sn","aQ")),value(get(receiver,"sn","aR"))),"private-random",String.format(Locale.ROOT,"%016x",seed((ObjectReference)get(receiver,"sn","bs")))));
                        journal.println(json(entry));
                    }else if(event instanceof ExceptionEvent){ExceptionEvent item=(ExceptionEvent)event;journal.println(json(map("event","exception","index",active,"class",item.exception().referenceType().name(),"throw",where(item.location()),"catch",item.catchLocation()==null?null:where(item.catchLocation()))));}
                }
                journal.flush();require(!journal.checkError(),"complete read-only transport");try{set.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"unchanged original target returned");stdout.join(10000);stderr.join(10000);require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained target streams");require(records==7&&active==-1,"seven declared calls completed");
            Files.writeString(output.resolve("live-Code.json"),json(codes)+"\n");
        }finally{if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
