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

/** Read-only entry/return observations; no remote invocation or target mutation. */
public final class BedSpawnJdi {
    final VirtualMachine vm;final PrintWriter output;
    final List<EventRequest> calls=new ArrayList<>();
    final List<Map<String,Object>> journal=new ArrayList<>();
    final Set<String> pins=new HashSet<>();
    final Map<String,Map<String,Object>> pending=new HashMap<>();
    int active=-1,rows;long bytes;
    BedSpawnJdi(VirtualMachine value,PrintWriter writer){vm=value;output=writer;}
    static Map<String,Object> map(Object... values){return BedSpawnObservation.map(values);}
    void emit(Map<String,Object> value){String text=BedSpawnObservation.json(value);bytes+=text.length()+1;if(bytes>4L*1024*1024)throw new IllegalStateException("JDI journal bound");output.println(text);output.flush();if(output.checkError())throw new IllegalStateException("JDI journal output");}
    void pin(Method method)throws Exception {
        String key=method.declaringType().name()+"."+method.name()+method.signature();if(!pins.add(key))return;
        byte[] code=method.bytecodes();emit(map("event","code","class",method.declaringType().name(),"method",method.name(),"descriptor",method.signature(),"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));
    }
    void begin(int next){
        if(active>=0){if(!pending.isEmpty())throw new IllegalStateException("unfinished selected query");emit(map("event","journal","case",active,"queries",new ArrayList<>(journal)));++rows;}
        active=next;journal.clear();for(EventRequest request:calls)request.setEnabled(next>=0);
    }
    Map<String,Object> selected(Method method,ThreadReference thread)throws Exception {
        if(thread.frameCount()<2)return null;Location caller=thread.frame(1).location();String owner=caller.declaringType().name();
        String signature=caller.method().signature(),name=method.name(),declaring=method.declaringType().name();
        int pc=(int)caller.codeIndex();String kind=null;
        if(owner.equals("gs")&&caller.method().name().equals("a")&&signature.equals("(Lfd;Lbr;)Lbr;")){
            if(declaring.equals("ok")&&name.equals("c")&&method.signature().equals("(II)Llm;")&&Set.of(22,45,68,91).contains(pc))kind="CAPTURED_ACQUIRE";
            if(declaring.equals("fd")&&name.equals("a")&&method.signature().equals("(III)I")&&pc==110)kind="BLOCK";
        }
        if(owner.equals("ve")&&caller.method().name().equals("f")&&signature.equals("(Lfd;IIII)Lbr;")&&declaring.equals("fd")){
            if(name.equals("e")&&method.signature().equals("(III)I")&&pc==4)kind="METADATA";
            if(name.equals("h")&&method.signature().equals("(III)Z")&&pc==101)kind="SUPPORT";
            if(name.equals("d")&&method.signature().equals("(III)Z")&&(pc==113||pc==127))kind="AIR";
        }
        if(kind==null)return null;
        List<Value> args=thread.frame(0).getArgumentValues();int x=((IntegerValue)args.get(0)).value();
        int y=kind.equals("CAPTURED_ACQUIRE")?0:((IntegerValue)args.get(1)).value();
        int z=((IntegerValue)args.get(kind.equals("CAPTURED_ACQUIRE")?1:2)).value();
        return map("kind",kind,"pc",pc,"x",x,"y",y,"z",z,"completed",false,"result",null);
    }
    String key(Method method,ThreadReference thread)throws Exception{return thread.uniqueID()+":"+thread.frameCount()+":"+method.declaringType().name()+"."+method.name()+method.signature();}
    void run()throws Exception {
        EventRequestManager manager=vm.eventRequestManager();
        for(String name:new String[]{"fd","ok","ve","gs"}){
            MethodEntryRequest entry=manager.createMethodEntryRequest();entry.addClassFilter(name);entry.setSuspendPolicy(EventRequest.SUSPEND_ALL);calls.add(entry);
            MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);calls.add(exit);
        }
        ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("BedSpawnObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
        boolean alive=true;
        while(alive){EventSet set=vm.eventQueue().remove(30000);if(set==null)throw new IllegalStateException("JDI event timeout");for(Event event:set){
            if(event instanceof ClassPrepareEvent){ModificationWatchpointRequest request=manager.createModificationWatchpointRequest(((ClassPrepareEvent)event).referenceType().fieldByName("active"));request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
            else if(event instanceof ModificationWatchpointEvent)begin(((IntegerValue)((ModificationWatchpointEvent)event).valueToBe()).value());
            else if(event instanceof MethodEntryEvent&&active>=0){MethodEntryEvent entry=(MethodEntryEvent)event;Map<String,Object> query=selected(entry.method(),entry.thread());String owner=entry.method().declaringType().name();
                if(query!=null){pin(entry.method());if(journal.size()>=64)throw new IllegalStateException("declared journal extent");journal.add(query);if(pending.put(key(entry.method(),entry.thread()),query)!=null)throw new IllegalStateException("duplicate query frame");}
                else if((owner.equals("gs")&&entry.method().name().equals("a")&&entry.method().signature().equals("(Lfd;Lbr;)Lbr;"))||(owner.equals("ve")&&entry.method().name().equals("f")&&entry.method().signature().equals("(Lfd;IIII)Lbr;")))pin(entry.method());
            }
            else if(event instanceof MethodExitEvent&&active>=0){MethodExitEvent exit=(MethodExitEvent)event;Map<String,Object> query=pending.remove(key(exit.method(),exit.thread()));if(query!=null){Value value=exit.returnValue();int result=query.get("kind").equals("CAPTURED_ACQUIRE")?0:value instanceof BooleanValue?(((BooleanValue)value).value()?1:0):((IntegerValue)value).value();query.put("completed",true);query.put("result",result);}}
            else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException done){alive=false;}}
        if(rows!=30||active!=-1)throw new IllegalStateException("complete declared JDI rows");
    }
    public static void main(String[] args)throws Exception {
        if(args.length!=8)throw new IllegalArgumentException("jar work profiles names binding output jdk-home classpath");
        Path output=Path.of(args[5]).toAbsolutePath();Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();String quote="\"";
        options.get("home").setValue(args[6]);StringJoiner main=new StringJoiner(" ");main.add("BedSpawnObservation");for(int i=0;i<5;++i)main.add(quote+args[i]+quote);
        options.get("main").setValue(main.toString());options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+quote+args[7]+quote);options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options);Process process=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread stdout=drain(process.getInputStream(),output.resolve("target.stdout"),process,failure),stderr=drain(process.getErrorStream(),output.resolve("target.stderr"),process,failure);stdout.start();stderr.start();boolean completed=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("queries.jsonl"),StandardCharsets.UTF_8))){new BedSpawnJdi(vm,writer).run();completed=true;}
        finally{if(!completed){try{vm.exit(1);}catch(VMDisconnectedException done){}process.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException done){}}
        process.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||process.exitValue()!=0)throw new IllegalStateException("JDI target/drain failure",failure.get());
        System.out.println("Read-only Bed spawn JDI: 30 journals");
    }
    static Thread drain(InputStream stream,Path path,Process process,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream output=Files.newOutputStream(path)){byte[] buffer=new byte[8192];long bytes=0;for(int count;(count=stream.read(buffer))>=0;){bytes+=count;if(bytes>4L*1024*1024)throw new IOException("target stream bound");output.write(buffer,0,count);}}catch(Throwable error){failure.compareAndSet(null,error);process.destroyForcibly();}});}
}
