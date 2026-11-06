import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Observe genuine gs.a at93 before its pending wake invocation. No original writes. */
public final class PlayerNbtPrefix {
    static String quote(String value) {
        if(value.indexOf('"')>=0||value.indexOf('\n')>=0||value.indexOf('\r')>=0)
            throw new IllegalArgumentException("unsupported quote/newline in observation path");
        return "\""+value+"\"";
    }
    static final class Drain extends Thread {
        final InputStream input;final Path path;volatile Throwable error;long count;
        Drain(InputStream stream,Path output){input=stream;path=output;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){
            byte[] buffer=new byte[4096];int n;
            while((n=in.read(buffer))!=-1){count+=n;if(count>2*1024*1024)throw new IOException("target output bound");out.write(buffer,0,n);}
        }catch(Throwable problem){error=problem;}}
    }
    static Throwable append(Throwable first,Throwable next){
        if(first==null)return next;if(first!=next)first.addSuppressed(next);return first;
    }
    static void closeUnstarted(Drain drain,InputStream fallback)throws IOException{
        if(drain==null||drain.getState()==Thread.State.NEW)fallback.close();
    }
    static void joinStarted(Drain drain)throws InterruptedException{
        if(drain==null||drain.getState()==Thread.State.NEW)return;
        drain.join(10000);if(drain.isAlive())throw new AssertionError("target reader not joined");
    }
    static void retire(VirtualMachine vm,Process p,Drain out,Drain err)throws Exception{
        Throwable failure=null;
        if(p!=null){
            try{if(p.isAlive())p.destroyForcibly();if(!p.waitFor(10,TimeUnit.SECONDS))throw new AssertionError("target cleanup not reaped");}
            catch(Throwable problem){failure=append(failure,problem);}
            try{p.getOutputStream().close();}catch(Throwable problem){failure=append(failure,problem);}
            try{closeUnstarted(out,p.getInputStream());}catch(Throwable problem){failure=append(failure,problem);}
            try{closeUnstarted(err,p.getErrorStream());}catch(Throwable problem){failure=append(failure,problem);}
            try{joinStarted(out);}catch(Throwable problem){failure=append(failure,problem);}
            try{joinStarted(err);}catch(Throwable problem){failure=append(failure,problem);}
        }else{
            try{vm.exit(1);}catch(VMDisconnectedException expected){}
            catch(Throwable problem){failure=append(failure,problem);}
        }
        try{vm.dispose();}catch(VMDisconnectedException expected){}
        catch(Throwable problem){failure=append(failure,problem);}
        if(failure instanceof Exception)throw (Exception)failure;
        if(failure instanceof Error)throw (Error)failure;
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=7)throw new IllegalArgumentException("classpath root jar natives stdout stderr gs-read-Code-sha256");
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(System.getProperty("java.home"));
        options.get("main").setValue("PlayerNbtObservation "+quote(args[1])+" "+quote(args[2])+" sleep");
        options.get("options").setValue("-Xint -Xverify:all -Xmx512m --add-opens=java.base/java.util=ALL-UNNAMED "+
            "-Dorg.lwjgl.librarypath="+quote(args[3])+" -cp "+quote(args[0]));
        Process process=null;Drain out=null,err=null;Throwable primary=null;
        VirtualMachine vm=connector.launch(options);
        try{
            process=vm.process();out=new Drain(process.getInputStream(),Path.of(args[4]));
            err=new Drain(process.getErrorStream(),Path.of(args[5]));out.start();err.start();
            EventRequestManager requests=vm.eventRequestManager();
            ClassPrepareRequest prepare=requests.createClassPrepareRequest();prepare.addClassFilter("gs");
            prepare.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);prepare.enable();
            int prepared=0,checkpoints=0,events=0;boolean done=false;
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(120);
            while(!done){
                if(System.nanoTime()>deadline||events>1000||out.error!=null||err.error!=null)
                    throw new AssertionError("observer deadline/event/reader bound");
                EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set){++events;
                    if(event instanceof ClassPrepareEvent){
                        ReferenceType type=((ClassPrepareEvent)event).referenceType();
                        List<Method> methods=type.methodsByName("a","(Lnu;)V");
                        if(methods.size()!=1)throw new AssertionError("actual gs.a inventory");
                        Method method=methods.get(0);byte[] code=method.bytecodes();
                        String hash=HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code));
                        if(!hash.equals(args[6])||code.length!=153||(code[93]&255)!=0xb6)
                            throw new AssertionError("actual live gs.a Code/pending invocation differs");
                        BreakpointRequest point=requests.createBreakpointRequest(method.locationOfCodeIndex(93));
                        point.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);point.enable();
                        prepare.disable();++prepared;
                    }else if(event instanceof BreakpointEvent){
                        BreakpointEvent point=(BreakpointEvent)event;
                        if(point.location().codeIndex()!=93||!point.location().method().signature().equals("(Lnu;)V")||
                           !point.location().declaringType().name().equals("gs")||++checkpoints!=1)
                            throw new AssertionError("actual wake checkpoint identity");
                        point.request().disable();
                        ObjectReference receiver=point.thread().frame(0).thisObject();
                        if(receiver==null||!receiver.referenceType().name().equals("dc"))
                            throw new AssertionError("genuine canonical dc receiver");
                        ClassType observer=(ClassType)vm.classesByName("PlayerNbtObservation").get(0);
                        Method snapshot=observer.methodsByName("sleepingPrefix","(Ldc;)Ljava/lang/String;").get(0);
                        Value observed=observer.invokeMethod(point.thread(),snapshot,List.of(receiver),ObjectReference.INVOKE_SINGLE_THREADED);
                        System.out.println("SITE sleep gs a (Lnu;)V 93");
                        System.out.print(((StringReference)observed).value());
                    }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try{set.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            if(!process.waitFor(10,TimeUnit.SECONDS))throw new AssertionError("target not reaped");
            out.join(10000);err.join(10000);
            if(out.isAlive()||err.isAlive()||out.error!=null||err.error!=null||process.exitValue()!=0||
               err.count!=0||prepared!=1||checkpoints!=1)
                throw new AssertionError("original target/reader/checkpoint incomplete");
            System.out.println("Original Player NBT prefix: 1 checkpoint");
            if(System.out.checkError())throw new IOException("observer stdout");
        }catch(Throwable problem){primary=problem;throw problem;}
        finally{
            try{retire(vm,process,out,err);}catch(Throwable cleanup){
                if(primary!=null)primary.addSuppressed(cleanup);
                else if(cleanup instanceof Exception)throw (Exception)cleanup;
                else throw (Error)cleanup;
            }
        }
    }
}
