import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Observer-only source entries/returns, queried values, actual pool and stores. */
public final class FluidJdi {
    static VirtualMachine vm;static ReferenceType helperType,btType;static boolean tracing;static int profile=-1,sequence,nextCall,queries,reads,stores,failures;
    static PrintWriter journal;static long journalBytes;
    static final Set<String> SOURCE=Set.of("cv","ew","fd","lm","nw","uu","rp","om","nx","ln","lu","sg","aq","ou","mx","ge","xp","bt","in","java.lang.Math");
    static final Set<String> CLASSES=Set.of("ew","cv","nw","uu","rp","bt","in","ln","lu","sg","aq","ou","java.lang.Math");
    static final Set<String> pinned=new HashSet<>(),prepared=new HashSet<>();
    static final List<EventRequest> controlled=new ArrayList<>();
    static final Map<Long,Deque<Integer>> calls=new HashMap<>();
    static final Map<Long,String> vectors=new LinkedHashMap<>(),materials=new HashMap<>(),blocks=new HashMap<>();
    static List<Field> nwFields,vectorFields;static ObjectReference tess,poolList,actualCache;static ArrayReference registered;static Field poolCursorField,poolListField;
    static ReferenceType helper(){if(helperType==null)helperType=vm.classesByName("FluidObservation").get(0);return helperType;}
    static ObjectReference owner(String name){return (ObjectReference)helper().getValue(helper().fieldByName(name));}
    static String location(Location p){return p==null?"NONE":p.declaringType().name()+"."+p.method().name()+p.method().signature()+"@"+p.codeIndex();}
    static Map<String,Object> map(Object... pairs){Map<String,Object> out=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)out.put((String)pairs[i],pairs[i+1]);return out;}
    static String json(Object v){if(v==null)return "null";if(v instanceof String){String s=(String)v;return "\""+s.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";}if(v instanceof Map){StringJoiner s=new StringJoiner(",","{","}");for(Object e:((Map<?,?>)v).entrySet()){Map.Entry<?,?> p=(Map.Entry<?,?>)e;s.add(json(p.getKey())+":"+json(p.getValue()));}return s.toString();}if(v instanceof Iterable){StringJoiner s=new StringJoiner(",","[","]");for(Object x:(Iterable<?>)v)s.add(json(x));return s.toString();}return v.toString();}
    static String vectorId(ObjectReference v){return vectors.computeIfAbsent(v.uniqueID(),k->"v"+vectors.size());}
    static Object value(Value v){
        if(v==null)return null;if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof ByteValue)return (int)((ByteValue)v).value();if(v instanceof ShortValue)return (int)((ShortValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));if(v instanceof VoidValue)return "void";
        ObjectReference o=(ObjectReference)v;if(o.equals(tess))return "ref:observedTess";if(o.equals(actualCache))return "ref:observedCache";if(o.equals(poolList))return "ref:actualVectorList";
        String mat=materials.get(o.uniqueID());if(mat!=null)return mat;String b=blocks.get(o.uniqueID());if(b!=null)return b;
        if(o.referenceType().name().equals("bt"))return vectorId(o);
        Field id=o.referenceType().fieldByName("bn");if(id!=null){int n=((IntegerValue)o.getValue(id)).value();if(n<0||n>=registered.length()||!o.equals(registered.getValue(n)))throw new AssertionError("actual registered Block identity");String label="block:"+n;blocks.put(o.uniqueID(),label);return label;}
        if(o.referenceType().name().equals("cv"))return "ref:cv";throw new AssertionError("unclassified actual source reference "+o.referenceType().name());
    }
    static List<Object> arguments(StackFrame frame){List<Object> out=new ArrayList<>();for(Value v:frame.getArgumentValues())out.add(value(v));return out;}
    static List<Object> vector(ObjectReference o){if(vectorFields==null)vectorFields=List.of(o.referenceType().fieldByName("a"),o.referenceType().fieldByName("b"),o.referenceType().fieldByName("c"));Map<Field,Value> v=o.getValues(vectorFields);List<Object> out=new ArrayList<>();out.add(vectorId(o));for(Field f:vectorFields)out.add(value(v.get(f)));return out;}
    static Map<String,Object> pool(boolean full){
        Map<Field,Value> statics=btType.getValues(List.of(poolListField,poolCursorField));if(!poolList.equals(statics.get(poolListField)))throw new AssertionError("same genuine Vector List");Field size=poolList.referenceType().fieldByName("size"),data=poolList.referenceType().fieldByName("elementData");Map<Field,Value> fields=poolList.getValues(List.of(size,data));int n=((IntegerValue)fields.get(size)).value();ArrayReference a=(ArrayReference)fields.get(data);Map<String,Object> out=map("cursor",value(statics.get(poolCursorField)),"size",n,"class",poolList.referenceType().name());
        if(full){List<Object> entries=new ArrayList<>();for(Value v:a.getValues(0,n))entries.add(vector((ObjectReference)v));out.put("entries",entries);}return out;
    }
    static Map<String,Object> nw(){if(nwFields==null){nwFields=new ArrayList<>();for(Field f:tess.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))nwFields.add(f);}Map<Field,Value> actual=tess.getValues(nwFields);Map<String,Object> result=new TreeMap<>();for(Field f:nwFields)result.put(f.name(),value(actual.get(f)));return result;}
    static void bind(){if(tess!=null)return;tess=owner("observedTess");actualCache=owner("observedCache");poolList=owner("observedPool");registered=(ArrayReference)owner("observedRegistered");btType=vm.classesByName("bt").get(0);poolListField=btType.fieldByName("d");poolCursorField=btType.fieldByName("e");ArrayReference a=(ArrayReference)owner("observedMaterials"),names=(ArrayReference)owner("observedMaterialNames");List<Value> av=a.getValues(),nv=names.getValues();for(int i=0;i<av.size();++i)materials.putIfAbsent(((ObjectReference)av.get(i)).uniqueID(),"material:"+((StringReference)nv.get(i)).value());pool(true);}
    static void emit(String event,Object... pairs){Map<String,Object> out=map("seq",sequence++,"profile",profile,"event",event);out.putAll(map(pairs));String line=json(out);journalBytes+=line.length()+1;if(sequence>100000||journalBytes>67108864)throw new AssertionError("bounded trace");journal.println(line);if(journal.checkError())throw new AssertionError("journal write");}
    static void pin(Method m)throws Exception{String key=m.declaringType().name()+"."+m.name()+m.signature();if(pinned.add(key)){if(m.isNative())System.out.println("FW_LIVE_NATIVE\t"+m.declaringType().name()+"\t"+m.name()+"\t"+m.signature()+"\t"+m.modifiers());else{byte[] code=m.bytecodes();System.out.println("FW_LIVE_CODE\t"+m.declaringType().name()+"\t"+m.name()+"\t"+m.signature()+"\t"+code.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code)));}}}
    static boolean watched(Method m){String t=m.declaringType().name(),n=m.name(),s=m.signature();
        if(t.equals("ew"))return !n.startsWith("<");
        if(t.equals("cv"))return n.equals("k")&&s.equals("(Luu;III)Z")||n.equals("a")&&Set.of("(IIILln;)F","(Luu;DDDI)V").contains(s);
        if(t.equals("rp"))return Set.of("b(Lxp;III)I","b(Lxp;IIII)Z","d(Lxp;III)F","d(Lxp;IIII)Z","c(Lxp;III)I","e(Lxp;III)Lbt;","a(Lxp;IIILln;)D","a(I)I","d(I)F").contains(n+s);
        if(t.equals("uu"))return Set.of("a(II)I","b(Lxp;IIII)Z","d(Lxp;IIII)Z").contains(n+s);
        if(Set.of("ln","lu","sg","aq","ou").contains(t))return (n.equals("a")||n.equals("c"))&&s.equals("()Z");
        if(t.equals("bt"))return Set.of("a(DDD)Lbt;","b(DDD)Lbt;","c()Lbt;","c(DDD)Lbt;","e(DDD)Lbt;","<init>(DDD)V").contains(n+s);
        if(t.equals("in"))return Set.of("a(F)F","b(F)F","a(D)F").contains(n+s);
        if(t.equals("java.lang.Math"))return Set.of("sqrt(D)D","atan2(DD)D").contains(n+s);
        return t.equals("nw")&&n.equals("a")&&Set.of("(FFF)V","(FFFF)V","(IIII)V","(DDDDD)V","(DD)V","(DDD)V").contains(s);
    }
    static void control(EventRequest r){r.setSuspendPolicy(EventRequest.SUSPEND_ALL);controlled.add(r);if(tracing)r.enable();}
    static void prepare(EventRequestManager manager,ReferenceType type){if(!prepared.add(type.name()))return;
        if(type.name().equals("FluidObservation")){ModificationWatchpointRequest r=manager.createModificationWatchpointRequest(type.fieldByName("active"));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();return;}
        if(!CLASSES.contains(type.name()))return;
        for(Method m:type.methods())if(watched(m)&&!m.isNative())control(manager.createBreakpointRequest(m.location()));
        MethodExitRequest exits=manager.createMethodExitRequest();exits.addClassFilter(type.name());control(exits);
        if(type.name().equals("java.lang.Math")){MethodEntryRequest entries=manager.createMethodEntryRequest();entries.addClassFilter(type.name());control(entries);}
        for(Field f:type.fields()){
            boolean access=type.name().equals("nw")&&f.name().equals("a")||type.name().equals("uu")&&Set.of("B","D").contains(f.name())||type.name().equals("ln")&&Set.of("g","h","s").contains(f.name());
            boolean modification=type.name().equals("uu")&&Set.of("bt","bw").contains(f.name())||type.name().equals("bt")&&f.name().equals("e");
            if(access)control(manager.createAccessWatchpointRequest(f));if(modification)control(manager.createModificationWatchpointRequest(f));}
    }
    static void drain(Process child,InputStream in,Path path,int bound,List<Throwable> errors){try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];int total=0,n;while((n=in.read(b))!=-1){total+=n;if(total>bound){child.destroyForcibly();throw new IOException("pipe bound");}out.write(b,0,n);}}catch(Throwable t){synchronized(errors){errors.add(t);}}}
    public static void main(String[] args)throws Exception{
        if(args.length!=9)throw new AssertionError("output,jdk-home,classpath,jar,world,profiles,first,cohort,exceptions");Path output=Path.of(args[0]);int expectedFailures=Integer.parseInt(args[8]);LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();options.get("home").setValue(args[1]);options.get("main").setValue("FluidObservation \""+args[3]+"\" \""+args[4]+"\" \""+args[5]+"\" "+args[6]+" "+args[7]);options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -cp \""+args[2]+"\"");options.get("suspend").setValue("true");
        vm=connector.launch(options);if(!vm.canWatchFieldAccess()||!vm.canWatchFieldModification()||!vm.canGetBytecodes()||!vm.canGetMethodReturnValues())throw new AssertionError("required read-only JDI capabilities");Process child=vm.process();List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());Thread stdout=new Thread(()->drain(child,child.getInputStream(),output.resolve("target.stdout"),16777216,errors)),stderr=new Thread(()->drain(child,child.getErrorStream(),output.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();boolean died=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(150);journal=new PrintWriter(Files.newBufferedWriter(output.resolve("events.jsonl")));
        try{EventRequestManager manager=vm.eventRequestManager();for(String name:CLASSES){ClassPrepareRequest p=manager.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();for(ReferenceType t:vm.classesByName(name))if(t.isPrepared())prepare(manager,t);}ClassPrepareRequest hp=manager.createClassPrepareRequest();hp.addClassFilter("FluidObservation");hp.setSuspendPolicy(EventRequest.SUSPEND_ALL);hp.enable();
            for(String name:SOURCE){ExceptionRequest r=manager.createExceptionRequest(null,true,true);r.addClassFilter(name);control(r);}
            while(!died){if(System.nanoTime()>deadline)throw new IOException("observer deadline");EventSet set;try{set=vm.eventQueue().remove(1000);}catch(VMDisconnectedException end){break;}if(set==null)continue;
                for(Event event:set){if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){died=true;continue;}
                    if(event instanceof ModificationWatchpointEvent&&((ModificationWatchpointEvent)event).field().declaringType().name().equals("FluidObservation")){ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;int next=((IntegerValue)e.valueToBe()).value();if(next>=0){profile=next;bind();for(EventRequest r:controlled)r.enable();tracing=true;emit("profile-start","pool",pool(true),"nw",nw());}else if(tracing){for(EventRequest r:controlled)r.disable();emit("profile-end","pool",pool(true),"nw",nw());tracing=false;profile=-1;}continue;}
                    if(!tracing)continue;
                    if(event instanceof BreakpointEvent||event instanceof MethodEntryEvent){ThreadReference thread;Location loc;Method m;if(event instanceof BreakpointEvent){BreakpointEvent e=(BreakpointEvent)event;thread=e.thread();loc=e.location();m=loc.method();}else{MethodEntryEvent e=(MethodEntryEvent)event;thread=e.thread();loc=e.location();m=e.method();if(!m.isNative())continue;}if(!watched(m))continue;pin(m);StackFrame frame=thread.frame(0);ObjectReference receiver=frame.thisObject();int call=nextCall++;calls.computeIfAbsent(thread.uniqueID(),k->new ArrayDeque<>()).push(call);if(m.declaringType().name().equals("ew"))queries++;
                        Map<String,Object> extra=map("call",call,"at",location(loc),"caller",location(thread.frame(1).location()),"receiver",value(receiver),"native",m.isNative(),"args",m.isNative()?null:arguments(frame));if(m.isNative())extra.put("caller_args",arguments(thread.frame(1)));if(m.declaringType().name().equals("nw")||m.declaringType().name().equals("cv"))extra.put("nw",nw());if(m.declaringType().name().equals("bt")){extra.put("pool",pool(false));if(receiver!=null)extra.put("vector",vector(receiver));}emit("entry","data",extra);
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;Method m=e.method();if(!watched(m))continue;pin(m);Deque<Integer> stack=calls.get(e.thread().uniqueID());if(stack==null||stack.isEmpty())throw new AssertionError("unmatched source return");Map<String,Object> extra=map("call",stack.pop(),"at",location(e.location()),"result",value(e.returnValue()));String type=m.declaringType().name();if(type.equals("nw")||type.equals("cv"))extra.put("nw",nw());if(type.equals("bt")){extra.put("pool",pool(false));if(e.returnValue() instanceof ObjectReference)extra.put("vector",vector((ObjectReference)e.returnValue()));}emit("return","data",extra);
                    }else if(event instanceof AccessWatchpointEvent){AccessWatchpointEvent e=(AccessWatchpointEvent)event;pin(e.location().method());emit("field-read","at",location(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"value",value(e.valueCurrent()));reads++;
                    }else if(event instanceof ModificationWatchpointEvent){ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;pin(e.location().method());emit("field-store","at",location(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"receiver",value(e.object()),"before",value(e.valueCurrent()),"after",value(e.valueToBe()));stores++;
                    }else if(event instanceof ExceptionEvent){ExceptionEvent e=(ExceptionEvent)event;pin(e.location().method());List<String> frames=new ArrayList<>();for(StackFrame f:e.thread().frames())if(SOURCE.contains(f.location().declaringType().name())){pin(f.location().method());frames.add(location(f.location()));}emit("exception","at",location(e.location()),"type",e.exception().referenceType().name(),"catch",location(e.catchLocation()),"frames",frames,"nw",nw(),"pool",pool(true));failures++;calls.remove(e.thread().uniqueID());}
                }try{set.resume();}catch(VMDisconnectedException end){died=true;}
            }
        }finally{journal.close();try{vm.dispose();}catch(VMDisconnectedException ignored){}if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(5,TimeUnit.SECONDS);}stdout.join(5000);stderr.join(5000);}
        if(child.isAlive()||child.exitValue()!=0||stdout.isAlive()||stderr.isAlive()||!errors.isEmpty()||failures!=expectedFailures)throw new AssertionError("incomplete source/pipe trace: failures="+failures+" errors="+errors);System.out.println("FW_JDI_COMPLETE events="+sequence+" entries="+nextCall+" queries="+queries+" reads="+reads+" stores="+stores+" exceptions="+failures+" target-exit=0 source-writes=0");
    }
}
