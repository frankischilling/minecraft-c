import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Passive original method-Code/call/active NW-buffer observer. No invokes/writes. */
public final class PistonDrawJdi {
    static Map<String,Object> map(Object...v){return ConstructorJson.map(v);}
    static void need(boolean v,String m){if(!v)throw new AssertionError(m);}
    static String hash(byte[] raw)throws Exception{StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    static final Map<String,List<String>> SELECT=new LinkedHashMap<>();
    static void add(String c,String...methods){SELECT.put(c,Arrays.asList(methods));}
    static {add("hy","a(Luk;DDDF)V");add("je","a(Ljava/lang/String;)V");add("uk","a(F)F","b(F)F","c(F)F","d(F)F");add("nw","a()V","b(DDD)V","a(III)V");add("cv","a(Luu;III)V","d(Luu;III)V","a(Luu;IIIZ)V","b(Luu;IIIZ)Z","c(Luu;IIIZ)Z","a(DDDDDDFD)V","b(DDDDDDFD)V","c(DDDDDDFD)V");add("h","a_(I)V","a()V");add("jq","i()I");add("u","a()V","b()V");add("net.minecraft.client.Minecraft","v()Z");add("ji","b(Ljava/lang/String;)I","b(I)V");}
    static boolean selected(Method m){return SELECT.getOrDefault(m.declaringType().name(),List.of()).contains(m.name()+m.signature());}
    static Object value(Value v){if(v==null)return null;if(v instanceof VoidValue)return map("void",true);if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));if(v instanceof StringReference)return ((StringReference)v).value();ObjectReference o=(ObjectReference)v;return map("class",o.referenceType().name(),"identity",o.uniqueID());}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> t=vm.classesByName(name);return t.isEmpty()?null:t.get(0);}
    static int active(VirtualMachine vm){ReferenceType t=type(vm,"PistonDrawObservation");return t==null?-1:((IntegerValue)t.getValue(t.fieldByName("active"))).value();}
    static Map<String,Object> where(Location l){return map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    static Object primitives(ObjectReference o){Map<String,Object> out=new TreeMap<>();for(Field f:o.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))out.put(f.name(),value(o.getValue(f)));return out;}
    static List<String> words(ObjectReference o,int count){ArrayReference a=(ArrayReference)o.getValue(o.referenceType().fieldByName("g"));need(count>=0&&count<=2048&&count<=a.length(),"bounded actual active NW words");List<String> out=new ArrayList<>();for(Value v:a.getValues(0,count))out.add(String.format(Locale.ROOT,"%08x",((IntegerValue)v).value()));return out;}
    static void drain(Process p,InputStream input,Path path,int limit,List<Object> errors){try(OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] buffer=new byte[8192];int total=0,n;while((n=input.read(buffer))!=-1){need(total+n<=limit,"target pipe bound");out.write(buffer,0,n);total+=n;}}catch(Throwable e){errors.add(map("type",e.getClass().getName(),"message",e.getMessage()));p.destroyForcibly();}}
    static String quote(String s){need(!s.contains("\""),"no connector quote");return "\""+s+"\"";}
    public static void main(String[] args)throws Exception {
        need(args.length==9,"out jdk mode natives cp jar saves profiles work");Path out=Path.of(args[0]);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> a=connector.defaultArguments();
        String main="PistonDrawObservation "+quote(args[5])+" "+quote(args[6])+" "+quote(args[7])+" "+quote(args[3])+" "+quote(args[8]);
        String options=(args[2].equals("interpreter")?"-Xint ":"")+"-Xverify:all -Xmx256m -XX:-UsePerfData -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+quote(args[3])+" -Djava.io.tmpdir="+quote(Path.of(args[8],"tmp").toString())+" -Duser.home="+quote(Path.of(args[8],"home").toString())+" -cp "+quote(args[4]);
        a.get("home").setValue(args[1]);a.get("main").setValue(main);a.get("options").setValue(options);a.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(a);Process child=vm.process();List<Object> errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),out.resolve("target.stdout"),4194304,errors));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),out.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();
        int events=0,draws=0,entries=0,returns=0;boolean deadlineHit=false,finished=false;Throwable failure=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.ndjson"),StandardOpenOption.CREATE_NEW))){
            EventRequestManager requests=vm.eventRequestManager();
            for(String name:SELECT.keySet()){ClassPrepareRequest p=requests.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();}
            for(String name:SELECT.keySet()){MethodExitRequest e=requests.createMethodExitRequest();e.addClassFilter(name);e.setSuspendPolicy(EventRequest.SUSPEND_ALL);e.enable();}
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);boolean alive=true;
            while(alive){if(System.nanoTime()>deadline){deadlineHit=true;throw new IOException("debugger deadline");}EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){ReferenceType t=((ClassPrepareEvent)event).referenceType();for(Method m:t.methods())if(selected(m)){journal.println(ConstructorJson.json(map("event","live-code","class",t.name(),"method",m.name(),"descriptor",m.signature(),"bytes",m.bytecodes().length,"sha256",hash(m.bytecodes()))));BreakpointRequest b=requests.createBreakpointRequest(m.location());b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();}}
                    else if(event instanceof BreakpointEvent){BreakpointEvent e=(BreakpointEvent)event;int profile=active(vm);if(profile<0)continue;Method m=e.location().method();StackFrame frame=e.thread().frame(0);ObjectReference receiver=frame.thisObject();List<Object> arguments=new ArrayList<>();for(Value v:frame.getArgumentValues())arguments.add(value(v));List<Object> frames=new ArrayList<>();for(StackFrame f:e.thread().frames())if(SELECT.containsKey(f.location().declaringType().name()))frames.add(where(f.location()));
                        Map<String,Object> record=map("event","entry","profile",profile,"site",where(e.location()),"receiver",receiver==null?null:value(receiver),"arguments",arguments,"source_frames",frames);journal.println(ConstructorJson.json(record));events++;
                        if(m.declaringType().name().equals("hy"))entries++;
                        if(m.declaringType().name().equals("nw")&&m.name().equals("a")&&m.signature().equals("()V")){ReferenceType t=type(vm,"nw");need(receiver.equals(t.getValue(t.fieldByName("a"))),"actual singleton NW receiver");int n=((IntegerValue)receiver.getValue(t.fieldByName("o"))).value();journal.println(ConstructorJson.json(map("event","nw-draw","profile",profile,"receiver",receiver.uniqueID(),"state",primitives(receiver),"source_frames",frames,"words",words(receiver,n))));draws++;}
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;int profile=active(vm);if(profile>=0&&selected(e.method())){journal.println(ConstructorJson.json(map("event","exit","profile",profile,"class",e.method().declaringType().name(),"method",e.method().name(),"descriptor",e.method().signature(),"return",value(e.returnValue()))));events++;if(e.method().declaringType().name().equals("hy"))returns++;}}
                    else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){alive=false;finished=true;}
                }
                journal.flush();need(!journal.checkError(),"journal physical write");try{set.resume();}catch(VMDisconnectedException e){alive=false;finished=true;}
            }
        }catch(Throwable e){failure=e;}finally{
            if(!finished)child.destroyForcibly();try{vm.dispose();}catch(VMDisconnectedException e){}
            if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(5,TimeUnit.SECONDS);}stdout.join(5000);stderr.join(5000);
            Map<String,Object> receipt=map("connector",connector.name(),"home",args[1],"main",main,"options",options,"suspend","true","target_exit",child.isAlive()?null:child.exitValue(),"timed_out",deadlineHit,"target_complete",finished,"pipe_capture_complete",!stdout.isAlive()&&!stderr.isAlive(),"reader_errors",errors,"events",events,"draws",draws,"entries",entries,"returns",returns,"failure",failure==null?null:map("class",failure.getClass().getName(),"message",failure.getMessage()));
            for(String name:List.of("target.stdout","target.stderr","journal.ndjson")){Path p=out.resolve(name);receipt.put(name,map("bytes",Files.size(p),"sha256",hash(Files.readAllBytes(p))));}
            Files.writeString(out.resolve("target.capture.json"),ConstructorJson.json(receipt)+"\n",StandardOpenOption.CREATE_NEW);
        }
        if(failure!=null)throw new AssertionError("passive observer failure",failure);
        need(child.exitValue()==0&&errors.isEmpty()&&!stdout.isAlive()&&!stderr.isAlive()&&entries==27&&returns==27,"complete direct draw capture");
        System.out.println("PASS passive original Piston draw observer "+entries+" entries "+draws+" batches");
    }
}
