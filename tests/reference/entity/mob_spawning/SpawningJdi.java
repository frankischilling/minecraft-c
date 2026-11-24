import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Standard read-only JDI frame observations; no target invocation or class rewriting. */
public final class SpawningJdi {
    private final VirtualMachine vm;
    private final PrintWriter output;
    private final Set<String> installed=new HashSet<>();
    private final List<MethodExitRequest> clockRequests=new ArrayList<>();
    private int records;
    private SpawningJdi(VirtualMachine vm,PrintWriter output) { this.vm=vm; this.output=output; }
    private ReferenceType type(String name) { List<ReferenceType> found=vm.classesByName(name); return found.isEmpty()?null:found.get(0); }
    private Value stat(String owner,String name) { ReferenceType found=type(owner); return found==null||!found.isPrepared()?null:found.getValue(found.fieldByName(name)); }
    private int active() { Value found=stat("SpawningTarget","activeCase"); return found==null?-1:((IntegerValue)found).value(); }
    private static Value field(ObjectReference receiver,String owner,String name) {
        if(receiver==null) return null;
        for(Field found:receiver.referenceType().allFields()) if(found.declaringType().name().equals(owner)&&found.name().equals(name)) return receiver.getValue(found);
        throw new IllegalArgumentException(owner+"."+name);
    }
    private static ObjectReference object(ObjectReference receiver,String owner,String name) { return (ObjectReference)field(receiver,owner,name); }
    private static List<Integer> stringUnits(StringReference reference) {
        ArrayReference bytes=(ArrayReference)field(reference,"java.lang.String","value");
        int coder=((ByteValue)field(reference,"java.lang.String","coder")).value();
        List<Integer> units=new ArrayList<>();
        if(coder==0) { for(Value item:bytes.getValues()) units.add(((ByteValue)item).value()&255); return units; }
        if(coder!=1) throw new IllegalStateException("String coder "+coder);
        List<ReferenceType> types=reference.virtualMachine().classesByName("java.lang.StringUTF16");
        if(types.size()!=1||!types.get(0).isPrepared()) throw new IllegalStateException("StringUTF16 not prepared");
        ReferenceType utf16=types.get(0);
        int high=((IntegerValue)utf16.getValue(utf16.fieldByName("HI_BYTE_SHIFT"))).value();
        int low=((IntegerValue)utf16.getValue(utf16.fieldByName("LO_BYTE_SHIFT"))).value();
        for(int i=0;i<bytes.length();i+=2) units.add((((ByteValue)bytes.getValue(i)).value()&255)<<high|(((ByteValue)bytes.getValue(i+1)).value()&255)<<low);
        return units;
    }
    private static Object value(Value found) {
        if(found==null) return null;
        if(found instanceof FloatValue) return ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)found).value())));
        if(found instanceof DoubleValue) return ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)found).value())));
        if(found instanceof LongValue) return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",((LongValue)found).value()));
        if(found instanceof BooleanValue) return ((BooleanValue)found).value();
        if(found instanceof ByteValue) return (int)((ByteValue)found).value();
        if(found instanceof ShortValue) return (int)((ShortValue)found).value();
        if(found instanceof IntegerValue) return ((IntegerValue)found).value();
        if(found instanceof CharValue) return (int)((CharValue)found).value();
        ObjectReference ref=(ObjectReference)found;
        Map<String,Object> out=ConstructorJson.map("class",ref.referenceType().name(),"oid",ref.uniqueID());
        if(ref instanceof StringReference) out.put("utf16",stringUnits((StringReference)ref));
        if(ref instanceof ArrayReference) out.put("length",((ArrayReference)ref).length());
        return out;
    }
    private static Map<String,Object> fields(ObjectReference receiver) {
        if(receiver==null) return null;
        Map<String,Object> out=new TreeMap<>();
        for(Field found:receiver.referenceType().allFields()) if(!found.isStatic()) out.put(found.declaringType().name()+"."+found.name(),value(receiver.getValue(found)));
        return out;
    }
    private static int listSize(ObjectReference reference) { return reference==null?-1:((IntegerValue)field(reference,"java.util.ArrayList","size")).value(); }
    private static long atomic(ObjectReference reference) { return ((LongValue)reference.getValue(reference.referenceType().fieldByName("value"))).value(); }
    private static Map<String,Object> watcher(ObjectReference reference) {
        if(reference==null) return null;
        ObjectReference map=object(reference,"ud","b"); ArrayReference table=(ArrayReference)field(map,"java.util.HashMap","table");
        List<Map<String,Object>> rows=new ArrayList<>();
        if(table!=null) for(Value start:table.getValues()) {
            ObjectReference node=(ObjectReference)start;
            while(node!=null) {
                ObjectReference record=(ObjectReference)node.getValue(node.referenceType().fieldByName("value")); ObjectReference box=object(record,"ma","c");
                rows.add(ConstructorJson.map("index",value(field(record,"ma","b")),"type",value(field(record,"ma","a")),"value",value(box.getValue(box.referenceType().fieldByName("value"))),"dirty",value(field(record,"ma","d"))));
                node=(ObjectReference)node.getValue(node.referenceType().fieldByName("next"));
            }
        }
        rows.sort(Comparator.comparingInt(row->(Integer)row.get("index")));
        return ConstructorJson.map("size",value(field(map,"java.util.HashMap","size")),"dirty",value(field(reference,"ud","c")),"records",rows);
    }
    private Map<String,Object> snapshot(String event,Location location) {
        ObjectReference receiver=(ObjectReference)stat("SpawningTarget","actor"),world=(ObjectReference)stat("SpawningTarget","world");
        ObjectReference own=object(receiver,"sn","bs"),random=object(world,"fd","r");
        return ConstructorJson.map("record",records++,"event",event,"case",active(),"location",ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex()),
            "own_seed",own==null?null:String.format(Locale.ROOT,"%012x",atomic(object(own,"java.util.Random","seed"))),
            "world_seed",random==null?null:String.format(Locale.ROOT,"%012x",atomic(object(random,"java.util.Random","seed"))),
            "world_f",value(field(world,"fd","f")),"world_info_null",field(world,"fd","x")==null,"actor_world_null",receiver==null?null:field(receiver,"sn","aI")==null,"box_cursor",value(stat("eq","h")),"vector_cursor",value(stat("bt","e")),
            "nearby_count",listSize(object(world,"fd","R")),"collision_count",listSize(object(world,"fd","K")));
    }
    private void breakpoint(ReferenceType type,String name,String descriptor,long... pcs) throws Exception {
        List<Method> found=type.methodsByName(name,descriptor); if(found.size()!=1) throw new IllegalStateException("method "+type.name()+"."+name);
        byte[] code=found.get(0).bytecodes(); StringJoiner digest=new StringJoiner(""); for(byte item:MessageDigest.getInstance("SHA-256").digest(code)) digest.add(String.format(Locale.ROOT,"%02x",item&255));
        output.println(ConstructorJson.json(ConstructorJson.map("event","code-pin","class",type.name(),"method",name,"descriptor",descriptor,"bytes",code.length,"sha256",digest.toString()))); output.flush();
        for(long pc:pcs) { Location where=found.get(0).locationOfCodeIndex(pc); if(where==null) throw new IllegalStateException("PC "+pc); BreakpointRequest request=vm.eventRequestManager().createBreakpointRequest(where); request.setSuspendPolicy(EventRequest.SUSPEND_ALL); request.enable(); }
    }
    private void install(ReferenceType type) throws Exception {
        if(!type.isPrepared()||!installed.add(type.name()))return;
        switch(type.name()) {
            case "SpawningTarget": {
                ModificationWatchpointRequest request=vm.eventRequestManager().createModificationWatchpointRequest(type.fieldByName("activeCase"));request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();break;
            }
            case "gz":breakpoint(type,"d","()Z",0,40,54,64,73,94,104,115,129,136,140);breakpoint(type,"a","(III)F",0,12);break;
            case "ii":breakpoint(type,"d","()Z",0,31,41);break;
            case "ls":breakpoint(type,"d","()Z",0,11,31,45,48,52);break;
            case "uw":breakpoint(type,"d","()Z",0,22,40,49,59,64,67,78,80);breakpoint(type,"v","()I",0);break;
            case "bg":breakpoint(type,"d","()Z",0,39,48,58,63,67,70,74);breakpoint(type,"a","(III)F",0,21,24,34,37);break;
            case "ar":breakpoint(type,"d","()Z",0,11);break;
            case "lm":breakpoint(type,"a","(J)Ljava/util/Random;",0,63);break;
            case "li":breakpoint(type,"a","(J)Ljava/util/Random;",0,63);break;
            case "fd":breakpoint(type,"a","(Leq;)Z",0,33,41,47,55);breakpoint(type,"a","(Lsn;Leq;)Ljava/util/List;",0);breakpoint(type,"b","(Leq;)Z",0);breakpoint(type,"n","(III)I",0);breakpoint(type,"m","(III)I",0);breakpoint(type,"B","()Z",0);breakpoint(type,"c","(III)F",0);breakpoint(type,"s","()J",0);break;
            default:throw new IllegalStateException(type.name());
        }
    }
    private void run() throws Exception {
        for(String name:new String[]{"SpawningTarget","ls","ii","gz","uw","bg","ar","lm","li","fd"}) {
            ClassPrepareRequest request=vm.eventRequestManager().createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
            ReferenceType existing=type(name);if(existing!=null)install(existing);
        }
        for(String name:new String[]{"java.util.Random"}) {
            MethodExitRequest request=vm.eventRequestManager().createMethodExitRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);clockRequests.add(request);
        }
        for(String name:new String[]{"gz","ii","ls","uw","bg","ar","fd","lm","li","SpawningTarget$FailureWorld"}) {
            ExceptionRequest request=vm.eventRequestManager().createExceptionRequest(null,true,true);request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
        }
        boolean alive=true;
        while(alive) {
            EventSet events=vm.eventQueue().remove(60000);if(events==null)throw new IllegalStateException("event timeout");
            for(Event event:events) {
                if(event instanceof ClassPrepareEvent)install(((ClassPrepareEvent)event).referenceType());
                else if(event instanceof ModificationWatchpointEvent) {
                    boolean enabled=((IntegerValue)((ModificationWatchpointEvent)event).valueToBe()).value()>=0;
                    for(MethodExitRequest request:clockRequests)request.setEnabled(enabled);
                }
                else if(event instanceof BreakpointEvent&&active()>=0) {
                    BreakpointEvent point=(BreakpointEvent)event;
                    output.println(ConstructorJson.json(snapshot("checkpoint",point.location())));output.flush();
                } else if(event instanceof ExceptionEvent&&active()>=0) {
                    ExceptionEvent failure=(ExceptionEvent)event;
                    Map<String,Object> row=snapshot("source_exception",failure.location());
                    row.put("exception",failure.exception().referenceType().name());output.println(ConstructorJson.json(row));output.flush();
                } else if(event instanceof MethodExitEvent&&active()>=0) {
                    MethodExitEvent exit=(MethodExitEvent)event;
                    if(exit.method().name().equals("nextInt")&&exit.method().signature().equals("(I)I")) {
                        ObjectReference random=exit.thread().frame(0).thisObject();
                        ObjectReference receiver=(ObjectReference)stat("SpawningTarget","actor");
                        output.println(ConstructorJson.json(ConstructorJson.map("event","random","case",active(),"own",Objects.equals(random,object(receiver,"sn","bs")),"bound",value(exit.thread().frame(0).getArgumentValues().get(0)),"result",value(exit.returnValue()),"seed",String.format(Locale.ROOT,"%012x",atomic(object(random,"java.util.Random","seed"))))));output.flush();
                    }
                } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
            }
            try{events.resume();}catch(VMDisconnectedException expected){alive=false;}
        }
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=4) throw new IllegalArgumentException("jar helper-classes output-directory jdk-home");
        Path directory=Path.of(args[2]).toAbsolutePath(); Files.createDirectories(directory);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector(); Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[3]); options.get("main").setValue("SpawningTarget target-observations.ndjson \""+Path.of(args[0]).toAbsolutePath()+"\"");
        String classpath=Path.of(args[1]).toAbsolutePath()+File.pathSeparator+Path.of(args[0]).toAbsolutePath();
        options.get("options").setValue("-Xint -Xmx256m -Xverify:all --add-opens=java.base/java.lang=ALL-UNNAMED --add-opens=java.base/java.util=ALL-UNNAMED -Dspawn.probe.classes=\""+directory.resolve("classes.txt")+"\" -Xlog:class+init=info:file=target-class-init.log -classpath \""+classpath+"\""); options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options); Process process=vm.process();
        java.util.concurrent.atomic.AtomicReference<IOException> streamError=new java.util.concurrent.atomic.AtomicReference<>();
        Thread stdout=new Thread(()->{try(OutputStream sink=Files.newOutputStream(directory.resolve("target.stdout"))) { process.getInputStream().transferTo(sink); } catch(IOException exception) { streamError.compareAndSet(null,exception); }});
        Thread stderr=new Thread(()->{try(OutputStream sink=Files.newOutputStream(directory.resolve("target.stderr"))) { process.getErrorStream().transferTo(sink); } catch(IOException exception) { streamError.compareAndSet(null,exception); }});
        stdout.start(); stderr.start();
        boolean observed=false;
        try(PrintWriter output=new PrintWriter(Files.newBufferedWriter(directory.resolve("checkpoints.ndjson"),StandardCharsets.UTF_8))) {
            new SpawningJdi(vm,output).run();
            if(output.checkError()) throw new IOException("checkpoint output");
            observed=true;
        }
        finally { try { vm.dispose(); } catch(VMDisconnectedException expected) {} if(!observed) process.destroyForcibly(); }
        if(!process.waitFor(10,java.util.concurrent.TimeUnit.SECONDS)) {
            process.destroyForcibly(); process.waitFor(); throw new IllegalStateException("target exit timeout");
        }
        int code=process.exitValue(); stdout.join(); stderr.join();
        if(streamError.get()!=null) throw streamError.get();
        if(code!=0) throw new IllegalStateException("target exited "+code);
    }
}
