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

/** Passive prefix observer. It never invokes, writes or returns a target method. */
public final class PlayerSleepParentJdi {
    final VirtualMachine vm;final PrintWriter out,control;
    final List<EventRequest> requests=new ArrayList<>();final Set<String> pins=new HashSet<>();
    final List<Map<String,Object>> calls=new ArrayList<>(),events=new ArrayList<>();
    final Map<String,Map<String,Object>> pending=new LinkedHashMap<>();final Map<Long,String> labels=new HashMap<>();
    final List<ThreadReference> parked=new ArrayList<>();
    final List<Object> parkedSites=new ArrayList<>();
    ReferenceType helper;ObjectReference actor,world,bounds,worker;long activeThread=-1;
    int active=-1,rows,clock,fresh;long bytes;
    PlayerSleepParentJdi(VirtualMachine v,PrintWriter o,PrintWriter c){vm=v;out=o;control=c;}
    static Map<String,Object> map(Object...x){return PlayerSleepParentObservation.map(x);}
    void emit(Object v){String s=PlayerSleepParentObservation.json(v);bytes+=s.length()+1;if(bytes>24L*1024*1024)throw new IllegalStateException("journal stream bound");out.println(s);out.flush();if(out.checkError())throw new IllegalStateException("journal output");}
    void event(Map<String,Object> v){if(events.size()>=4096)throw new IllegalStateException("event bound");v.put("tick",clock++);events.add(v);}
    static Object site(Location l){return map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",(int)l.codeIndex());}
    void pin(Method m)throws Exception{String key=m.declaringType().name()+"."+m.name()+m.signature();if(!pins.add(key))return;byte[] b=m.bytecodes();emit(map("event","code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b))));}
    void label(Value v,String name){if(v instanceof ObjectReference)labels.putIfAbsent(((ObjectReference)v).uniqueID(),name);}
    Object value(Value v)throws Exception{
        if(v==null||v instanceof VoidValue)return null;
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof ByteValue)return (int)((ByteValue)v).value();
        if(v instanceof ShortValue)return (int)((ShortValue)v).value();
        if(v instanceof CharValue)return (int)((CharValue)v).value();
        if(v instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));
        if(v instanceof FloatValue)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof StringReference){List<Integer> units=new ArrayList<>();for(char c:((StringReference)v).value().toCharArray())units.add((int)c);return map("utf16",units);}
        ObjectReference r=(ObjectReference)v;String type=r.referenceType().name(),name=labels.get(r.uniqueID());
        if(name==null){name=type+"-"+fresh++;labels.put(r.uniqueID(),name);}
        return map("class",type,"identity",name,"raw_object_id",r.uniqueID());
    }
    String key(Method m,ThreadReference t)throws Exception{return t.uniqueID()+":"+t.frameCount()+":"+m.declaringType().name()+m.name()+m.signature();}
    boolean current(ThreadReference t){return active>=0&&t.uniqueID()==activeThread;}
    void begin(int number,ThreadReference thread)throws Exception{
        if(active!=-1||number!=rows||number>=48)throw new IllegalStateException("sequential bounded worker");
        active=number;activeThread=thread.uniqueID();calls.clear();events.clear();pending.clear();labels.clear();clock=fresh=0;
        actor=(ObjectReference)helper.getValue(helper.fieldByName("actor"));world=(ObjectReference)helper.getValue(helper.fieldByName("world"));bounds=(ObjectReference)helper.getValue(helper.fieldByName("originalBounds"));worker=(ObjectReference)helper.getValue(helper.fieldByName("worker"));
        if(worker.uniqueID()!=thread.uniqueID()||!thread.name().equals("original-gs-parent-"+number))throw new IllegalStateException("actual source-worker identity");
        label(actor,"actor");label(world,"world");label(bounds,"initial-bounds");label(worker,"source-worker");
        label(helper.getValue(helper.fieldByName("beforeV")),"initial-v");label(helper.getValue(helper.fieldByName("beforeQ")),"initial-Q");label(helper.getValue(helper.fieldByName("beforeBN")),"initial-private-bN");label(helper.getValue(helper.fieldByName("beforeList")),"initial-player-list");label(helper.getValue(helper.fieldByName("beforeT")),"initial-Block-T");
        for(EventRequest request:requests)request.enable();
    }
    void terminal(ThreadReference thread,Location location,String outcome,String exception)throws Exception{
        if(!current(thread)||!thread.isSuspended()||thread.suspendCount()<1)throw new IllegalStateException("actual parked source frame");
        if(outcome.equals("PREFIX111")){
            if(!location.declaringType().name().equals("gs")||!location.method().name().equals("w_")||location.codeIndex()!=111||pending.size()!=1)throw new IllegalStateException("exact original continuation111");
            Map<String,Object> root=calls.get(0);if(root.get("completed")!=Boolean.FALSE||!root.get("site").equals(site(location.method().location())))throw new IllegalStateException("unreturned original root");
            event(map("kind","PREFIX","site",site(location)));
        }else{
            for(Map<String,Object> call:pending.values())call.put("exception",exception);
            event(map("kind","EXCEPTION","site",site(location),"class",exception));
        }
        List<Object> stack=new ArrayList<>();for(StackFrame frame:thread.frames())stack.add(site(frame.location()));
        emit(map("event","journal","case",active,"outcome",outcome,"terminal_site",site(location),"exception",exception,
                 "source_worker",map("name",thread.name(),"class",thread.referenceType().name(),"raw_object_id",thread.uniqueID(),"suspended",thread.isSuspended(),"suspend_count",thread.suspendCount(),"daemon",value(thread.getValue(vm.classesByName("java.lang.Thread").get(0).fieldByName("daemon")))),
                 "source_receiver",value(thread.frame(0).thisObject()),"stack",stack,"calls",new ArrayList<>(calls),"events",new ArrayList<>(events),"root_returned",false,"terminal_resumed",false));
        parked.add(thread);parkedSites.add(site(location));int finished=active;active=-1;activeThread=-1;++rows;
        for(EventRequest request:requests)request.disable();
        control.println("SNAPSHOT "+finished);control.flush();if(control.checkError())throw new IllegalStateException("target control pipe");
    }
    void prepare(ReferenceType type)throws Exception{
        EventRequestManager m=vm.eventRequestManager();String name=type.name();
        if(name.equals("PlayerSleepParentObservation")){
            helper=type;for(String nameOfField:new String[]{"active","finished"}){ModificationWatchpointRequest w=m.createModificationWatchpointRequest(type.fieldByName(nameOfField));w.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);w.enable();}return;
        }
        Map<String,Set<String>> watched=Map.of(
            "sn",Set.of("aI","aM","aN","aO","aP","aQ","aR","aW","bf","bg","bh"),
            "gs",Set.of("u","a","v","w","y","b","bN"),"fd",Set.of("J","B","f","d"),
            "br",Set.of("a","b","c"),"eq",Set.of("a","b","c","d","e","f"),"uu",Set.of("T","bn"));
        for(Field field:type.fields())if(watched.getOrDefault(name,Set.of()).contains(field.name())){
            ModificationWatchpointRequest w=m.createModificationWatchpointRequest(field);w.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(w);
            AccessWatchpointRequest a=m.createAccessWatchpointRequest(field);a.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(a);
            if(active>=0){w.enable();a.enable();}
        }
        if(name.equals("gs")){
            Method method=type.methodsByName("w_","()V").get(0);pin(method);
            BreakpointRequest b=m.createBreakpointRequest(method.locationOfCodeIndex(111));b.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(b);if(active>=0)b.enable();
        }
        if(name.equals("ls"))pin(type.methodsByName("w_","()V").get(0));
        if(name.equals("dc"))pin(type.methodsByName("o","()V").get(0));
    }
    void run(int expected)throws Exception{
        EventRequestManager m=vm.eventRequestManager();
        for(String n:new String[]{"PlayerSleepParentObservation","sn","ls","gs","dc","fd","br","eq","uu"}){ClassPrepareRequest p=m.createClassPrepareRequest();p.addClassFilter(n);p.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);p.enable();}
        for(String n:new String[]{"sn","ls","gs","dc","fd","ve","br","eq","cw","lm","ok","uu"}){
            MethodEntryRequest e=m.createMethodEntryRequest();e.addClassFilter(n);e.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(e);
            MethodExitRequest x=m.createMethodExitRequest();x.addClassFilter(n);x.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(x);
        }
        ExceptionRequest exceptions=m.createExceptionRequest(null,true,true);exceptions.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(exceptions);
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(30000);if(set==null)throw new IllegalStateException("source-event deadline");boolean resume=true;
            for(Event e:set){
                if(e instanceof ClassPrepareEvent)prepare(((ClassPrepareEvent)e).referenceType());
                else if(e instanceof ModificationWatchpointEvent){ModificationWatchpointEvent w=(ModificationWatchpointEvent)e;String owner=w.field().declaringType().name();
                    if(owner.equals("PlayerSleepParentObservation")){int next=((IntegerValue)w.valueToBe()).value();if(next<0){if(next!=-1||active!=-1||rows!=0)throw new IllegalStateException("helper initialization only");}
                        else if(w.field().name().equals("active"))begin(next,w.thread());
                        else{
                            if(next!=expected||rows!=expected||active!=-1||parked.size()!=expected)throw new IllegalStateException("complete parked source prefixes");
                            List<Object> verified=new ArrayList<>();
                            for(int i=0;i<parked.size();++i){ThreadReference thread=parked.get(i);if(!thread.isSuspended()||thread.suspendCount()<1||!site(thread.frame(0).location()).equals(parkedSites.get(i)))throw new IllegalStateException("source frame moved before planned termination");verified.add(map("case",i,"site",site(thread.frame(0).location()),"suspended",true,"suspend_count",thread.suspendCount(),"raw_worker_id",thread.uniqueID()));}
                            emit(map("event","complete","profiles",rows,"parked_workers",parked.size(),"verified_parked",verified,"source_returns",0,"terminal_resumes",0,"jdi_invocations",0,"jdi_writes",0,"jdi_forced_returns",0,"planned_target_termination","JDWP_VM_EXIT_0","vm_exit_requests",1));
                            alive=false;resume=false;try{vm.exit(0);}catch(VMDisconnectedException expectedExit){}
                        }
                    }
                    else if(current(w.thread()))event(map("kind","STORE","site",site(w.location()),"owner",owner,"field",w.field().name(),"receiver",value(w.object()),"before",value(w.valueCurrent()),"value",value(w.valueToBe())));
                }else if(e instanceof AccessWatchpointEvent){AccessWatchpointEvent a=(AccessWatchpointEvent)e;if(current(a.thread()))event(map("kind","FIELD_READ","site",site(a.location()),"owner",a.field().declaringType().name(),"field",a.field().name(),"receiver",value(a.object()),"value",value(a.valueCurrent())));
                }else if(e instanceof MethodEntryEvent){MethodEntryEvent in=(MethodEntryEvent)e;if(current(in.thread())){
                    pin(in.method());if(calls.isEmpty()){Location caller=in.thread().frame(1).location();pin(caller.method());if(!in.method().declaringType().name().equals("gs")||!in.method().name().equals("w_"))throw new IllegalStateException("actual original root entry");}
                    List<Object> arguments=new ArrayList<>();for(Value v:in.thread().frame(0).getArgumentValues())arguments.add(value(v));
                    Map<String,Object> call=map("id",calls.size(),"caller",site(in.thread().frame(1).location()),"site",site(in.location()),"receiver",value(in.thread().frame(0).thisObject()),"arguments",arguments,"entered",clock,"returned",null,"completed",false,"exception",null,"result",null);
                    if(calls.size()>=512||pending.put(key(in.method(),in.thread()),call)!=null)throw new IllegalStateException("source call extent");calls.add(call);event(map("kind","ENTER","call",call.get("id")));
                }}else if(e instanceof MethodExitEvent){MethodExitEvent x=(MethodExitEvent)e;if(current(x.thread())){Map<String,Object> call=pending.remove(key(x.method(),x.thread()));if(call!=null){call.put("returned",clock);call.put("completed",true);call.put("result",value(x.returnValue()));event(map("kind","RETURN","call",call.get("id")));}}}
                else if(e instanceof BreakpointEvent){BreakpointEvent b=(BreakpointEvent)e;if(current(b.thread())){terminal(b.thread(),b.location(),"PREFIX111",null);resume=false;}}
                else if(e instanceof ExceptionEvent){ExceptionEvent x=(ExceptionEvent)e;if(current(x.thread())){String name=x.exception().referenceType().name().replace('.','/');terminal(x.thread(),x.location(),"SOURCE_EXCEPTION",name);resume=false;}}
                else if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)throw new IllegalStateException("unexpected target termination before explicit parked-frame shutdown");
            }
            if(resume)try{set.resume();}catch(VMDisconnectedException done){alive=false;}
        }
        if(rows!=expected||active!=-1||parked.size()!=expected)throw new IllegalStateException("complete parked source prefixes "+rows);
    }
    public static void main(String[] a)throws Exception{
        if(a.length!=7)throw new IllegalArgumentException("jdk classpath jar profiles classnames output count");Path output=Path.of(a[5]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();String q="\"";
        options.get("home").setValue(a[0]);options.get("main").setValue("PlayerSleepParentObservation "+q+a[2]+q+" "+q+a[3]+q+" "+q+a[4]+q);
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m -Xss256k -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+q+a[1]+q);options.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(options);Process process=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread stdout=drain(process.getInputStream(),output.resolve("target.stdout"),process,failure),stderr=drain(process.getErrorStream(),output.resolve("target.stderr"),process,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8));PrintWriter control=new PrintWriter(new OutputStreamWriter(process.getOutputStream(),StandardCharsets.US_ASCII))){new PlayerSleepParentJdi(vm,journal,control).run(Integer.parseInt(a[6]));done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}process.destroyForcibly();}}
        process.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||process.exitValue()!=0)throw new IllegalStateException("target/drain failure",failure.get());
        System.out.println("Passive original GS parent prefixes: "+a[6]+" verified parked workers; planned JDWP VM exit0; no source parent return");
    }
    static Thread drain(InputStream in,Path path,Process process,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream out=Files.newOutputStream(path)){byte[] bytes=new byte[8192];long count=0;for(int n;(n=in.read(bytes))>=0;){count+=n;if(count>24L*1024*1024)throw new IOException("pipe bound");out.write(bytes,0,n);}}catch(Throwable error){failure.compareAndSet(null,error);process.destroyForcibly();}});}
}
