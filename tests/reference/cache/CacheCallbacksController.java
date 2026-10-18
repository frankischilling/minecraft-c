import com.sun.jdi.ArrayReference;
import com.sun.jdi.ByteValue;
import com.sun.jdi.StringReference;
import com.sun.jdi.Bootstrap;
import com.sun.jdi.BooleanValue;
import com.sun.jdi.ClassLoaderReference;
import com.sun.jdi.Field;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.Location;
import com.sun.jdi.LongValue;
import com.sun.jdi.Method;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.StackFrame;
import com.sun.jdi.ThreadReference;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.BreakpointEvent;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.MethodExitEvent;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.request.BreakpointRequest;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import com.sun.jdi.request.EventRequestManager;
import com.sun.jdi.request.MethodExitRequest;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Loopback, read-only JDI capture of genuine successful-load and natural-population callback boundaries. */
public final class CacheCallbacksController {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final int MAX_THREADS = 64, MAX_FRAMES = 64;
    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private static Map<String, Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }
    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    private static String json(Object value) throws Exception {
        java.lang.reflect.Method encode = Class.forName("ChunkObservation").getDeclaredMethod("json", Object.class);
        encode.setAccessible(true); return (String) encode.invoke(null, value);
    }
    private static long remaining(long deadline) {
        long result = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(result > 0, "90-second debuggee/controller deadline"); return Math.min(result, 10000);
    }
    private static void newOutput(Path path) {
        require(!Files.exists(path, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(path)
                && Files.isDirectory(path.getParent()), "new output beneath existing parent: " + path);
    }
    private static byte[] capture(InputStream source, CompletableFuture<String> startup, AtomicLong total) throws Exception {
        ByteArrayOutputStream result = new ByteArrayOutputStream(), first = startup == null ? null : new ByteArrayOutputStream();
        byte[] block = new byte[8192];
        try {
            for (int count; (count = source.read(block)) >= 0;) {
                require(total.addAndGet(count) <= MAX_BYTES, "combined child stdout/stderr bound");
                result.write(block, 0, count);
                if (first != null) for (int i = 0; i < count; ++i) {
                    if (block[i] == '\n') {
                        String line = first.toString(StandardCharsets.US_ASCII);
                        if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
                        startup.complete(line); first = null; break;
                    }
                    require(first.size() < 1024, "bounded JDWP startup line"); first.write(block[i]);
                }
            }
            if (first != null) startup.completeExceptionally(new IllegalStateException("missing JDWP startup line"));
            return result.toByteArray();
        } catch (Throwable failure) {
            if (startup != null) startup.completeExceptionally(failure); throw failure;
        }
    }
    private static Method method(ReferenceType owner, String name, String signature, int modifiers) {
        List<Method> methods = owner.methodsByName(name, signature);
        require(methods.size() == 1 && methods.get(0).declaringType().equals(owner) && methods.get(0).modifiers() == modifiers,
                "exact original method " + owner.name() + "." + name + signature);
        return methods.get(0);
    }
    private static Field field(ReferenceType owner, String name, String signature, int modifiers) {
        Field result = owner.fieldByName(name);
        require(result != null && result.declaringType().equals(owner) && result.signature().equals(signature)
                && (result.modifiers() == modifiers || (modifiers & 0x1000) != 0 && result.modifiers() == (modifiers | 0xf0000000)), "exact field " + owner.name() + "." + name + " expected=" + signature + "/" + modifiers + " actual=" + (result == null ? "null" : result.signature() + "/" + result.modifiers()));
        return result;
    }
    private static int integer(Value value) {
        require(value instanceof IntegerValue, "actual integer JDI value"); return ((IntegerValue) value).value();
    }
    private static ObjectReference reference(Value value) {
        require(value instanceof ObjectReference, "actual nonnull JDI object"); return (ObjectReference) value;
    }

    private static long randomSeed(ObjectReference random) {
        ReferenceType owner = random.referenceType();
        require(owner.name().equals("java.util.Random") && owner.classLoader() == null, "bootstrap java.util.Random");
        ObjectReference seed = reference(random.getValue(field(owner, "seed", "Ljava/util/concurrent/atomic/AtomicLong;", 18)));
        require(seed.referenceType().name().equals("java.util.concurrent.atomic.AtomicLong") && seed.referenceType().classLoader() == null,
                "bootstrap atomic seed owner");
        Value value = seed.getValue(field(seed.referenceType(), "value", "J", 66));
        require(value instanceof LongValue, "actual long seed"); long result = ((LongValue) value).value();
        require(result >= 0 && result < (1L << 48), "actual 48-bit Java Random state"); return result;
    }

    private static final class Session {
        final VirtualMachine vm;
        final EventRequestManager manager;
        final boolean client;
        final String scenario;
        final int callCount;
        final Field daemonField, holderField;
        final String daemonLayout;
        final List<Object> operations = new ArrayList<>();
        final Map<String,Object> objectReferences = new LinkedHashMap<>();
        final Map<String,Long> objectIDs = new LinkedHashMap<>();
        final Map<Long,ObjectReference> chunkObjects = new LinkedHashMap<>();
        final Map<String,ReferenceType> originalTypes = new LinkedHashMap<>();
        final Map<Method,String> exitMethods = new LinkedHashMap<>();
        final List<BreakpointRequest> quietInsidePopulation = new ArrayList<>();
        final List<Map<String,Object>> seamStack = new ArrayList<>();
        final Map<String,Object> bindings = new LinkedHashMap<>();
        final List<Object> timeProof = new ArrayList<>();
        Field generatorRandom, caveField, caveRandom;
        ObjectReference world, cache, generator, loader, chunk, spawnWorld, timeWorld, populationTarget;
        ThreadReference thread, spawnThread, timeThread;
        ReferenceType worldType, cacheType, chunkType;
        Method acquire, get, wrapperLoad, actualLoad, generate, providerPopulate, activate, setTime;
        MethodExitRequest cacheExits, loaderExits, generatorExits, registrationExits;
        BreakpointRequest getEntry, spawnEntry, spawnStep;
        int active=-1, beginCount, endCount, eventCount, deliveredEvents, spawnEntries, randomSteps, identityMarkers;
        int generatedCount, finishingCount, populationCount, opaqueDepth, timeState;
        boolean finishing;
        List<Object> events, seams;
        Map<String,Object> beginThreads, beginRandom;
        Session(VirtualMachine vm, boolean client, String scenario) {
            this.vm=vm; this.client=client; this.scenario=scenario;
            callCount=scenario.equals("load")?2:4; manager=vm.eventRequestManager();
            ReferenceType threads=vm.classesByName("java.lang.Thread").get(0);
            require(threads.classLoader()==null,"bootstrap Thread binding");
            if(threads.fieldByName("daemon")!=null) {
                daemonField=field(threads,"daemon","Z",2);holderField=null;daemonLayout="thread-daemon";
            } else {
                holderField=field(threads,"holder","Ljava/lang/Thread$FieldHolder;",18);
                List<ReferenceType> holders=vm.classesByName("java.lang.Thread$FieldHolder");
                require(holders.size()==1&&holders.get(0).classLoader()==null,"bootstrap FieldHolder binding");
                daemonField=field(holders.get(0),"daemon","Z",64);daemonLayout="thread-holder-daemon";
            }
            prepare("CacheCallbacksObservation"); prepare(client?"fd":"dj");
        }
        void prepare(String name) {
            ClassPrepareRequest request=manager.createClassPrepareRequest();request.addClassFilter(name);
            request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
        }
        BreakpointRequest stop(Method method,long index,String marker,boolean filtered,int policy) {
            Location location=method.locationOfCodeIndex(index);
            require(location!=null&&location.codeIndex()==index,"exact code index "+method+"@"+index);
            BreakpointRequest request=manager.createBreakpointRequest(location);request.putProperty("marker",marker);
            if(filtered)request.addThreadFilter(thread);request.setSuspendPolicy(policy);request.enable();return request;
        }
        void prepared(ClassPrepareEvent event) {
            ReferenceType type=event.referenceType();
            if(type.name().equals("CacheCallbacksObservation")) {
                for(String name:new String[]{"begin","end","identity","timeBegin","timeEnd"}) {
                    String sig=name.equals("begin")?"(Ljava/lang/Object;Ljava/lang/Object;I)V":name.equals("end")?"(Ljava/lang/Object;I)V":name.equals("identity")?"(Ljava/lang/Object;Ljava/lang/String;)V":"(Ljava/lang/Object;J)V";
                    stop(method(type,name,sig,10),0,name,false,name.equals("identity")?EventRequest.SUSPEND_EVENT_THREAD:EventRequest.SUSPEND_ALL);
                }
            } else {
                require(type.name().equals(client?"fd":"dj"),"known original world prepare");
                setTime=method(type,"a","(J)V",1);byte[] code=setTime.bytecodes();
                require(code.length==9&&(code[5]&255)==182&&(code[8]&255)==177,"pinned simple metadata time setter");
                stop(setTime,0,"time_api",false,EventRequest.SUSPEND_EVENT_THREAD);
                stop(setTime,8,"time_api",false,EventRequest.SUSPEND_EVENT_THREAD);
                if(!client) {
                    Method spawn=method(type,"c","()V",4);code=spawn.bytecodes();
                    require(code.length==87&&(code[24]&255)==27&&(code[31]&255)==182&&(code[62]&255)==182,"pinned ordinary spawn body");
                    spawnEntry=stop(spawn,0,"spawn_entry",false,EventRequest.SUSPEND_EVENT_THREAD);
                    spawnStep=stop(spawn,24,"spawn_step",false,EventRequest.SUSPEND_EVENT_THREAD);
                }
            }
            event.request().disable();
        }
        ReferenceType originalType(String name) {
            if (originalTypes.containsKey(name)) return originalTypes.get(name);
            List<ReferenceType> found = vm.classesByName(name);
            require(found.size() == 1 && world.referenceType().classLoader().equals(found.get(0).classLoader()), "same original loader " + name);
            originalTypes.put(name, found.get(0)); return found.get(0);
        }
        Value read(ObjectReference object, ReferenceType type, String name, String signature, int modifiers) {
            return object.getValue(field(type, name, signature, modifiers));
        }
        ObjectReference cacheObject(String name, String signature) { return reference(read(cache, cacheType, name, signature, 2)); }
        Value bootstrap(ObjectReference value, String name, String signature) {
            require(value.referenceType().classLoader() == null, "bootstrap collection identity");
            Field actual = value.referenceType().fieldByName(name);
            require(actual != null && actual.signature().equals(signature), "bootstrap field descriptor " + name);
            return value.getValue(actual);
        }
        List<Object> mapNodes(ObjectReference map, boolean values) {
            require(map.referenceType().name().equals("java.util.HashMap"), "actual HashMap");
            int size = integer(bootstrap(map, "size", "I")); require(size >= 0 && size <= 64, "complete bounded map");
            List<Object> result = new ArrayList<>(); Set<Long> nodes = new HashSet<>();
            Value table = bootstrap(map, "table", "[Ljava/util/HashMap$Node;");
            if (table != null) {
                require(table instanceof ArrayReference && ((ArrayReference) table).length() <= 256, "bounded table");
                for (Value bucket : ((ArrayReference) table).getValues()) {
                    for (ObjectReference node = (ObjectReference) bucket; node != null; node = (ObjectReference) bootstrap(node, "next", "Ljava/util/HashMap$Node;")) {
                        require(result.size() < 64 && nodes.add(node.uniqueID()) && node.referenceType().name().equals("java.util.HashMap$Node"), "bounded ordinary map chains");
                        ObjectReference key = reference(bootstrap(node, "key", "Ljava/lang/Object;"));
                        require(key.referenceType().name().equals("java.lang.Integer"), "actual Integer key");
                        int number = integer(bootstrap(key, "value", "I"));
                        result.add(values ? object("key", number, "value", chunk((ObjectReference) bootstrap(node, "value", "Ljava/lang/Object;"))) : number);
                    }
                }
            }
            require(result.size() == size, "complete actual map traversal"); return result;
        }
        List<Object> listValues(ObjectReference list, boolean chunks, int limit) {
            require(list.referenceType().name().equals("java.util.ArrayList"), "actual ArrayList");
            int size = integer(bootstrap(list, "size", "I")); require(size >= 0 && size <= limit, "bounded list");
            ArrayReference elements = (ArrayReference) bootstrap(list, "elementData", "[Ljava/lang/Object;");
            require(elements != null && elements.length() >= size && elements.length() <= 65536, "bounded list owner storage");
            List<Object> result = new ArrayList<>();
            for (Value value : elements.getValues(0, size)) result.add(chunks ? chunk((ObjectReference) value) : region(reference(value)));
            return result;
        }
        Object chunk(ObjectReference value) {
            if (value == null) return null;
            require(chunkObjects.size() < 32 || chunkObjects.containsKey(value.uniqueID()), "bounded complete chunk identity inventory");
            chunkObjects.putIfAbsent(value.uniqueID(), value);
            require(chunkType.classLoader().equals(value.referenceType().classLoader()), "same original chunk loader");
            Map<String, Object> flags = new LinkedHashMap<>();
            for (String name : new String[] {"c", "n", "o", "p", "q"}) flags.put(name, ((BooleanValue) read(value, chunkType, name, "Z", 1)).value());
            return object("id", value.uniqueID(), "class", value.referenceType().name(),
                    "coordinates", Arrays.asList(integer(read(value, chunkType, "j", "I", 17)), integer(read(value, chunkType, "k", "I", 17))),
                    "flags", flags, "time", String.format("%016x", ((LongValue) read(value, chunkType, "r", "J", 1)).value()));
        }
        Object region(ObjectReference value) {
            ReferenceType type = originalType(client ? "st" : "lv");
            require(value.referenceType().equals(type), "actual queued light region");
            List<Field> fields = new ArrayList<>(); fields.add(field(type, "a", client ? "Leb;" : "Lco;", 17));
            for (String name : new String[] {"b", "c", "d", "e", "f", "g"}) fields.add(field(type, name, "I", 1));
            Map<Field,Value> values = value.getValues(fields);
            ObjectReference kind = reference(values.get(fields.get(0)));
            ReferenceType kindType = originalType(client ? "eb" : "co");
            List<Object> bounds = new ArrayList<>();
            ObjectReference sky = reference(kindType.getValue(field(kindType, "a", client ? "Leb;" : "Lco;", 0x4019)));
            ObjectReference block = reference(kindType.getValue(field(kindType, "b", client ? "Leb;" : "Lco;", 0x4019)));
            require(kind.equals(sky) || kind.equals(block), "actual light enum identity");
            bounds.add(kind.equals(sky) ? 0 : 1);
            for (int index = 1; index < fields.size(); ++index) bounds.add(integer(values.get(fields.get(index))));
            return object("id", value.uniqueID(), "region", bounds);
        }
        Object cacheSnapshot() {
            String[] names = {"a","b","e","f"};
            ObjectReference map = cacheObject(names[2], "Ljava/util/Map;"), list = cacheObject(names[3], "Ljava/util/List;");
            ObjectReference pendingSet = cacheObject(names[0], "Ljava/util/Set;");
            require(pendingSet.referenceType().name().equals("java.util.HashSet"), "actual pending HashSet");
            ObjectReference pendingMap = reference(bootstrap(pendingSet, "map", "Ljava/util/HashMap;"));
            ObjectReference queue = reference(read(world, worldType, "C", "Ljava/util/List;", 2));
            return object("cache_id", cache.uniqueID(), "map_id", map.uniqueID(), "list_id", list.uniqueID(),
                    "pending_id", pendingSet.uniqueID(), "queue_id", queue.uniqueID(),
                    "entries", mapNodes(map, true), "insertions", listValues(list, true, 64), "pending", mapNodes(pendingMap, false),
                    "queue", listValues(queue, false, 32768), "fallback", chunk(cacheObject(names[1], client ? "Llm;" : "Lhi;")),
                    "world_force_load", ((BooleanValue) read(world, worldType, "y", "Z", 1)).value(),
                    "provider_force_load", null);
        }
        Object byteArray(ObjectReference value, String name, int length) {
            ArrayReference array = (ArrayReference) read(value, chunkType, name, "[B", 1);
            return bytes(array, length);
        }
        Object bytes(ArrayReference array, int length) {
            require(array != null && array.length() == length, "complete original local array");
            byte[] raw = new byte[length]; List<Value> values = array.getValues();
            for (int i = 0; i < length; ++i) { require(values.get(i) instanceof ByteValue, "actual byte value"); raw[i] = ((ByteValue) values.get(i)).value(); }
            return object("length", length, "values", HexFormat.of().formatHex(raw));
        }
        Object localChunk(ObjectReference value) {
            Map<String, Object> data = new LinkedHashMap<>(); data.put("blocks", byteArray(value, "b", 32768));
            ReferenceType nibble = originalType(client ? "wi" : "ob");
            String[] names = {"e","f","g"}, roles = {"metadata","skylight","block_light"};
            for (int i = 0; i < names.length; ++i) {
                ObjectReference wrapper = reference(read(value, chunkType, names[i], client ? "Lwi;" : "Lob;", 1));
                data.put(roles[i], bytes((ArrayReference) read(wrapper, nibble, "a", "[B", 17), 16384));
            }
            data.put("heightmap", byteArray(value, "h", 256)); data.put("minimum_height", integer(read(value, chunkType, "i", "I", 1)));
            return object("chunk", chunk(value), "data", data);
        }
        MethodExitRequest exits(ReferenceType type) {
            MethodExitRequest request=manager.createMethodExitRequest();request.addClassFilter(type);request.addThreadFilter(thread);
            request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);return request;
        }
        void bind(String name,Method method,int length) throws Exception {
            byte[] bytes=method.bytecodes();require(bytes.length==length,"exact original method length "+method);
            bindings.put(name,object("class",method.declaringType().name(),"method",method.name(),"signature",method.signature(),
                    "modifiers",method.modifiers(),"bytecode_length",bytes.length,"bytecode_hex",HexFormat.of().formatHex(bytes),"bytecode_sha256",hash(bytes)));
        }
        void watch(Method method,int index,String name,boolean quiet) {
            BreakpointRequest request=stop(method,index,name,true,EventRequest.SUSPEND_EVENT_THREAD);
            if(quiet)quietInsidePopulation.add(request);
        }
        void install() throws Exception {
            worldType=originalType(client?"fd":"dj");chunkType=originalType(client?"lm":"hi");cacheType=originalType(client?"ok":"jb");
            require(world.referenceType().equals(worldType)&&cache.referenceType().equals(cacheType),"exact persistent base-world scope");
            generator=cacheObject("c",client?"Lcl;":"Lbl;");loader=cacheObject("d",client?"Lbf;":"Lan;");
            ReferenceType generatorType=originalType(client?"yf":"pk");
            require(generator.referenceType().equals(generatorType)&&loader.referenceType().equals(originalType(client?"ld":"hb")),"actual retained provider and persistent loader");
            require(reference(read(generator,generatorType,"p",client?"Lfd;":"Ldj;",2)).equals(world),"retained generator world");
            generatorRandom=field(generatorType,"j","Ljava/util/Random;",2);caveField=field(generatorType,"u",client?"Lfv;":"Ldy;",2);
            caveRandom=field(originalType(client?"fv":"dy"),"b","Ljava/util/Random;",4);
            acquire=method(cacheType,"c",client?"(II)Llm;":"(II)Lhi;",1);bind("acquire",acquire,394);
            for(int index:new int[]{0,20,46,78,85,100,112,128,391})watch(acquire,index,"acquire",index!=0);
            get=method(cacheType,"b",client?"(II)Llm;":"(II)Lhi;",1);bind("get",get,34);
            getEntry=stop(get,0,"get",true,EventRequest.SUSPEND_EVENT_THREAD);quietInsidePopulation.add(getEntry);exitMethods.put(get,"get_return");
            wrapperLoad=method(cacheType,"d",client?"(II)Llm;":"(II)Lhi;",2);bind("load_wrapper",wrapperLoad,49);
            for(int index:new int[]{0,25,37,40})watch(wrapperLoad,index,"load_wrapper",true);
            exitMethods.put(wrapperLoad,"load_wrapper_return");
            actualLoad=method(loader.referenceType(),"a",client?"(Lfd;II)Llm;":"(Ldj;II)Lhi;",1);bind("load",actualLoad,278);
            watch(actualLoad,0,"loader",false);
            for(int index:new int[]{27,78,134,277})watch(actualLoad,index,"loader_ready",false);
            exitMethods.put(actualLoad,"loader_return");loaderExits=exits(loader.referenceType());
            generate=method(generatorType,"b",client?"(II)Llm;":"(II)Lhi;",1);bind("generate",generate,130);
            watch(generate,0,"generate",false);watch(generate,129,"generator_ready",false);exitMethods.put(generate,"generate_return");
            providerPopulate=method(generatorType,"a",client?"(Lcl;II)V":"(Lbl;II)V",1);
            byte[] populationCode=providerPopulate.bytecodes();require(populationCode.length>1000&&(populationCode[populationCode.length-1]&255)==177,"complete original population return");
            bind("provider_populate",providerPopulate,populationCode.length);watch(providerPopulate,0,"provider_populate",false);
            watch(providerPopulate,populationCode.length-1,"provider_ready",false);exitMethods.put(providerPopulate,"provider_populate_return");generatorExits=exits(generatorType);
            Method finish=method(chunkType,client?"c":"b","()V",1);bind("finish",finish,222);watch(finish,0,"finish",false);watch(finish,221,"finish",false);
            activate=method(chunkType,client?"e":"d","()V",1);bind("activate",activate,52);
            for(int index:new int[]{0,5,51})watch(activate,index,"activate",true);
            Method population=method(cacheType,"a",client?"(Lcl;II)V":"(Lbl;II)V",1);bind("population_wrapper",population,47);
            for(int index:new int[]{0,22,36,43,46})watch(population,index,"population",true);
            Method dirty=method(chunkType,client?"g":"f","()V",1);bind("mark_dirty",dirty,6);watch(dirty,0,"mark_dirty",true);watch(dirty,5,"mark_dirty",true);
            for(String signature:new String[]{"(Ljava/util/Collection;)V","(Ljava/util/List;)V"}) {
                Method registration=method(worldType,"a",signature,1);watch(registration,0,signature.contains("Collection")?"register_tiles":"register_entities",true);
                exitMethods.put(registration,signature.contains("Collection")?"register_tiles_return":"register_entities_return");
            }
            exitMethods.put(acquire,"acquire_return");cacheExits=exits(cacheType);registrationExits=exits(worldType);
            bind("time_api",setTime,9);
        }
        Map<String,Object> inventory(BreakpointEvent event,String marker,int index) throws Exception {
            require(event.request().suspendPolicy()==EventRequest.SUSPEND_ALL&&event.thread().equals(thread)&&thread.name().equals("main"),"complete matching main marker inventory");
            List<ThreadReference> threads=vm.allThreads();require(!threads.isEmpty()&&threads.size()<=MAX_THREADS,"complete thread inventory bound");
            List<Object> result=new ArrayList<>();ClassLoaderReference original=world.referenceType().classLoader();int mains=0;
            for(ThreadReference candidate:threads) {
                require(candidate.isSuspended(),"all Java threads suspended");boolean measured=candidate.equals(thread);if(measured)++mains;
                ObjectReference daemonOwner=holderField==null?candidate:reference(candidate.getValue(holderField));
                require(daemonOwner.referenceType().equals(daemonField.declaringType())||holderField==null,"daemon field holder identity");
                Value value=daemonOwner.getValue(daemonField);require(value instanceof BooleanValue,"actual daemon value");boolean daemon=((BooleanValue)value).value();
                int count=candidate.frameCount();require(count>=0&&count<=MAX_FRAMES,"complete bounded stack inventory");List<Object> frames=new ArrayList<>();boolean fromOriginal=false;
                for(StackFrame frame:candidate.frames(0,count)) {
                    Location location=frame.location();ClassLoaderReference loader=location.declaringType().classLoader();boolean actualOriginal=original.equals(loader);fromOriginal|=actualOriginal;
                    frames.add(object("ordinal",frames.size(),"class",location.declaringType().name(),"method",location.method().name(),"signature",location.method().signature(),"code_index",location.codeIndex(),"class_loader_id",loader==null?null:loader.uniqueID(),"original_frame",actualOriginal));
                }
                require(measured?!daemon:daemon&&!fromOriginal,"strict finite base-world inventory; no other original frame");
                result.add(object("ordinal",result.size(),"id",candidate.uniqueID(),"name",candidate.name(),"object_class",candidate.referenceType().name(),"status",candidate.status(),"daemon",daemon,"measured",measured,"suspended",true,"frame_count",count,"original_frame",fromOriginal,"frames",frames));
            }
            require(mains==1,"one measured main");return object("marker",marker,"index",index,"suspend_policy",EventRequest.SUSPEND_ALL,"measured_thread_id",thread.uniqueID(),"original_loader_id",original.uniqueID(),"thread_limit",MAX_THREADS,"frames_per_thread_limit",MAX_FRAMES,"daemon_layout",daemonLayout,"thread_count",result.size(),"measured_main_thread",true,"all_threads_suspended",true,"all_other_threads_daemon",true,"no_other_original_frames",true,"threads",result);
        }
        Map<String,Object> seeds() {
            ObjectReference cave=reference(generator.getValue(caveField)), random=reference(generator.getValue(generatorRandom)), caveRng=reference(cave.getValue(caveRandom));
            return object("generator_id",generator.uniqueID(),"generator_random_id",random.uniqueID(),"generator_seed",String.format("%012x",randomSeed(random)),"cave_id",cave.uniqueID(),"cave_random_id",caveRng.uniqueID(),"cave_seed",String.format("%012x",randomSeed(caveRng)));
        }
        Object worldTime(ObjectReference actualWorld) {
            ReferenceType type=actualWorld.referenceType();ObjectReference info=reference(read(actualWorld,type,"x",client?"Lei;":"Lct;",4));
            return String.format("%016x",((LongValue)read(info,originalType(client?"ei":"ct"),"e","J",2)).value());
        }
        Object boundary(ObjectReference returned) {
            if(returned!=null)chunk(returned);Object cacheState=cacheSnapshot();List<Object> chunks=new ArrayList<>();
            for(ObjectReference value:chunkObjects.values()) {
                boolean blank=!value.referenceType().equals(chunkType);require(!blank||value.referenceType().equals(originalType(client?"li":"hg")),"actual normal or blank chunk");
                List<Object> owners = new ArrayList<>(), slices = new ArrayList<>();
                owners.add(reference(read(value,chunkType,"b","[B",1)).uniqueID());
                ReferenceType nibble = originalType(client?"wi":"ob");
                for(String name:new String[]{"e","f","g"}) {
                    ObjectReference wrapper=reference(read(value,chunkType,name,client?"Lwi;":"Lob;",1));
                    owners.add(wrapper.uniqueID());owners.add(reference(read(wrapper,nibble,"a","[B",17)).uniqueID());
                }
                owners.add(reference(read(value,chunkType,"h","[B",1)).uniqueID());
                ObjectReference tiles=reference(read(value,chunkType,"l","Ljava/util/Map;",1));
                ArrayReference entities=(ArrayReference)read(value,chunkType,"m","[Ljava/util/List;",1);
                require(entities.length()==8,"eight original entity slice owners");
                for(Value slice:entities.getValues()) {
                    ObjectReference actual=reference(slice);slices.add(object("id",actual.uniqueID(),"size",integer(bootstrap(actual,"size","I"))));
                }
                chunks.add(object("chunk",chunk(value),"blank",blank,"heightmap",byteArray(value,"h",256),"minimum_height",integer(read(value,chunkType,"i","I",1)),"data",blank?null:((Map<?,?>)localChunk(value)).get("data"),
                        "world_id",reference(read(value,chunkType,"d",client?"Lfd;":"Ldj;",1)).uniqueID(),"array_ids",owners,
                        "tile_map",object("id",tiles.uniqueID(),"size",integer(bootstrap(tiles,"size","I"))),"entity_slices",slices));
            }
            ObjectReference info=reference(read(world,worldType,"x",client?"Lei;":"Lct;",4));ReferenceType infoType=originalType(client?"ei":"ct");
            ObjectReference provider=reference(read(world,worldType,"t",client?"Lxa;":"Los;",17));
            return object("cache",cacheState,"chunks",chunks,"world_time",worldTime(world),"depth",integer(worldType.getValue(field(worldType,"A","I",8))),"drain_depth",integer(read(world,worldType,"M","I",2)),
                    "no_sky",((BooleanValue)read(provider,originalType(client?"xa":"os"),"e","Z",1)).value(),"spawn",Arrays.asList(integer(read(info,infoType,"b","I",2)),integer(read(info,infoType,"d","I",2))),
                    "world_force_load",((BooleanValue)read(world,worldType,"y","Z",1)).value(),"provider_force_load",null);
        }
        void begin(BreakpointEvent event) throws Exception {
            List<Value> args=event.thread().frame(0).getArgumentValues();int index=integer(args.get(2));
            require(active==-1&&index==operations.size()&&index<callCount,"ordered complete markers");
            ObjectReference actualWorld=reference(args.get(0)), actualCache=reference(args.get(1));
            if(index==0) {
                world=actualWorld;cache=actualCache;thread=event.thread();require(thread.name().equals("main"),"measured main");
                if(!client) {
                    if(scenario.equals("population")) require(spawnEntries==1&&randomSteps==0&&world.equals(spawnWorld)&&thread.equals(spawnThread),"fresh fixed origin setup without random walk");
                    else require(spawnEntries==0&&randomSteps==0&&spawnWorld==null&&spawnThread==null,"saved metadata constructor bypasses fresh spawn selection");
                    spawnEntry.disable();spawnStep.disable();
                }
                require(scenario.equals("population")?timeState==0:timeState==4&&world.equals(timeWorld)&&thread.equals(timeThread),"actual time API proof before measured load");
                install();
            } else require(world.equals(actualWorld)&&cache.equals(actualCache)&&thread.equals(event.thread()),"same measured world/cache/main");
            active=index;chunk=null;events=new ArrayList<>();seams=new ArrayList<>();generatedCount=0;finishingCount=0;populationCount=0;
            require(seamStack.isEmpty()&&!finishing&&opaqueDepth==0,"no pending callbacks at begin");beginThreads=inventory(event,"begin",index);beginRandom=seeds();
        }
        void record(String kind,ObjectReference result) throws Exception {
            require(active>=0&&events.size()<1024&&++eventCount<=4096,"bounded exact lifecycle events");
            StackFrame frame=thread.frame(0);Location site=frame.location();List<Object> args=new ArrayList<>(), frames=new ArrayList<>();
            for(Value value:frame.getArgumentValues()) {
                if(value instanceof IntegerValue)args.add(integer(value));
                else if(value instanceof ObjectReference)args.add(object("id",((ObjectReference)value).uniqueID(),"class",((ObjectReference)value).referenceType().name()));
                else require(value==null,"known callback argument shape");
            }
            require(thread.frameCount()<=MAX_FRAMES,"bounded original call stack");
            for(StackFrame caller:thread.frames()) {
                Location location=caller.location();if(world.referenceType().classLoader().equals(location.declaringType().classLoader()))
                    frames.add(object("class",location.declaringType().name(),"method",location.method().name(),"signature",location.method().signature(),"code_index",location.codeIndex(),"receiver",caller.thisObject()==null?null:caller.thisObject().uniqueID()));
            }
            events.add(object("ordinal",events.size(),"kind",kind,"opaque_interior",opaqueDepth>0,"class",site.declaringType().name(),"method",site.method().name(),"signature",site.method().signature(),"code_index",site.codeIndex(),"thread_id",thread.uniqueID(),"receiver",frame.thisObject()==null?null:frame.thisObject().uniqueID(),"arguments",args,"result",result==null?null:chunk(result),"chunk",chunk==null?null:chunk(chunk),"cache",cacheSnapshot(),"original_frames",frames));
        }
        void requireCaller(Method method,ObjectReference receiver) throws Exception {
            require(thread.frameCount()>=2&&thread.frame(1).location().method().equals(method)&&receiver.equals(thread.frame(1).thisObject()),"exact direct original caller and receiver");
        }
        void startSeam(String kind) throws Exception {
            require(seams.size()<32,"bounded complete external callbacks");StackFrame frame=thread.frame(0);List<Value> args=frame.getArgumentValues();
            int shift=kind.equals("generate")?0:1;int x=integer(args.get(shift)),z=integer(args.get(shift+1));
            if(kind.equals("load")) {
                require(frame.thisObject().equals(loader)&&reference(args.get(0)).equals(world),"actual persistent loader/context");requireCaller(wrapperLoad,cache);
            } else if(kind.equals("generate")) {
                require(frame.thisObject().equals(generator),"retained generator");requireCaller(acquire,cache);++generatedCount;
            } else {
                require(frame.thisObject().equals(generator)&&reference(args.get(0)).equals(cache),"actual retained provider and coordinator argument");
                require(thread.frameCount()>=2&&thread.frame(1).thisObject().equals(cache)&&thread.frame(1).location().method().name().equals("a")&&thread.frame(1).location().codeIndex()==36,"actual population wrapper callsite");
                require(populationTarget!=null&&((BooleanValue)read(populationTarget,chunkType,"n","Z",1)).value(),"actual b-return target already populated before provider");
                ++populationCount;
            }
            Map<String,Object> seam=object("ordinal",seams.size(),"kind",kind,"x",x,"z",z,"receiver_id",frame.thisObject().uniqueID(),"provider_argument_id",kind.equals("populate")?cache.uniqueID():null,"population_target_id",kind.equals("populate")?populationTarget.uniqueID():null,
                    "opaque_interior",opaqueDepth>0,"before",boundary(null),"after",null,"result",null,"entry_event",events.size());
            seams.add(seam);seamStack.add(seam);if(kind.equals("populate")) {
                ++opaqueDepth;for(BreakpointRequest request:quietInsidePopulation)request.disable();
            }
        }
        void endSeam(String kind,ObjectReference result) {
            require(!seamStack.isEmpty(),"matching callback entry");Map<String,Object> seam=seamStack.remove(seamStack.size()-1);
            require(seam.get("kind").equals(kind),"ordered direct callback return");
            seam.put("after",boundary(result));seam.put("result",result==null?null:chunk(result));seam.put("return_event",events.size());
            if(kind.equals("populate")) {
                require(--opaqueDepth>=0,"matched opaque population depth");if(opaqueDepth==0)for(BreakpointRequest request:quietInsidePopulation)request.enable();
            }
        }
        void identity(BreakpointEvent event) throws Exception {
            require(++identityMarkers<=1024,"bounded identity marker count");
            List<Value> args=event.thread().frame(0).getArgumentValues();ObjectReference value=reference(args.get(0));
            require(args.get(1)instanceof StringReference&&event.thread().name().equals("main"),"ordinary helper identity marker");String name=((StringReference)args.get(1)).value();
            require(name.matches("r[0-9]+")&&objectReferences.size()<1024,"bounded deterministic reference");Long previous=objectIDs.putIfAbsent(name,value.uniqueID());
            require(previous==null||previous.longValue()==value.uniqueID(),"stable helperref/objectID");String className=value instanceof ArrayReference?value.referenceType().signature().replace('/','.'):value.referenceType().name();
            objectReferences.putIfAbsent(name,object("reference",name,"object_id",value.uniqueID(),"class",className,"loader_id",value.referenceType().classLoader()==null?null:value.referenceType().classLoader().uniqueID()));
        }
        void time(BreakpointEvent event,String marker) throws Exception {
            require(active==-1&&beginCount==0&&scenario.equals("load"),"time setter only before measured load");StackFrame frame=event.thread().frame(0);List<Value> args=frame.getArgumentValues();
            if(marker.equals("timeBegin")) {
                require(timeState==0&&((LongValue)args.get(1)).value()==1234,"first bounded time API marker");timeWorld=reference(args.get(0));timeThread=event.thread();timeState=1;
            } else if(marker.equals("timeEnd")) {
                require(timeState==3&&timeThread.equals(event.thread())&&timeWorld.equals(reference(args.get(0)))&&((LongValue)args.get(1)).value()==1234,"complete time API proof");timeState=4;
            } else {
                require(timeThread.equals(event.thread())&&timeWorld.equals(frame.thisObject())&&((LongValue)args.get(0)).value()==1234&&frame.location().method().equals(setTime),"actual original public time setter receiver/thread/value");
                long pc=frame.location().codeIndex();require(timeState==1&&pc==0||timeState==2&&pc==8,"ordered actual time setter entry/return");++timeState;
            }
            ObjectReference info=reference(read(timeWorld,timeWorld.referenceType(),"x",client?"Lei;":"Lct;",4));
            require(info.referenceType().name().equals(client?"ei":"ct")&&info.referenceType().classLoader().equals(timeWorld.referenceType().classLoader()),"actual world metadata time owner");
            String current=String.format("%016x",((LongValue)read(info,info.referenceType(),"e","J",2)).value());
            timeProof.add(object("kind",marker,"class",frame.location().declaringType().name(),"method",frame.location().method().name(),"signature",frame.location().method().signature(),"code_index",frame.location().codeIndex(),"thread_id",event.thread().uniqueID(),"world_id",timeWorld.uniqueID(),"metadata_id",info.uniqueID(),"world_time",current,"value",1234));
        }
        void stopped(BreakpointEvent event) throws Exception {
            String marker=(String)event.request().getProperty("marker");
            if(marker.equals("identity")){identity(event);return;}
            if(marker.startsWith("time")){time(event,marker);return;}
            if(marker.equals("spawn_entry")) {
                require(beginCount==0&&spawnEntries++==0,"one actual constructor spawn");spawnWorld=event.thread().frame(0).thisObject();spawnThread=event.thread();return;
            }
            if(marker.equals("spawn_step")) {
                require(beginCount==0&&spawnEntries==1&&spawnThread.equals(event.thread())&&spawnWorld.equals(event.thread().frame(0).thisObject())&&++randomSteps<=65536,"bounded constructor random steps");return;
            }
            if(marker.equals("begin")){++beginCount;begin(event);return;}
            require(active>=0&&thread.equals(event.thread()),"original event inside measured main operation");StackFrame frame=thread.frame(0);long pc=event.location().codeIndex();
            if(marker.equals("end")) {
                ++endCount;List<Value> args=frame.getArgumentValues();require(integer(args.get(1))==active&&seamStack.isEmpty()&&!finishing&&opaqueDepth==0&&generatedCount==finishingCount,"complete operation and callback/finish boundaries");
                if(scenario.equals("load"))require(generatedCount==0&&finishingCount==0&&populationCount==0,"loaded chunks never regenerate/refinish/populate");
                operations.add(object("index",active,"thread_id",thread.uniqueID(),"world_id",world.uniqueID(),"cache_id",cache.uniqueID(),"generator_id",generator.uniqueID(),"loader_id",loader.uniqueID(),"result",chunk((ObjectReference)args.get(0)),
                        "begin_threads",beginThreads,"end_threads",inventory(event,"end",active),"begin_random",beginRandom,"end_random",seeds(),"generated_count",generatedCount,"finish_count",finishingCount,"population_count",populationCount,"events",events,"seams",seams));
                active=-1;cacheExits.disable();loaderExits.disable();generatorExits.disable();registrationExits.disable();return;
            }
            if(marker.equals("acquire")||marker.equals("get")||marker.equals("load_wrapper")||marker.equals("population"))require(cache.equals(frame.thisObject()),"retained cache receiver");
            if(marker.equals("population")&&pc==0)populationTarget=null;
            if(marker.equals("acquire")&&pc==0&&opaqueDepth>0){record("opaque_acquire",null);return;}
            if(marker.equals("get")||marker.equals("acquire")&&pc==391)cacheExits.enable();
            if(marker.equals("load_wrapper")) {
                requireCaller(acquire,cache);if(pc==0||pc==40)cacheExits.enable();
            }
            if(marker.equals("loader")){cacheExits.disable();startSeam("load");}
            if(marker.equals("loader_ready")){require(loader.equals(frame.thisObject()),"actual loader return site");loaderExits.enable();}
            if(marker.equals("generate")){cacheExits.disable();startSeam("generate");}
            if(marker.equals("generator_ready")){require(generator.equals(frame.thisObject())&&!finishing,"complete generator return");generatorExits.enable();}
            if(marker.equals("provider_populate")){cacheExits.disable();startSeam("populate");}
            if(marker.equals("provider_ready")){require(generator.equals(frame.thisObject())&&opaqueDepth>0,"complete opaque provider body return");generatorExits.enable();}
            if(marker.equals("finish")) {
                requireCaller(generate,generator);
                if(pc==0){require(!finishing,"one full finish");finishing=true;chunk=frame.thisObject();cacheExits.disable();getEntry.disable();}
                else {require(finishing&&chunk.equals(frame.thisObject()),"matched full finish exit");finishing=false;++finishingCount;if(opaqueDepth==0)getEntry.enable();}
            }
            if(marker.equals("activate")){requireCaller(acquire,cache);chunk=frame.thisObject();}
            if(marker.equals("register_tiles")||marker.equals("register_entities")){requireCaller(activate,chunk);require(world.equals(frame.thisObject()),"actual registration world");registrationExits.enable();}
            record(marker,null);
        }
        void returned(MethodExitEvent event) throws Exception {
            String kind=exitMethods.get(event.method());if(kind==null||active<0)return;require(thread.equals(event.thread()),"paired original return thread");
            if(event.request().equals(cacheExits))cacheExits.disable();if(event.request().equals(loaderExits))loaderExits.disable();
            if(event.request().equals(generatorExits))generatorExits.disable();if(event.request().equals(registrationExits))registrationExits.disable();
            Value value=event.returnValue();ObjectReference result=value instanceof ObjectReference?(ObjectReference)value:null;
            if(event.method().equals(actualLoad)){require(value==null||value instanceof ObjectReference,"genuine loader result");endSeam("load",result);cacheExits.enable();}
            if(event.method().equals(generate)){require(result!=null&&result.equals(chunk)&&!finishing,"nonnull complete generated chunk");endSeam("generate",result);}
            if(event.method().equals(providerPopulate))endSeam("populate",null);
            if(event.method().equals(get)&&thread.frameCount()>=2) {
                Location caller=thread.frame(1).location();
                if(caller.declaringType().equals(cacheType)&&caller.method().name().equals("a")&&caller.method().signature().equals(client?"(Lcl;II)V":"(Lbl;II)V")&&caller.codeIndex()==3)
                    populationTarget=result;
            }
            if(event.method().equals(wrapperLoad)&&result!=null)chunk=result;
            record(kind,result);
        }
        boolean beforeMeasurement(){return beginCount==0&&endCount==0&&operations.isEmpty()&&active==-1&&eventCount==0&&identityMarkers==0&&objectReferences.isEmpty()&&objectIDs.isEmpty()&&world==null&&cache==null&&generator==null&&chunk==null&&seamStack.isEmpty()&&!finishing&&timeState==0&&timeProof.isEmpty();}
        Object setupProof(){return object("scope","persistent-base-world-api","spawn_entries",spawnEntries,"random_steps",randomSteps,"same_world",spawnWorld==null?null:world.equals(spawnWorld),"same_thread",spawnThread==null?null:thread.equals(spawnThread),"time_api_complete",scenario.equals("load")?timeState==4:null);}
    }
    private static void saveLog(Path path, Future<byte[]> bytes) {
        if (path == null || bytes == null || Files.exists(path, LinkOption.NOFOLLOW_LINKS)) return;
        try { Files.write(path, bytes.get(2, TimeUnit.SECONDS), StandardOpenOption.CREATE_NEW); }
        catch (Exception failure) { System.err.println("Child log capture failed: " + failure); }
    }
    public static void main(String[] args) {
        Process process = null; VirtualMachine vm = null; Future<byte[]> stdout = null, stderr = null;
        Path stdoutPath = null, stderrPath = null;
        ExecutorService readers = Executors.newFixedThreadPool(2); long started = System.nanoTime();
        long deadline = started + TimeUnit.SECONDS.toNanos(90); int status = 1;
        try {
            require(args.length == 7, "jar side load|population seed_hex owned_root snapshots_json trace_json");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            String scenario = args[2];
            require((scenario.equals("load") || scenario.equals("population")) && args[3].equals("0000000000000017"), "explicit callback scope and seed23");
            Path trace = Path.of(args[args.length-1]).toAbsolutePath().normalize(), snapshot = Path.of(args[5]).toAbsolutePath().normalize();
            stdoutPath = Path.of(trace + ".child-stdout.txt"); stderrPath = Path.of(trace + ".child-stderr.txt");
            Path unavailable = Path.of(trace + ".unavailable.json");
            for (Path output : Arrays.asList(trace, snapshot, stdoutPath, stderrPath, unavailable)) newOutput(output);
            require(new HashSet<>(Arrays.asList(trace,snapshot,stdoutPath,stderrPath,unavailable)).size() == 5, "distinct new output paths");
            Path jar = Path.of(args[0]).toRealPath(); String jarHash = hash(Files.readAllBytes(jar));
            require(jarHash.equals(client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"), "pinned original archive");
            Path java = Path.of(System.getProperty("java.home"), "bin", System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            List<String> command = new ArrayList<>(Arrays.asList(java.toString(), "-Xint", "-Xmx512m",
                    "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", System.getProperty("java.class.path"), "CacheCallbacksObservation"));
            command.addAll(Arrays.asList(Arrays.copyOf(args,args.length-1))); process = new ProcessBuilder(command).start(); Process child = process;
            AtomicLong captured = new AtomicLong(); CompletableFuture<String> startup = new CompletableFuture<>();
            stdout = readers.submit(() -> capture(child.getInputStream(), startup, captured)); stderr = readers.submit(() -> capture(child.getErrorStream(), null, captured));
            String listening = startup.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(listening.matches("Listening for transport dt_socket at address: [0-9]+"), "loopback ephemeral JDWP startup");
            String port = listening.substring(listening.lastIndexOf(' ')+1);
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                    .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String, Connector.Argument> connection = connector.defaultArguments(); connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(port); connection.get("timeout").setValue(Long.toString(remaining(deadline))); vm = connector.attach(connection);
            require(vm.canGetMethodReturnValues() && vm.canGetBytecodes(), "actual return values and bytecodes required");
            String vmName = vm.name(), vmVersion = vm.version(); Session session = new Session(vm,client,scenario);
            boolean alive = true; vm.resume();
            while (alive && System.nanoTime() < deadline) {
                EventSet set;
                try { set = vm.eventQueue().remove(Math.min(1000,remaining(deadline))); } catch (VMDisconnectedException closed) { break; }
                if (set == null) continue;
                for (Event event : set) {
                    require(++session.deliveredEvents <= 65536, "bounded total JDI event delivery");
                    if (event instanceof ClassPrepareEvent) session.prepared((ClassPrepareEvent) event);
                    else if (event instanceof BreakpointEvent) session.stopped((BreakpointEvent) event);
                    else if (event instanceof MethodExitEvent) session.returned((MethodExitEvent) event);
                    else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) alive = false;
                }
                try { set.resume(); } catch (VMDisconnectedException closed) { alive = false; }
            }
            require(process.waitFor(remaining(deadline),TimeUnit.MILLISECONDS), "bounded child exit");
            byte[] out = stdout.get(remaining(deadline),TimeUnit.MILLISECONDS), err = stderr.get(remaining(deadline),TimeUnit.MILLISECONDS);
            Files.write(stdoutPath,out,StandardOpenOption.CREATE_NEW); Files.write(stderrPath,err,StandardOpenOption.CREATE_NEW);
            require(hash(Files.readAllBytes(jar)).equals(jarHash), "original archive preserved");
            if (process.exitValue() == 77) {
                require(session.beforeMeasurement() && !Files.exists(snapshot,LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(snapshot), "exit77 only before all measurements and without snapshots");
                Files.writeString(unavailable,json(object("schema",1,"status","unavailable","stage","original_setup","child_exit",77,
                        "side",args[1],"mode",args[2],"scenario",scenario,"jar_sha256",jarHash,"begin_markers",0,"end_markers",0,
                        "completed_operations",0,"identity_markers",session.identityMarkers,"snapshot_started",false,"snapshot_exists",false,"child_command",command,"debuggee_mode","interpreted","elapsed_ms",TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-started)))+"\n",StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW);
                status = 77;
            } else {
                require(process.exitValue() == 0,"original child exit " + process.exitValue() + ":\n" + new String(err,StandardCharsets.UTF_8));
                require(session.operations.size() == session.callCount && session.beginCount == session.callCount && session.endCount == session.callCount
                        && session.active == -1 && session.seamStack.isEmpty() && !session.finishing, "all complete operation/callback boundaries");
                require(Files.isRegularFile(snapshot,LinkOption.NOFOLLOW_LINKS) && Files.size(snapshot) <= MAX_BYTES,"complete bounded snapshots");
                Map<String,Object> record = object("schema",1,"scope","cache-callback-boundary","side",args[1],"scenario",scenario,"seed_hex",args[3],"jar_sha256",jarHash,
                        "can_get_method_return_values",true,"can_get_bytecodes",true,"vm_name",vmName,"vm_version",vmVersion,"child_command",command,"debuggee_mode","interpreted",
                        "listen_address","127.0.0.1","port",Integer.parseInt(port),"setup_proof",session.setupProof(),"bindings",session.bindings,"time_proof",session.timeProof,"object_references",new ArrayList<>(session.objectReferences.values()),"operations",session.operations,
                        "jdi_event_count",session.deliveredEvents,"child_exit",0,"child_stdout",object("file",stdoutPath.getFileName().toString(),"bytes",out.length,"sha256",hash(out)),
                        "child_stderr",object("file",stderrPath.getFileName().toString(),"bytes",err.length,"sha256",hash(err)),
                        "elapsed_ms",TimeUnit.NANOSECONDS.toMillis(System.nanoTime()-started));
                byte[] encoded = (json(record)+"\n").getBytes(StandardCharsets.UTF_8); require(encoded.length <= MAX_BYTES,"complete trace output bound");
                Files.write(trace,encoded,StandardOpenOption.CREATE_NEW); status = 0;
                System.out.println(args[1]+" "+scenario+" operations="+session.callCount+" events="+session.eventCount);
            }
        } catch (Throwable failure) { System.err.println("FAILURE: cache callback controller"); failure.printStackTrace(System.err); }
        finally {
            if (vm != null) try { vm.dispose(); } catch (RuntimeException ignored) { }
            if (process != null && process.isAlive()) {
                process.destroyForcibly(); try { process.waitFor(2,TimeUnit.SECONDS); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            }
            saveLog(stdoutPath,stdout); saveLog(stderrPath,stderr); readers.shutdownNow();
        }
        System.exit(status);
    }
}
