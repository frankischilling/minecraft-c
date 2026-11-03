import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only actual Code, entropy, component/movement/brightness observations. */
public final class TorchParticleJdi {
    static VirtualMachine vm;
    static ObjectReference actor;
    static int constructorDepth, constructorProfile, constructorRecords, seq;
    static boolean ownCaptured;
    static long ownInitial;
    static String constructorKind;
    static final List<String> maths=new ArrayList<>(),own=new ArrayList<>(),mathSeeds=new ArrayList<>(),ownSeeds=new ArrayList<>();
    static final List<Object> constructorMethods=new ArrayList<>();
    static final Set<String> codeSeen=new HashSet<>();
    static final List<EventRequest> entropyRequests=new ArrayList<>();
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(ReferenceType type,String owner,String name){
        for(Field f:type.allFields())if(f.declaringType().name().equals(owner)&&f.name().equals(name))return f;
        throw new AssertionError("actual field "+owner+"."+name);
    }
    static Value get(ObjectReference value,String owner,String name){return value.getValue(field(value.referenceType(),owner,name));}
    static ReferenceType helper(){return vm.classesByName("TorchParticleObservation").get(0);}
    static Value helper(String name){ReferenceType t=helper();return t.getValue(t.fieldByName(name));}
    static int component(){return ((IntegerValue)helper("component_case")).value();}
    static long seed(ObjectReference random){
        ObjectReference atomic=(ObjectReference)get(random,"java.util.Random","seed");
        return ((LongValue)get(atomic,"java.util.concurrent.atomic.AtomicLong","value")).value();
    }
    static ObjectReference ownRandom(){return (ObjectReference)get(actor,"sn","bs");}
    static ObjectReference mathRandom(){
        ReferenceType type=vm.classesByName("java.lang.Math$RandomNumberGeneratorHolder").get(0);
        return (ObjectReference)type.getValue(type.fieldByName("randomNumberGenerator"));
    }
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String s(long value){return String.format(Locale.ROOT,"%012x",value);}
    static Object encoded(Value v){
        if(v==null)return null;
        if(v instanceof DoubleValue)return d(((DoubleValue)v).value());
        if(v instanceof FloatValue)return f(((FloatValue)v).value());
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof LongValue)return ((LongValue)v).value();
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof VoidValue)return "VOID";
        if(v instanceof ObjectReference){
            ObjectReference r=(ObjectReference)v;
            if(actor!=null&&r.equals(actor))return "ref:constructedParticle";
            Value particle=helper("observed_particle"),nw=helper("observed_nw");
            if(r.equals(particle))return "ref:observedParticle";
            if(r.equals(nw))return "ref:observedNW";
            return "ref:"+r.referenceType().name();
        }
        return v.toString();
    }
    static List<Object> vector(ObjectReference r,String owner,String... names){
        List<Object> values=new ArrayList<>();for(String name:names)values.add(encoded(get(r,owner,name)));return values;
    }
    static Map<String,Object> state(ObjectReference r){
        ObjectReference b=(ObjectReference)get(r,"sn","aW");
        return ConstructorJson.map("position",vector(r,"sn","aM","aN","aO"),
            "previous",vector(r,"sn","aJ","aK","aL"),"velocity",vector(r,"sn","aP","aQ","aR"),
            "bounds",vector(b,"eq","a","b","c","d","e","f"),"size",vector(r,"sn","bf","bg","bh"),
            "y_size",encoded(get(r,"sn","bo")),"texture",encoded(get(r,"xw","b")),
            "c",encoded(get(r,"xw","c")),"d",encoded(get(r,"xw","d")),
            "age",encoded(get(r,"xw","e")),"lifetime",encoded(get(r,"xw","f")),
            "scale",encoded(get(r,"xw","g")),"gravity",encoded(get(r,"xw","h")),
            "rgb",vector(r,"xw","i","j","k"),"base_scale",encoded(get(r,r.referenceType().name(),"a")),
            "dead",encoded(get(r,"sn","be")),"ground",encoded(get(r,"sn","aX")),"no_clip",encoded(get(r,"sn","bq")),
            "collision",vector(r,"sn","aY","aZ","ba","bb","bc","bd"));
    }
    static String at(Location location){return location.declaringType().name()+"."+location.method().name()+location.method().signature()+"@"+location.codeIndex();}
    static Map<String,Object> code(Method m)throws Exception{
        byte[] bytes=m.bytecodes();return ConstructorJson.map("class",m.declaringType().name(),"name",m.name(),"descriptor",m.signature(),
            "bytes",bytes.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
    }
    static boolean selected(Method m){
        String c=m.declaringType().name(),n=m.name(),s=m.signature();
        if(c.equals("xn")||c.equals("qu")||c.equals("xw"))return true;
        if(c.equals("sn"))return n.equals("<init>")||n.equals("K")||n.equals("a")&&s.equals("(F)F")||n.equals("b")&&s.equals("(DDD)V");
        if(c.equals("fd"))return n.equals("a")&&s.equals("(IIIIII)Z")||n.equals("c")&&s.equals("(III)F")||n.equals("d")&&s.equals("(III)I")||n.equals("b")&&s.equals("(IIII)I");
        if(c.equals("eq"))return n.equals("d")&&s.equals("(DDD)Leq;");
        if(c.equals("in"))return n.equals("a")&&s.equals("(D)F")||n.equals("b")&&s.equals("(D)I");
        if(c.equals("nw"))return n.equals("a")&&(s.equals("(FFF)V")||s.equals("(DDDDD)V")||s.equals("(DD)V")||s.equals("(DDD)V"));
        return false;
    }
    static Map<String,Object> event(MethodEntryEvent e)throws Exception{
        List<Object> args=new ArrayList<>();for(Value value:e.thread().frame(0).getArgumentValues())args.add(encoded(value));
        Map<String,Object> row=ConstructorJson.map("seq",seq++,"profile",component(),"event","entry","at",at(e.location()),
            "caller",e.thread().frameCount()>1?at(e.thread().frame(1).location()):null,"receiver",encoded(e.thread().frame(0).thisObject()),"args",args);
        if(component()>=0)row.put("particle",state((ObjectReference)helper("observed_particle")));
        return row;
    }
    static void request(EventRequestManager manager,String type,boolean entropy){
        MethodEntryRequest a=manager.createMethodEntryRequest();a.addClassFilter(type);a.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        MethodExitRequest b=manager.createMethodExitRequest();b.addClassFilter(type);b.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        if(entropy){entropyRequests.add(a);entropyRequests.add(b);}else{a.enable();b.enable();}
    }
    static String quoted(String value){require(value.indexOf('"')<0&&value.indexOf('\r')<0&&value.indexOf('\n')<0,"safe transport path");return "\""+value+"\"";}
    static final class Drain extends Thread{
        final InputStream input;final Path output;Throwable failure;
        Drain(InputStream i,Path o){input=i;output=o;setDaemon(true);}
        public void run(){try(InputStream i=input;OutputStream o=Files.newOutputStream(output,StandardOpenOption.CREATE_NEW)){
            byte[] b=new byte[8192];int n;long total=0;while((n=i.read(b))!=-1){total+=n;require(total<=16*1048576,"bounded original transport");o.write(b,0,n);}
        }catch(Throwable e){failure=e;}}
    }
    public static void main(String[] args)throws Exception{
        require(args.length==5,"classpath jar jdk-home output profiles");Path output=Path.of(args[3]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[2]);options.get("suspend").setValue("true");
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+quoted(args[0]));
        options.get("main").setValue("TorchParticleObservation "+quoted(args[1])+" "+quoted(args[4]));
        Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter constructors=new PrintWriter(Files.newBufferedWriter(output.resolve("constructors.jsonl"),StandardOpenOption.CREATE_NEW));
            PrintWriter events=new PrintWriter(Files.newBufferedWriter(output.resolve("events.jsonl"),StandardOpenOption.CREATE_NEW));
            PrintWriter codes=new PrintWriter(Files.newBufferedWriter(output.resolve("live-code.jsonl"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"read-only JDI code/returns");
            EventRequestManager manager=vm.eventRequestManager();ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("TorchParticleObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);prepare.enable();
            boolean done=false,enabled=false;long deadline=System.nanoTime()+300_000_000_000L;
            while(!done){
                require(System.nanoTime()<deadline,"bounded original observation deadline");EventSet batch;
                try{batch=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}
                if(batch==null)continue;
                for(Event raw:batch){
                    if(raw instanceof ClassPrepareEvent){ReferenceType t=((ClassPrepareEvent)raw).referenceType();
                        Method ready=t.methodsByName("ready","()V").get(0);BreakpointRequest breakpoint=manager.createBreakpointRequest(ready.location());breakpoint.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);breakpoint.enable();
                    }else if(raw instanceof BreakpointEvent){
                        require(!enabled,"one real ready boundary");enabled=true;raw.request().disable();prepare.disable();
                        for(String name:new String[]{"xn","qu","xw","sn","fd","eq","in","nw"})request(manager,name,false);
                        request(manager,"java.util.Random",true);request(manager,"java.lang.Math",true);
                        ExceptionRequest errors=manager.createExceptionRequest(null,true,true);errors.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);errors.enable();
                    }else if(raw instanceof MethodEntryEvent){
                        MethodEntryEvent e=(MethodEntryEvent)raw;Method m=e.method();String name=m.declaringType().name();
                        boolean derived=m.name().equals("<init>")&&(name.equals("xn")||name.equals("qu"));
                        if(derived){
                            ObjectReference current=e.thread().frame(0).thisObject();
                            if(actor==null){actor=current;constructorDepth=0;constructorProfile=((IntegerValue)helper("constructor_case")).value();constructorKind=((StringReference)helper("constructor_kind")).value();
                                maths.clear();own.clear();mathSeeds.clear();ownSeeds.clear();constructorMethods.clear();ownCaptured=false;
                                for(EventRequest request:entropyRequests)request.enable();}
                            require(actor.equals(current),"same actual delegated constructor owner");++constructorDepth;
                        }
                        if(actor!=null&&name.equals("java.util.Random")&&m.name().equals("nextFloat")){
                            ObjectReference random=e.thread().frame(0).thisObject();
                            if(random.equals(ownRandom())&&!ownCaptured){ownInitial=seed(random);ownCaptured=true;}
                        }
                        if((actor!=null||component()>=0)&&selected(m)){
                            String key=name+"."+m.name()+m.signature();Map<String,Object> pin=code(m);
                            if(codeSeen.add(key)){codes.println(ConstructorJson.json(pin));codes.flush();}
                            if(actor!=null&&m.name().equals("<init>"))constructorMethods.add(pin);
                            events.println(ConstructorJson.json(event(e)));events.flush();
                        }
                    }else if(raw instanceof MethodExitEvent){
                        MethodExitEvent e=(MethodExitEvent)raw;Method m=e.method();String name=m.declaringType().name();
                        if(actor!=null&&name.equals("java.lang.Math")&&m.name().equals("random")){
                            maths.add(d(((DoubleValue)e.returnValue()).value()));mathSeeds.add(s(seed(mathRandom())));
                        }else if(actor!=null&&name.equals("java.util.Random")&&m.name().equals("nextFloat")){
                            ObjectReference random=e.thread().frame(0).thisObject();if(random.equals(ownRandom())){own.add(f(((FloatValue)e.returnValue()).value()));ownSeeds.add(s(seed(random)));}
                        }
                        if((actor!=null||component()>=0)&&selected(m)){
                            Map<String,Object> row=ConstructorJson.map("seq",seq++,"profile",component(),"event","return","at",at(e.location()),"result",encoded(e.returnValue()));
                            if(component()>=0)row.put("particle",state((ObjectReference)helper("observed_particle")));
                            events.println(ConstructorJson.json(row));events.flush();
                        }
                        if(actor!=null&&m.name().equals("<init>")&&(name.equals("xn")||name.equals("qu"))&&--constructorDepth==0){
                            require(ownCaptured,"actual private Random first float");
                            constructors.println(ConstructorJson.json(ConstructorJson.map("profile",constructorProfile,"kind",constructorKind,
                                "math_values",new ArrayList<>(maths),"math_after_each",new ArrayList<>(mathSeeds),
                                "own_values",new ArrayList<>(own),"own_after_each",new ArrayList<>(ownSeeds),"own_initial",s(ownInitial),
                                "own_after",s(seed(ownRandom())),"methods",new ArrayList<>(constructorMethods),"state",state(actor))));constructors.flush();
                            ++constructorRecords;actor=null;for(EventRequest request:entropyRequests)request.disable();
                        }
                    }else if(raw instanceof ExceptionEvent&&enabled&&(actor!=null||component()>=0)){
                        ExceptionEvent e=(ExceptionEvent)raw;List<String> stack=new ArrayList<>();for(StackFrame frame:e.thread().frames())stack.add(at(frame.location()));
                        events.println(ConstructorJson.json(ConstructorJson.map("seq",seq++,"profile",component(),"event","exception","at",at(e.location()),
                            "exception",e.exception().referenceType().name(),"catch",e.catchLocation()==null?null:at(e.catchLocation()),"frames",stack)));events.flush();
                    }else if(raw instanceof VMDeathEvent||raw instanceof VMDisconnectEvent)done=true;
                }
                try{batch.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"actual target completion");stdout.join(10000);stderr.join(10000);
            require(enabled&&constructorRecords==37&&actor==null&&!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"complete actual observation/transport");
            require(!constructors.checkError()&&!events.checkError()&&!codes.checkError(),"complete retained journals");
        }finally{
            if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}
            if(child!=null&&child.isAlive()){if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}}
        }
    }
}
