import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Read-only source observation: no target invocation, allocation or mutation. */
public final class StreamFaultJdi {
    static final Set<String> CLASSES=Set.of("StreamFaultTarget","mx","fm","ei","as","java.io.File","java.io.FileOutputStream","java.io.DataOutputStream","java.io.FilterOutputStream","java.util.zip.GZIPOutputStream","java.util.zip.DeflaterOutputStream");
    final VirtualMachine vm;final PrintWriter out;final Set<String>installed=new HashSet<>(),pinned=new HashSet<>();
    int phase=-1,sequence;
    StreamFaultJdi(VirtualMachine v,PrintWriter p){vm=v;out=p;}
    static Object value(Value v){
        if(v==null||v instanceof VoidValue)return null;
        if(v instanceof BooleanValue)return((BooleanValue)v).value();
        if(v instanceof IntegerValue)return((IntegerValue)v).value();
        if(v instanceof LongValue)return((LongValue)v).value();
        if(v instanceof StringReference)return((StringReference)v).value();
        if(v instanceof PrimitiveValue)return v.toString();
        ObjectReference o=(ObjectReference)v;return ConstructorJson.map("class",o.referenceType().name(),"oid",o.uniqueID());
    }
    static Map<String,Object> location(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    List<Object> frames(ThreadReference t)throws Exception{List<Object>r=new ArrayList<>();for(StackFrame f:t.frames())r.add(location(f.location()));return r;}
    List<Object> arguments(StackFrame f){List<Object>r=new ArrayList<>();for(Value v:f.getArgumentValues())r.add(value(v));return r;}
    Map<String,Object> fields(ObjectReference o){Map<String,Object>r=new TreeMap<>();if(o==null)return r;
        for(Field f:o.referenceType().allFields())if(!f.isStatic()){
            Value v=o.getValue(f);r.put(f.declaringType().name()+"."+f.name(),value(v));
            if(v instanceof ObjectReference && f.typeName().equals("java.util.zip.Deflater")){
                ObjectReference d=(ObjectReference)v;Map<String,Object>s=new TreeMap<>();for(Field q:d.referenceType().allFields())if(!q.isStatic())s.put(q.name(),value(d.getValue(q)));r.put("actualDeflaterFields",s);
            }
        }return r;
    }
    List<Object> frameObjects(ThreadReference t)throws Exception{List<Object>r=new ArrayList<>();for(StackFrame f:t.frames()){String c=f.location().declaringType().name();if(c.startsWith("java.io.")||c.startsWith("java.util.zip.")||c.equals("StreamFaultTarget$Sink"))r.add(ConstructorJson.map("location",location(f.location()),"receiver",value(f.thisObject()),"fields",fields(f.thisObject())));}return r;}
    void emit(Map<String,Object>r){if(sequence>=50000)throw new IllegalStateException("bounded event transport");r.put("sequence",sequence++);r.put("phase",phase);out.println(ConstructorJson.json(r));out.flush();if(out.checkError())throw new IllegalStateException("observation transport");}
    void pin(Method m)throws Exception{String key=m.declaringType().name()+"."+m.name()+m.signature();if(!pinned.add(key))return;Map<String,Object>r=ConstructorJson.map("event","code-pin","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"native",m.isNative(),"abstract",m.isAbstract());if(!m.isNative()&&!m.isAbstract()){byte[]b=m.bytecodes();r.put("bytes",b.length);r.put("sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b)));}emit(r);}
    boolean selected(Method m){String c=m.declaringType().name(),n=m.name(),d=m.signature();
        if(c.equals("StreamFaultTarget"))return n.equals("mark");
        if(c.equals("mx")||c.equals("fm"))return n.equals("a")&&d.equals("(Lei;Ljava/util/List;)V");
        if(c.equals("ei"))return n.equals("a")&&(d.equals("(Ljava/util/List;)Lnu;")||d.equals("(Lnu;Lnu;)V"));
        if(c.equals("as"))return n.equals("a")&&(d.equals("(Lnu;Ljava/io/OutputStream;)V")||d.equals("(Lnu;Ljava/io/DataOutput;)V"));
        if(c.equals("java.io.File"))return Set.of("exists","delete","renameTo").contains(n);
        if(c.equals("java.io.FileOutputStream"))return Set.of("<init>","write","flush","close").contains(n);
        if(c.equals("java.io.DataOutputStream"))return n.equals("<init>");
        if(c.equals("java.io.FilterOutputStream"))return n.equals("close");
        if(c.equals("java.util.zip.GZIPOutputStream"))return Set.of("<init>","writeHeader","finish").contains(n);
        if(c.equals("java.util.zip.DeflaterOutputStream"))return Set.of("close","flush").contains(n);
        return false;
    }
    boolean reached(ThreadReference t)throws Exception{for(StackFrame f:t.frames())if(f.location().declaringType().name().equals("as")&&f.location().method().name().equals("a")&&f.location().method().signature().equals("(Lnu;Ljava/io/OutputStream;)V"))return true;return false;}
    void observe(String kind,ThreadReference t,Location l,Value result,boolean hasResult)throws Exception{
        Method m=l.method();if(!selected(m))return;StackFrame f=t.frame(0);
        if(m.declaringType().name().equals("StreamFaultTarget")&&m.name().equals("mark")){if(kind.equals("entry"))phase=((IntegerValue)f.getArgumentValues().get(0)).value();return;}
        if(phase<0||!reached(t))return;pin(m);
        Map<String,Object>r=ConstructorJson.map("event",kind,"location",location(l),"receiver",value(f.thisObject()),"receiver_fields",fields(f.thisObject()),"arguments",arguments(f),"frames",frames(t));if(hasResult)r.put("result",value(result));emit(r);
    }
    void prepare(ReferenceType t){String c=t.name();if(!CLASSES.contains(c)||!installed.add(c))return;
        for(Method m:t.methods())if(selected(m)&&!m.isNative()&&!m.isAbstract()){
            Location l=m.locationOfCodeIndex(0);if(l!=null){BreakpointRequest b=vm.eventRequestManager().createBreakpointRequest(l);b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();}
            if(c.equals("as")&&m.signature().equals("(Lnu;Ljava/io/OutputStream;)V"))for(long pc:new long[]{9,12,15,18,21,22,28,30,34,35}){
                BreakpointRequest b=vm.eventRequestManager().createBreakpointRequest(m.locationOfCodeIndex(pc));b.putProperty("source-prefix",true);b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();
            }
        }
    }
    void run()throws Exception{
        EventRequestManager r=vm.eventRequestManager();for(String c:CLASSES){ClassPrepareRequest p=r.createClassPrepareRequest();p.addClassFilter(c);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();MethodExitRequest e=r.createMethodExitRequest();e.addClassFilter(c);e.setSuspendPolicy(EventRequest.SUSPEND_ALL);e.enable();}
        for(ReferenceType t:vm.allClasses())if(t.isPrepared())prepare(t);
        ExceptionRequest ex=r.createExceptionRequest(null,true,true);ex.setSuspendPolicy(EventRequest.SUSPEND_ALL);ex.enable();boolean alive=true;
        while(alive){EventSet set=vm.eventQueue().remove();for(Event event:set){
            if(event instanceof ClassPrepareEvent)prepare(((ClassPrepareEvent)event).referenceType());
            else if(event instanceof BreakpointEvent){BreakpointEvent b=(BreakpointEvent)event;observe(b.request().getProperty("source-prefix")==null?"entry":"source-prefix",b.thread(),b.location(),null,false);}
            else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;observe("return",e.thread(),e.location(),e.returnValue(),true);}
            else if(event instanceof ExceptionEvent && phase>=0){ExceptionEvent e=(ExceptionEvent)event;if(reached(e.thread()))emit(ConstructorJson.map("event","exception","exception",value(e.exception()),"location",location(e.location()),"catch",e.catchLocation()==null?null:location(e.catchLocation()),"frames",frames(e.thread()),"frame_objects",frameObjects(e.thread())));}
            else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException e){alive=false;}}
    }
    public static void main(String[]args)throws Exception{
        Path output=Path.of(args[0]);String q="\"";LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument>a=connector.defaultArguments();
        a.get("home").setValue(args[1]);a.get("main").setValue("StreamFaultTarget "+q+args[0]+q+" "+q+args[2]+q);
        a.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Xlog:class+init=info:file="+q+output.resolve("class-init.log")+q+" -Xlog:library=info:file="+q+output.resolve("library-load.log")+q+" -cp "+q+args[3]+q);a.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(a);Process process=vm.process();Thread shutdown=new Thread(()->process.destroyForcibly());Runtime.getRuntime().addShutdownHook(shutdown);
        Thread stdout=new Thread(()->{try(OutputStream o=Files.newOutputStream(output.resolve("target.stdout"))){process.getInputStream().transferTo(o);}catch(IOException e){throw new UncheckedIOException(e);}});
        Thread stderr=new Thread(()->{try(OutputStream o=Files.newOutputStream(output.resolve("target.stderr"))){process.getErrorStream().transferTo(o);}catch(IOException e){throw new UncheckedIOException(e);}});stdout.start();stderr.start();
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output.resolve("checkpoints.ndjson")))){new StreamFaultJdi(vm,out).run();}finally{try{vm.dispose();}catch(VMDisconnectedException e){}}
        process.waitFor();stdout.join();stderr.join();Runtime.getRuntime().removeShutdownHook(shutdown);if(process.exitValue()!=0)throw new IllegalStateException("target exit "+process.exitValue());
    }
}
