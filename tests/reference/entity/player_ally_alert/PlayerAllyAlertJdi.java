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

/** Read-only live Code, actual source entry/return order and first exceptions. */
public final class PlayerAllyAlertJdi {
    final VirtualMachine vm;
    final PrintWriter out;
    final List<EventRequest> requests=new ArrayList<>();
    final List<Object> events=new ArrayList<>();
    final Set<String> pinned=new HashSet<>();
    final Map<Long,String> identities=new HashMap<>();
    ReferenceType helper;
    int active=-1,completed,fresh,sequence;
    long bytes;
    static final Map<String,Set<String>> SELECTED=Map.ofEntries(
        Map.entry("gs",Set.of("a(Lsn;I)Z","a(Lls;Z)V","F()Z","b(I)V")),
        Map.entry("dc",Set.of("a(Lvr;I)V")),
        Map.entry("ls",Set.of("a(Lsn;I)Z","a(Lsn;IDD)V","b(I)V","ai()V","j_()Ljava/lang/String;","k()F")),
        Map.entry("gi",Set.of("D()Z","A()Ljava/lang/String;","B()Z","b(Z)V")),
        Map.entry("ii",Set.of("G()Lsn;","c(Lsn;)V")),
        Map.entry("ud",Set.of("a(I)B","c(I)Ljava/lang/String;","b(ILjava/lang/Object;)V")),
        Map.entry("ma",Set.of("b()Ljava/lang/Object;","a(Ljava/lang/Object;)V","a(Z)V")),
        Map.entry("fd",Set.of("a(Ljava/lang/Class;Leq;)Ljava/util/List;","a(Lsn;Ljava/lang/String;FF)V","a(Lsn;B)V")),
        Map.entry("lm",Set.of("a(Ljava/lang/Class;Leq;Ljava/util/List;)V")),
        Map.entry("eq",Set.of("b(DDDDDD)Leq;","b(DDD)Leq;")),
        Map.entry("xi",Set.of("a(Lvr;I)V")));
    PlayerAllyAlertJdi(VirtualMachine vm,PrintWriter out){this.vm=vm;this.out=out;}
    static Map<String,Object> map(Object... values){return ConstructorJson.map(values);}
    static Object site(Location value){return map("class",value.declaringType().name(),"method",value.method().name(),"descriptor",value.method().signature(),"pc",value.codeIndex());}
    static void need(boolean value,String message){if(!value)throw new IllegalStateException(message);}
    static Field find(ObjectReference value,String owner,String name){for(Field field:value.referenceType().allFields())if(field.declaringType().name().equals(owner)&&field.name().equals(name))return field;return null;}
    static Value field(ObjectReference value,String owner,String name){if(value==null)return null;Field field=find(value,owner,name);need(field!=null,"actual field "+owner+"."+name);return value.getValue(field);}
    String identity(ObjectReference value){return identities.computeIfAbsent(value.uniqueID(),key->"reference-"+fresh++);}
    Object scalar(Value input){
        if(input==null||input instanceof VoidValue)return null;
        if(input instanceof BooleanValue)return((BooleanValue)input).value();
        if(input instanceof ByteValue)return(int)((ByteValue)input).value();if(input instanceof ShortValue)return(int)((ShortValue)input).value();
        if(input instanceof IntegerValue)return((IntegerValue)input).value();
        if(input instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)input).value()));
        if(input instanceof FloatValue)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)input).value())));
        if(input instanceof DoubleValue)return map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)input).value())));
        if(input instanceof StringReference){List<Integer> result=new ArrayList<>();for(char c:((StringReference)input).value().toCharArray())result.add((int)c);return map("utf16",result);}
        ObjectReference value=(ObjectReference)input;String type=value.referenceType().name();
        if(value instanceof ClassObjectReference)return map("class_literal",((ClassObjectReference)value).reflectedType().name());
        Field id=find(value,"sn","aD");if(id!=null)return map("class",type,"entity",((IntegerValue)value.getValue(id)).value());
        if(type.equals("eq")){List<Object> bounds=new ArrayList<>();for(String name:new String[]{"a","b","c","d","e","f"})bounds.add(scalar(field(value,"eq",name)));return map("class",type,"bounds",bounds,"identity",identity(value));}
        if(type.equals("java.util.ArrayList")){
            int size=((IntegerValue)field(value,"java.util.ArrayList","size")).value();
            ArrayReference storage=(ArrayReference)field(value,"java.util.ArrayList","elementData");
            List<Object> members=new ArrayList<>();for(Value member:storage.getValues(0,size))members.add(scalar(member));
            return map("class",type,"identity",identity(value),"members",members);}
        Field statistic=find(value,"vr","e");if(statistic!=null)return map("class",type,"statistic",scalar(value.getValue(statistic)));
        if(type.equals("java.lang.Byte"))return map("boxed_byte",scalar(field(value,"java.lang.Byte","value")));
        if(type.equals("java.lang.Integer"))return map("boxed_int",scalar(field(value,"java.lang.Integer","value")));
        return map("class",type,"identity",identity(value));
    }
    void emit(Object value){String text=ConstructorJson.json(value);bytes+=text.length()+1;need(bytes<=32L*1024*1024,"bounded journal");out.println(text);out.flush();need(!out.checkError(),"complete journal write");}
    void event(Object value){need(events.size()<32768,"bounded entered call events");events.add(map("sequence",sequence++,"value",value));}
    boolean selected(Method method){return SELECTED.getOrDefault(method.declaringType().name(),Set.of()).contains(method.name()+method.signature());}
    void pin(Method method)throws Exception{
        String key=method.declaringType().name()+"."+method.name()+method.signature();if(!pinned.add(key))return;
        byte[] code=method.bytecodes();emit(map("event","code","class",method.declaringType().name(),"method",method.name(),
            "descriptor",method.signature(),"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));
    }
    void begin(int next){
        if(active>=0){emit(map("event","call","case",active,"events",new ArrayList<>(events)));++completed;}
        active=next;events.clear();identities.clear();fresh=0;sequence=0;for(EventRequest request:requests)request.setEnabled(next>=0);
    }
    void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();ClassPrepareRequest prepare=manager.createClassPrepareRequest();
        prepare.addClassFilter("PlayerAllyAlertObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
        for(String name:SELECTED.keySet()){
            MethodEntryRequest entry=manager.createMethodEntryRequest();entry.addClassFilter(name);entry.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(entry);
            MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exit);}
        ExceptionRequest exceptions=manager.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exceptions);
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(60000);need(set!=null,"live target event deadline");
            for(Event event:set){
                if(event instanceof ClassPrepareEvent){helper=((ClassPrepareEvent)event).referenceType();
                    ModificationWatchpointRequest watch=manager.createModificationWatchpointRequest(helper.fieldByName("active"));watch.setSuspendPolicy(EventRequest.SUSPEND_ALL);watch.enable();}
                else if(event instanceof ModificationWatchpointEvent)begin(((IntegerValue)((ModificationWatchpointEvent)event).valueToBe()).value());
                else if(active>=0&&event instanceof MethodEntryEvent){MethodEntryEvent entry=(MethodEntryEvent)event;if(!selected(entry.method()))continue;pin(entry.method());
                    StackFrame frame=entry.thread().frame(0);List<Object> args=new ArrayList<>();for(Value value:frame.getArgumentValues())args.add(scalar(value));
                    event(map("kind","ENTRY","site",site(entry.location()),"receiver",scalar(frame.thisObject()),"arguments",args));}
                else if(active>=0&&event instanceof MethodExitEvent){MethodExitEvent exit=(MethodExitEvent)event;if(selected(exit.method()))
                    event(map("kind","RETURN","site",site(exit.location()),"result",scalar(exit.returnValue())));}
                else if(active>=0&&event instanceof ExceptionEvent){ExceptionEvent failure=(ExceptionEvent)event;
                    event(map("kind","EXCEPTION","site",site(failure.location()),"class",failure.exception().referenceType().name().replace('.','/'),
                        "message",scalar(field(failure.exception(),"java.lang.Throwable","detailMessage")),"catch",failure.catchLocation()==null?null:site(failure.catchLocation())));}
                else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
            }try{set.resume();}catch(VMDisconnectedException ended){alive=false;}}
        need(completed==40&&active==-1,"all complete entered source calls");
    }
    static Thread drain(InputStream input,Path output,Process target,AtomicReference<Throwable> failure){return new Thread(()->{
        try(OutputStream stream=Files.newOutputStream(output)){long total=0;byte[] block=new byte[8192];for(int count;(count=input.read(block))>=0;){total+=count;need(total<=16L*1024*1024,"bounded target stream");stream.write(block,0,count);}}
        catch(Throwable error){failure.compareAndSet(null,error);target.destroyForcibly();}});}
    public static void main(String[] args)throws Exception{
        need(args.length==7,"jdk classpath jar work profiles classes output");Path output=Path.of(args[6]);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();String quote="\"";
        options.get("home").setValue(args[0]);options.get("main").setValue("PlayerAllyAlertObservation "+quote+args[2]+quote+" "+quote+args[3]+quote+" "+quote+args[4]+quote+" "+quote+args[5]+quote);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+quote+args[1]+quote);
        options.get("suspend").setValue("true");VirtualMachine vm=connector.launch(options);Process target=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread stdout=drain(target.getInputStream(),output.resolve("target.stdout"),target,failure),stderr=drain(target.getErrorStream(),output.resolve("target.stderr"),target,failure);
        stdout.start();stderr.start();boolean done=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8))){new PlayerAllyAlertJdi(vm,writer).run();done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}target.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        target.waitFor();stdout.join(5000);stderr.join(5000);need(!stdout.isAlive()&&!stderr.isAlive()&&failure.get()==null&&target.exitValue()==0,"complete target and stream drains");
        System.out.println("Read-only original ally alert: 40 complete calls");
    }
}
