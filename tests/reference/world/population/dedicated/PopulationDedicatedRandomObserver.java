import com.sun.jdi.ArrayReference;
import com.sun.jdi.BooleanValue;
import com.sun.jdi.Bootstrap;
import com.sun.jdi.ClassType;
import com.sun.jdi.DoubleValue;
import com.sun.jdi.Field;
import com.sun.jdi.FloatValue;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.Location;
import com.sun.jdi.LongValue;
import com.sun.jdi.Method;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.StackFrame;
import com.sun.jdi.StringReference;
import com.sun.jdi.ThreadReference;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.VoidValue;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.BreakpointEvent;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventQueue;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.MethodEntryEvent;
import com.sun.jdi.event.MethodExitEvent;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.event.VMStartEvent;
import com.sun.jdi.request.BreakpointRequest;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import com.sun.jdi.request.EventRequestManager;
import com.sun.jdi.request.MethodEntryRequest;
import com.sun.jdi.request.MethodExitRequest;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Passive, original-only JDI experiment. The outer driver owns both processes.
 * No target invocation, field assignment, clock replacement or reseeding occurs.
 * This observer does not qualify a native implementation or an uninstrumented run.
 */
public final class PopulationDedicatedRandomObserver {
    static final String TARGET = "PopulationDedicatedObservation";
    static final String EXPERIMENT = TARGET + "$Experiment";
    static final String ORIGINAL = TARGET + "$Original";
    static final String RANDOM = "java.util.Random";
    static final String SYSTEM = "java.lang.System";
    static final String HOLDER = "java.lang.Math$RandomNumberGeneratorHolder";
    static final String OPERATION_SIGNATURE = "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;";
    static final long INITIAL_U = 8682522807148012L;
    static final long U_MULTIPLIER = 1181783497276652981L;
    static final long MULTIPLIER = 0x5deece66dL, MASK = (1L << 48) - 1;
    static final long TIME_LIMIT = TimeUnit.SECONDS.toNanos(180);
    static final int EVENT_LIMIT = 25000, CONSTRUCTOR_LIMIT = 4096, MIRROR_LIMIT = 16384;
    static final int STACK_LIMIT = 512, DRAW_DEPTH_LIMIT = 32, OUTPUT_LIMIT = 16 * 1024 * 1024;
    static final String[] ACTIONS = {"origin_c", "origin_c", "origin_c", "origin_c", "origin_c",
        "world_time_1234", "full_world_save_0", "full_world_save_1", "protected_drop_0_0",
        "far_c", "far_c", "far_c", "far_c", "drop_16_0", "cache_maintenance",
        "reload_c_16_0", "repeated_c_16_0"};
    static final int[][] COORDINATES = {{0,0},{1,0},{0,1},{1,1},{1,1},null,null,null,{0,0},
        {16,0},{17,0},{16,1},{17,1},{16,0},null,{16,0},{16,0}};

    static final class Rejected extends RuntimeException {
        private static final long serialVersionUID = 1L;
        Rejected(String reason) { super(reason); }
    }
    static void need(boolean condition, String reason) {
        if (!condition) throw new Rejected(reason);
    }
    static String hex(long value) { return String.format(Locale.ROOT, "%016x", value); }
    static String number(long value) { return Long.toString(value); }

    static final class Trace {
        int bytes;
        void row(String... fields) {
            StringBuilder line = new StringBuilder();
            for (String field : fields) {
                need(field != null && field.length() <= 8192, "trace-field-bound");
                for (int i = 0; i < field.length(); ++i)
                    need(field.charAt(i) >= 32 && field.charAt(i) <= 126, "trace-field-ascii");
                if (line.length() != 0) line.append('\t');
                line.append(field);
            }
            need(line.length() <= 16384, "trace-line-bound");
            line.append('\n');
            byte[] encoded = line.toString().getBytes(StandardCharsets.US_ASCII);
            need(encoded.length <= OUTPUT_LIMIT - bytes, "trace-total-bound");
            bytes += encoded.length;
            System.out.write(encoded, 0, encoded.length);
            need(!System.out.checkError(), "trace-output-error");
        }
        void text(String name, String text) {
            byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
            need(bytes.length <= 4096, "metadata-text-bound");
            StringBuilder bits = new StringBuilder(bytes.length * 2);
            for (byte b : bytes) bits.append(String.format(Locale.ROOT, "%02x", b & 255));
            row("meta", name + "_utf8hex", bits.toString());
        }
    }

    record Held(ObjectReference mirror, String label) {}
    record State(ObjectReference owner, ObjectReference seed, String ownerID, String seedID,
                 long seed48, long cached, boolean flag) {
        boolean same(State other) {
            return other != null && owner.equals(other.owner) && seed.equals(other.seed)
                && seed48 == other.seed48 && cached == other.cached && flag == other.flag;
        }
    }
    static final class ConstructorCall {
        final int index;
        final ObjectReference owner;
        final long beforeU;
        final boolean ordinary;
        long committed, sample, argument;
        int phase = 1, scrambleReturns, baseSetSeedReturns;
        State delegated;
        ConstructorCall(int index, ObjectReference owner, long beforeU, boolean ordinary) {
            this.index = index; this.owner = owner; this.beforeU = beforeU; this.ordinary = ordinary;
        }
    }
    static final class Draw {
        final int index, depth;
        final Method method;
        final State before;
        final List<Value> arguments;
        Draw(int index, int depth, Method method, State before, List<Value> arguments) {
            this.index = index; this.depth = depth; this.method = method;
            this.before = before; this.arguments = arguments;
        }
    }

    static final class Session {
        final VirtualMachine vm;
        final EventRequestManager requests;
        final Trace out;
        final long started;
        final Map<Long,Held> held = new LinkedHashMap<>();
        final Map<Long,State> constructed = new LinkedHashMap<>();
        final Deque<Draw> draws = new ArrayDeque<>();
        ReferenceType target, experimentType, random, system, mathHolder;
        ThreadReference main;
        ObjectReference appLoader, gameLoader, original, experiment, world, cache;
        ObjectReference worldRandom, worldSeed, uniquifier, mathOwner, mathSeed;
        Field randomSeed, randomCache, randomFlag, randomU, atomicValue;
        BreakpointRequest defaultEntry, uniqueEntry, delegateEntry;
        MethodExitRequest constructorExits, clocks, drawExits;
        final List<BreakpointRequest> drawEntries = new ArrayList<>();
        ConstructorCall activeConstructor;
        State beforeOperation, lastBoundary, lastDraw;
        long currentU = INITIAL_U, expectedFarSeed;
        int nextOperation, activeOperation = -1;
        int delivered, defaults, uniqueReturns, clockReturns, delegateReturns, drawEntriesCount, drawReturns;
        int farScopes, stackSerial, breakpointEvents, prepareEvents, validatedScrambleReturns;
        int ordinaryDefaults, subclassDefaults, validatedBaseSetSeedReturns;
        boolean startedVM, died, disconnected, targetExit, far, uWitnessed;

        Session(VirtualMachine vm, Trace out, long started) {
            this.vm = vm; this.requests = vm.eventRequestManager(); this.out = out; this.started = started;
        }
        long remainingMillis() {
            long remaining = TIME_LIMIT - (System.nanoTime() - started);
            need(remaining > 0, "observer-180-second-deadline");
            return Math.max(1, TimeUnit.NANOSECONDS.toMillis(remaining));
        }
        void mainThread(ThreadReference thread) {
            need(main != null && main.equals(thread), "unexpected-asynchronous-source-thread");
        }
        String hold(ObjectReference value) {
            need(value != null, "nonnull-held-mirror");
            long id = value.uniqueID();
            Held known = held.get(id);
            if (known != null) {
                need(known.mirror().equals(value), "retained-mirror-identity-reuse");
                return known.label();
            }
            need(held.size() < MIRROR_LIMIT, "retained-mirror-bound");
            value.disableCollection();
            String label = "o" + (held.size() + 1);
            held.put(id, new Held(value, label));
            out.row("identity", label, value.referenceType().name());
            return label;
        }
        Field field(ReferenceType type, String name, String signature, boolean isStatic) {
            need(type.isPrepared(), "field-owner-not-prepared");
            Field found = null;
            for (Field candidate : type.fields()) {
                if (candidate.declaringType().equals(type) && candidate.name().equals(name)
                        && candidate.signature().equals(signature) && candidate.isStatic() == isStatic) {
                    need(found == null, "duplicate-field-binding"); found = candidate;
                }
            }
            need(found != null, "missing-field-" + type.name() + "." + name);
            return found;
        }
        Method method(ReferenceType type, String name, String signature, boolean isStatic) {
            Method found = null;
            for (Method candidate : type.methodsByName(name, signature)) {
                if (candidate.declaringType().equals(type) && candidate.isStatic() == isStatic) {
                    need(found == null, "duplicate-method-binding"); found = candidate;
                }
            }
            need(found != null && !found.isAbstract() && !found.isNative(),
                    "missing-bytecode-method-" + type.name() + "." + name);
            return found;
        }
        ObjectReference reference(Value value) {
            need(value instanceof ObjectReference, "nonnull-reference-field");
            return (ObjectReference) value;
        }
        long longValue(Value value) {
            need(value instanceof LongValue, "long-value-type"); return ((LongValue) value).value();
        }
        int intValue(Value value) {
            need(value instanceof IntegerValue, "int-value-type"); return ((IntegerValue) value).value();
        }
        boolean booleanValue(Value value) {
            need(value instanceof BooleanValue, "boolean-value-type"); return ((BooleanValue) value).value();
        }
        ObjectReference read(ObjectReference owner, String name, String signature) {
            return reference(owner.getValue(field(owner.referenceType(), name, signature, false)));
        }
        void bootstrap(ReferenceType type, String name) {
            need(type.name().equals(name) && type.classLoader() == null, "bootstrap-type-binding");
        }
        BreakpointRequest breakpoint(Method method, long pc, String role, boolean enable) {
            Location location = method.locationOfCodeIndex(pc);
            need(location != null && location.codeIndex() == pc, "exact-breakpoint-location");
            BreakpointRequest request = requests.createBreakpointRequest(location);
            request.setSuspendPolicy(EventRequest.SUSPEND_ALL); request.putProperty("role", role);
            if (enable) request.enable();
            out.row("breakpoint", role, method.declaringType().name(), method.name(), method.signature(), number(pc));
            return request;
        }
        BreakpointRequest entry(Method method, String role, boolean enable) {
            need(method.location() != null && method.location().codeIndex() == 0, "entry-PC-zero");
            return breakpoint(method, 0, role, enable);
        }
        void bindRandom(ReferenceType type) {
            bootstrap(type, RANDOM);
            if (random != null) { need(random.equals(type), "Random-class-identity-changed"); return; }
            random = type;
            randomSeed = field(type, "seed", "Ljava/util/concurrent/atomic/AtomicLong;", false);
            randomCache = field(type, "nextNextGaussian", "D", false);
            randomFlag = field(type, "haveNextNextGaussian", "Z", false);
            randomU = field(type, "seedUniquifier", "Ljava/util/concurrent/atomic/AtomicLong;", true);
            defaultEntry = entry(method(type, "<init>", "()V", false), "default-entry", true);
            uniqueEntry = entry(method(type, "seedUniquifier", "()J", true), "uniquifier-entry", false);
            delegateEntry = entry(method(type, "<init>", "(J)V", false), "delegate-entry", false);
            constructorExits = requests.createMethodExitRequest(); constructorExits.addClassFilter(RANDOM);
            constructorExits.addThreadFilter(main); constructorExits.setSuspendPolicy(EventRequest.SUSPEND_ALL);
            constructorExits.putProperty("role", "constructor-exit");
            out.row("binding", "random", type.name(), "bootstrap", "class-not-actively-initialized-by-observer");
        }
        void bindSystem(ReferenceType type) {
            bootstrap(type, SYSTEM);
            if (system != null) { need(system.equals(type), "System-class-identity-changed"); return; }
            system = type;
            entry(method(type, "exit", "(I)V", true), "target-exit", true);
            clocks = requests.createMethodExitRequest(); clocks.addClassFilter(type); clocks.addThreadFilter(main);
            clocks.setSuspendPolicy(EventRequest.SUSPEND_ALL); clocks.putProperty("role", "clock-exit");
        }
        boolean helperName(String name) {
            for (String base : new String[]{TARGET, "NaturalPopulationObservation", "ChunkLifecycleObservation",
                    "ChunkObservation", "CacheCallbacksObservation"})
                if (name.equals(base) || name.startsWith(base + "$")) return true;
            return false;
        }
        void bindTarget(ReferenceType type) {
            need(type.name().equals(TARGET) && type.classLoader() != null, "target-class-binding");
            if (target != null) { need(target.equals(type), "target-class-identity-changed"); return; }
            target = type; appLoader = type.classLoader();
            out.row("binding", "helper_loader", hold(appLoader));
            method(type, "main", "([Ljava/lang/String;)V", true);
        }
        void bindExperiment(ReferenceType type) {
            need(target != null && type.name().equals(EXPERIMENT) && appLoader.equals(type.classLoader()),
                    "experiment-class-binding");
            if (experimentType != null) { need(experimentType.equals(type), "experiment-class-identity-changed"); return; }
            experimentType = type;
            Method operation = method(type, "operation", OPERATION_SIGNATURE, false);
            byte[] code = operation.bytecodes();
            need(code.length > 1 && code.length <= 65535 && (code[code.length - 1] & 255) == 0xb0,
                    "operation-must-end-with-actual-ARETURN");
            entry(operation, "operation-before", true);
            breakpoint(operation, code.length - 1L, "operation-after", true);
            out.row("binding", "operation_bytecode_length", number(code.length), "final_opcode", "b0");
        }
        void bind(ReferenceType type) {
            switch (type.name()) {
                case RANDOM -> bindRandom(type);
                case SYSTEM -> bindSystem(type);
                case TARGET -> bindTarget(type);
                case EXPERIMENT -> bindExperiment(type);
                default -> throw new Rejected("unexpected-class-prepare");
            }
        }
        long readU() {
            need(random != null && random.isInitialized() && !random.failedToInitialize(), "Random-not-initialized");
            ObjectReference current = reference(random.getValue(randomU));
            bootstrap(current.referenceType(), "java.util.concurrent.atomic.AtomicLong");
            if (uniquifier == null) {
                uniquifier = current; hold(current);
                atomicValue = field(current.referenceType(), "value", "J", false);
            }
            need(uniquifier.equals(current), "shared-uniquifier-owner-changed");
            return longValue(current.getValue(atomicValue));
        }
        void stableU() {
            need(readU() == currentU, "unobserved-shared-uniquifier-advance");
            if (!uWitnessed) {
                need(currentU == INITIAL_U, "fresh-Random-profile-admission"); uWitnessed = true;
                out.row("uniquifier", "fresh_initial", hold(uniquifier), hex(currentU));
            }
        }
        boolean randomInstance(ReferenceType type) {
            while (type instanceof ClassType current) {
                if (current.equals(random)) return true;
                type = current.superclass();
            }
            return false;
        }
        State state(ObjectReference owner) {
            need(random != null && randomInstance(owner.referenceType()), "actual-Random-base-owner");
            ObjectReference seed = reference(owner.getValue(randomSeed));
            bootstrap(seed.referenceType(), "java.util.concurrent.atomic.AtomicLong");
            if (atomicValue == null) atomicValue = field(seed.referenceType(), "value", "J", false);
            long seed48 = longValue(seed.getValue(atomicValue));
            need((seed48 & ~MASK) == 0, "Random-seed48-range");
            Value cached = owner.getValue(randomCache);
            need(cached instanceof DoubleValue, "Random-cache-double-type");
            return new State(owner, seed, hold(owner), hold(seed), seed48,
                    Double.doubleToRawLongBits(((DoubleValue) cached).value()), booleanValue(owner.getValue(randomFlag)));
        }
        String continuation(State state) {
            long seed = state.seed48(); StringBuilder result = new StringBuilder();
            for (int i = 0; i < 8; ++i) {
                seed = (seed * MULTIPLIER + 11) & MASK; int high = (int) (seed >>> 16);
                seed = (seed * MULTIPLIER + 11) & MASK; int low = (int) (seed >>> 16);
                if (i != 0) result.append(',');
                result.append(hex(((long) high << 32) + low));
            }
            return result.toString();
        }
        void emitState(String context, String role, State state) {
            out.row("state", context, role, state.ownerID(), state.seedID(), hex(state.seed48()),
                    hex(state.cached()), state.flag() ? "1" : "0", continuation(state));
        }
        void bindOriginal(ObjectReference value) {
            need(value.referenceType().name().equals(ORIGINAL) && appLoader != null
                    && appLoader.equals(value.referenceType().classLoader()), "actual-project-Original-owner");
            ObjectReference loader = read(value, "loader", "Ljava/net/URLClassLoader;");
            if (original == null) {
                original = value; gameLoader = loader;
                out.row("binding", "original", hold(value), "game_loader", hold(loader));
            }
            need(original.equals(value) && gameLoader.equals(loader) && !gameLoader.equals(appLoader),
                    "original-or-game-loader-identity-changed");
        }
        List<StackFrame> frames(ThreadReference thread) throws Exception {
            List<StackFrame> result = thread.frames();
            need(!result.isEmpty() && result.size() <= STACK_LIMIT,
                    "complete-source-stack-depth-bound:" + result.size() + ":limit=" + STACK_LIMIT);
            for (StackFrame frame : result) {
                if (frame.location().declaringType().name().equals(ORIGINAL) && frame.thisObject() != null) {
                    ObjectReference owner = frame.thisObject();
                    Value loader = owner.getValue(field(owner.referenceType(), "loader", "Ljava/net/URLClassLoader;", false));
                    // A JDK constructor may run before Original's loader field is published.
                    if (loader != null) bindOriginal(owner);
                }
            }
            return result;
        }
        String scope(ReferenceType type) {
            ObjectReference loader = type.classLoader();
            if (loader == null) return "jdk_bootstrap";
            if (helperName(type.name())) {
                need(appLoader != null && appLoader.equals(loader), "helper-loader-mismatch");
                return type.name().equals(TARGET) ? "target" : "helper";
            }
            if (gameLoader != null && gameLoader.equals(loader)) return "game";
            if (type.name().startsWith("jdk.") || type.name().startsWith("java.")
                    || type.name().startsWith("sun.") || type.name().startsWith("com.sun.")) return "jdk_nonbootstrap";
            throw new Rejected("unknown-source-stack-loader-" + type.name());
        }
        void stack(String context, ThreadReference thread) throws Exception {
            List<StackFrame> frames = frames(thread); String stack = "s" + (++stackSerial);
            out.row("stack", stack, context, number(frames.size()), hold(thread));
            boolean caller = false;
            for (int i = 0; i < frames.size(); ++i) {
                Location location = frames.get(i).location(); Method method = location.method();
                ReferenceType type = method.declaringType(); String category = scope(type);
                String loader = type.classLoader() == null ? "bootstrap" : hold(type.classLoader());
                out.row("frame", stack, number(i), category, type.name(), method.name(), method.signature(),
                        number(location.codeIndex()), loader, method.isNative() ? "native" : "java");
                if (!caller && !type.name().equals(RANDOM)
                        && !(type.name().equals(SYSTEM) && method.name().equals("nanoTime"))) {
                    out.row("caller", context, stack, number(i), category, type.name(), method.name(),
                            method.signature(), number(location.codeIndex()), loader); caller = true;
                }
            }
            need(caller, "constructor-or-draw-caller-missing");
        }
        void idle() {
            need(activeConstructor == null && draws.isEmpty(), "partial-constructor-or-draw-at-boundary");
            stableU();
        }
        void math(String marker) {
            List<ReferenceType> found = vm.classesByName(HOLDER);
            need(found.size() <= 1, "duplicate-Math-holder-class");
            if (found.isEmpty()) {
                need(mathHolder == null && mathOwner == null, "Math-holder-disappeared");
                out.row("math", marker, "absent", "0", "0", "0", "-", "-"); return;
            }
            ReferenceType type = found.get(0); bootstrap(type, HOLDER);
            if (mathHolder == null) mathHolder = type;
            need(mathHolder.equals(type), "Math-holder-class-identity-changed");
            boolean initialized = type.isInitialized();
            need(!type.failedToInitialize(), "Math-holder-failed-initialization");
            if (!initialized) {
                need(mathOwner == null, "Math-holder-lost-initialization");
                out.row("math", marker, "loaded", type.isPrepared() ? "1" : "0", "0", "0", "-", "-");
                return;
            }
            ObjectReference owner = reference(type.getValue(field(type, "randomNumberGenerator", "Ljava/util/Random;", true)));
            need(owner.referenceType().equals(random), "exact-ordinary-Math-Random");
            State observed = state(owner);
            need(constructed.containsKey(owner.uniqueID()), "Math-owner-default-construction-not-observed");
            if (mathOwner == null) { mathOwner = owner; mathSeed = observed.seed(); }
            need(mathOwner.equals(owner) && mathSeed.equals(observed.seed()), "Math-owner-or-seed-changed");
            out.row("math", marker, "loaded", "1", "1", "0", hold(mathOwner), hold(mathSeed));
            emitState(marker, "Math", observed);
        }
        void defaultBegin(BreakpointEvent event) throws Exception {
            mainThread(event.thread());
            need(!targetExit && activeConstructor == null && draws.isEmpty(), "overlapping-or-shutdown-default-constructor");
            need(random != null && event.location().method().declaringType().equals(random), "default-entry-owner");
            stableU();
            ObjectReference owner = reference(event.thread().frame(0).thisObject());
            need(randomInstance(owner.referenceType()) && !constructed.containsKey(owner.uniqueID()), "fresh-default-base-owner");
            need(++defaults <= CONSTRUCTOR_LIMIT, "default-constructor-count-bound");
            boolean ordinary = owner.referenceType().equals(random);
            if (ordinary) ordinaryDefaults++; else subclassDefaults++;
            activeConstructor = new ConstructorCall(defaults, owner, currentU, ordinary);
            out.row("constructor", number(defaults), "entry", hold(owner), hold(uniquifier), hex(currentU),
                    activeOperation >= 0 ? ACTIONS[activeOperation]
                        : nextOperation == 0 ? "target_bootstrap"
                        : nextOperation == ACTIONS.length ? "target_completion" : "between_operations",
                    number(activeOperation), ordinary ? "ordinary" : "subclass_base", owner.referenceType().name());
            stack("constructor_" + defaults + "_entry", event.thread());
            uniqueEntry.enable(); delegateEntry.enable(); constructorExits.enable();
        }
        void uniqueBegin(BreakpointEvent event) throws Exception {
            mainThread(event.thread());
            need(activeConstructor != null && activeConstructor.phase == 1, "uniquifier-entry-order");
            stableU(); activeConstructor.phase = 2;
            out.row("constructor", number(activeConstructor.index), "uniquifier_entry", hex(currentU));
        }
        void delegateBegin(BreakpointEvent event) throws Exception {
            mainThread(event.thread());
            need(activeConstructor != null && activeConstructor.phase == 4, "seeded-delegate-entry-order");
            List<Value> arguments = event.thread().frame(0).getArgumentValues();
            need(arguments.size() == 1 && activeConstructor.owner.equals(event.thread().frame(0).thisObject()),
                    "seeded-delegate-owner-and-arguments");
            activeConstructor.argument = longValue(arguments.get(0));
            need(activeConstructor.argument == (activeConstructor.committed ^ activeConstructor.sample), "delegate-actual-XOR-input");
            activeConstructor.phase = 5;
            out.row("constructor", number(activeConstructor.index), "delegate_entry", hex(activeConstructor.argument));
            stack("constructor_" + activeConstructor.index + "_delegate", event.thread());
        }
        void constructorExit(MethodExitEvent event) throws Exception {
            mainThread(event.thread());
            need(activeConstructor != null && event.method().declaringType().equals(random), "constructor-exit-owner-and-phase");
            ConstructorCall call = activeConstructor; Method method = event.method();
            if (method.name().equals("seedUniquifier") && method.signature().equals("()J")) {
                need(call.phase == 2, "uniquifier-return-order");
                call.committed = longValue(event.returnValue());
                need(call.committed == call.beforeU * U_MULTIPLIER && readU() == call.committed,
                        "actual-shared-uniquifier-CAS-progression");
                currentU = call.committed; call.phase = 3; uniqueReturns++;
                out.row("constructor", number(call.index), "uniquifier_return", hold(uniquifier), hex(call.committed));
                stack("constructor_" + call.index + "_uniquifier_return", event.thread()); clocks.enable();
            } else if (method.name().equals("initialScramble") && method.signature().equals("(J)J")) {
                need(call.phase == 5 && ++call.scrambleReturns == 1
                        && longValue(event.returnValue()) == ((call.argument ^ MULTIPLIER) & MASK),
                        "delegate-initial-scramble-return");
                validatedScrambleReturns++;
            } else if (method.name().equals("setSeed") && method.signature().equals("(J)V")) {
                need(call.phase == 5 && !call.ordinary && ++call.baseSetSeedReturns == 1 && call.scrambleReturns == 1
                        && event.returnValue() instanceof VoidValue
                        && call.owner.equals(event.thread().frame(0).thisObject()), "subclass-base-setSeed-return-order");
                List<Value> arguments = event.thread().frame(0).getArgumentValues();
                need(arguments.size() == 1 && longValue(arguments.get(0)) == call.argument, "subclass-base-setSeed-input");
                State seeded = state(call.owner);
                need(seeded.seed48() == ((call.argument ^ MULTIPLIER) & MASK) && !seeded.flag(), "subclass-base-setSeed-result");
                emitState("constructor_" + call.index + "_base_setSeed_return", "subclass_base", seeded);
                validatedBaseSetSeedReturns++;
            } else if (method.name().equals("<init>") && method.signature().equals("(J)V")) {
                need(call.phase == 5 && event.returnValue() instanceof VoidValue
                        && call.owner.equals(event.thread().frame(0).thisObject()), "delegate-return-order-and-owner");
                call.delegated = state(call.owner);
                if (call.ordinary) {
                    need(call.scrambleReturns == 1 && call.baseSetSeedReturns == 0
                            && call.delegated.seed48() == ((call.argument ^ MULTIPLIER) & MASK)
                            && call.delegated.cached() == 0 && !call.delegated.flag(), "fresh-ordinary-delegate-complete-state");
                } else need(call.scrambleReturns == call.baseSetSeedReturns,
                        "subclass-delegate-unaccounted-Random-base-methods");
                need(!call.delegated.seed().equals(uniquifier), "ordinary-seed-distinct-from-shared-uniquifier");
                for (State prior : constructed.values())
                    need(!prior.seed().equals(call.delegated.seed()), "distinct-default-AtomicLong-owner");
                emitState("constructor_" + call.index + "_delegate_return", call.ordinary ? "ordinary" : "subclass_base", call.delegated);
                call.phase = 6; delegateReturns++;
            } else if (method.name().equals("<init>") && method.signature().equals("()V")) {
                need(call.phase == 6 && event.returnValue() instanceof VoidValue
                        && call.owner.equals(event.thread().frame(0).thisObject()), "default-return-order-and-owner");
                State completed = state(call.owner); need(call.delegated.same(completed), "default-delegate-state-identity-match");
                stableU(); constructed.put(call.owner.uniqueID(), completed);
                emitState("constructor_" + call.index + "_default_return", call.ordinary ? "ordinary" : "subclass_base", completed);
                out.row("constructor", number(call.index), "return", hold(call.owner), hex(currentU));
                constructorExits.disable(); uniqueEntry.disable(); delegateEntry.disable(); clocks.disable();
                activeConstructor = null;
            } else throw new Rejected("unexpected-method-exit-during-default-" + method.name() + method.signature());
        }
        void clockExit(MethodExitEvent event) throws Exception {
            mainThread(event.thread()); Method method = event.method();
            need(activeConstructor != null && activeConstructor.phase == 3 && method.declaringType().equals(system)
                    && method.name().equals("nanoTime") && method.signature().equals("()J")
                    && method.isNative() && method.isStatic(), "actual-native-nanoTime-return-order");
            activeConstructor.sample = longValue(event.returnValue()); activeConstructor.phase = 4; clockReturns++;
            out.row("constructor", number(activeConstructor.index), "nano_time_return", hex(activeConstructor.sample));
            stack("constructor_" + activeConstructor.index + "_nano_time_return", event.thread()); clocks.disable();
        }

        int boxedInt(Value value) {
            ObjectReference owner = reference(value); bootstrap(owner.referenceType(), "java.lang.Integer");
            return intValue(owner.getValue(field(owner.referenceType(), "value", "I", false)));
        }
        long boxedLong(Value value) {
            ObjectReference owner = reference(value); bootstrap(owner.referenceType(), "java.lang.Long");
            return longValue(owner.getValue(field(owner.referenceType(), "value", "J", false)));
        }
        boolean boxedBoolean(Value value) {
            ObjectReference owner = reference(value); bootstrap(owner.referenceType(), "java.lang.Boolean");
            return booleanValue(owner.getValue(field(owner.referenceType(), "value", "Z", false)));
        }
        void worldBinding(ObjectReference observedExperiment) {
            need(observedExperiment.referenceType().equals(experimentType), "actual-Experiment-owner");
            if (experiment == null) { experiment = observedExperiment; hold(experiment); }
            need(experiment.equals(observedExperiment), "Experiment-owner-changed");
            bindOriginal(read(experiment, "source", "L" + ORIGINAL + ";"));
            ObjectReference actualWorld = read(original, "world", "Ljava/lang/Object;");
            ObjectReference actualCache = read(original, "cache", "Ljava/lang/Object;");
            need(actualWorld.referenceType().name().equals("dp") && actualCache.referenceType().name().equals("he")
                    && gameLoader.equals(actualWorld.referenceType().classLoader())
                    && gameLoader.equals(actualCache.referenceType().classLoader()), "genuine-dedicated-World-cache-types");
            if (world == null) { world = actualWorld; cache = actualCache; }
            need(world.equals(actualWorld) && cache.equals(actualCache), "actual-World-cache-identity-changed");
            need(actualWorld.referenceType() instanceof ClassType, "World-class-type");
            ClassType base = ((ClassType) actualWorld.referenceType()).superclass();
            need(base != null && base.name().equals("dj") && gameLoader.equals(base.classLoader()), "actual-dp-super-dj");
            ObjectReference actualRandom = reference(world.getValue(field(base, "r", "Ljava/util/Random;", false)));
            need(actualRandom.referenceType().equals(random), "exact-ordinary-World-r");
            need(cache.equals(world.getValue(field(base, "v", "Lbl;", false)))
                    && cache.equals(world.getValue(field(world.referenceType(), "C", "Lhe;", false))), "C-is-actual-base-cache");
            need(world.equals(actualCache.getValue(field(actualCache.referenceType(), "h", "Ldp;", false))), "cache-actual-World-binding");
            State observed = state(actualRandom);
            need(constructed.containsKey(actualRandom.uniqueID()), "World-r-default-constructor-not-observed");
            if (worldRandom == null) {
                worldRandom = actualRandom; worldSeed = observed.seed();
                out.row("binding", "world", hold(world), "cache", hold(cache), "World_r", hold(worldRandom), hold(worldSeed));
                // VM method-entry instance filters also deliver static helper
                // events. Bind actual nonstatic entry PCs instead, including
                // unsupported instance methods so they still fail closed.
                for (Method candidate : random.methods()) {
                    if (!candidate.isStatic() && !candidate.isAbstract() && !candidate.isNative()) {
                        BreakpointRequest point = entry(candidate, "World-r-draw-entry", false);
                        point.addInstanceFilter(worldRandom); point.addThreadFilter(main); drawEntries.add(point);
                    }
                }
                need(!drawEntries.isEmpty() && drawEntries.size() <= 64, "bounded-Random-instance-entry-points");
                drawExits = requests.createMethodExitRequest(); drawExits.addClassFilter(random);
                drawExits.addInstanceFilter(worldRandom); drawExits.addThreadFilter(main);
                drawExits.setSuspendPolicy(EventRequest.SUSPEND_ALL); drawExits.putProperty("role", "World-r-draw-exit");
            }
            need(worldRandom.equals(actualRandom) && worldSeed.equals(observed.seed()), "World-r-owner-or-AtomicLong-changed");
        }
        void operationBefore(BreakpointEvent event) throws Exception {
            mainThread(event.thread()); idle();
            need(!targetExit && activeOperation == -1 && nextOperation < ACTIONS.length, "operation-entry-order");
            StackFrame frame = event.thread().frame(0); List<Value> args = frame.getArgumentValues();
            need(args.size() == 4 && args.get(0) instanceof StringReference && args.get(3) instanceof ArrayReference,
                    "actual-operation-arguments");
            String action = ((StringReference) args.get(0)).value();
            need(ACTIONS[nextOperation].equals(action), "operation-action-schedule");
            worldBinding(reference(frame.thisObject()));
            ArrayReference packed = (ArrayReference) args.get(3); List<Value> values = packed.getValues();
            need(packed.referenceType().signature().equals("[Ljava/lang/Object;"), "operation-varargs-object-array");
            ObjectReference actualMethod = reference(args.get(2)); ObjectReference expectedMethod;
            if (COORDINATES[nextOperation] != null) {
                int[] point = COORDINATES[nextOperation];
                need(values.size() == 2 && boxedInt(values.get(0)) == point[0] && boxedInt(values.get(1)) == point[1]
                        && cache.equals(args.get(1)), "operation-coordinate-and-cache-schedule");
                expectedMethod = action.contains("drop") ? read(experiment, "drop", "Ljava/lang/reflect/Method;")
                        : read(original, "acquire", "Ljava/lang/reflect/Method;");
            } else if (nextOperation == 5) {
                need(values.size() == 1 && boxedLong(values.get(0)) == 1234 && world.equals(args.get(1)), "actual-time-operation-input");
                expectedMethod = read(original, "timeSetter", "Ljava/lang/reflect/Method;");
            } else if (nextOperation == 6 || nextOperation == 7) {
                need(values.size() == 2 && boxedBoolean(values.get(0)) && values.get(1) == null
                        && world.equals(args.get(1)), "actual-full-save-operation-input");
                expectedMethod = read(original, "worldSave", "Ljava/lang/reflect/Method;");
            } else {
                need(nextOperation == 14 && values.isEmpty() && cache.equals(args.get(1)), "actual-maintenance-input");
                expectedMethod = read(experiment, "maintain", "Ljava/lang/reflect/Method;");
            }
            need(expectedMethod.equals(actualMethod), "actual-held-source-Method-object");
            activeOperation = nextOperation;
            beforeOperation = state(worldRandom);
            if (lastBoundary != null) need(lastBoundary.same(beforeOperation), "World-r-change-between-measured-operations");
            out.row("boundary", number(activeOperation), action, "before", hold(world), hold(cache), hex(currentU));
            emitState("operation_" + activeOperation + "_before", "World_r", beforeOperation);
            math("operation_" + activeOperation + "_before");
            if (nextOperation == 12) {
                need(!far && ++farScopes == 1, "unique-far-17-1-scope");
                far = true; expectedFarSeed = beforeOperation.seed48(); lastDraw = beforeOperation;
                for (BreakpointRequest point : drawEntries) point.enable();
                out.row("draw_scope", "begin", number(activeOperation), hold(worldRandom), hold(worldSeed), "actual-instance-filter");
            }
        }
        void operationAfter(BreakpointEvent event) throws Exception {
            mainThread(event.thread()); idle();
            need(activeOperation == nextOperation && activeOperation >= 0
                    && experiment.equals(event.thread().frame(0).thisObject()), "operation-return-order-and-owner");
            worldBinding(experiment); State after = state(worldRandom);
            if (far) {
                need(activeOperation == 12 && after.seed48() == expectedFarSeed && lastDraw.same(after),
                        "complete-World-r-far-draw-trajectory");
                for (BreakpointRequest point : drawEntries) point.disable();
                drawExits.disable(); far = false;
                out.row("draw_scope", "end", number(activeOperation), number(drawEntriesCount), number(drawReturns));
            } else need(beforeOperation.same(after), "World-r-changed-outside-traced-far-operation");
            out.row("boundary", number(activeOperation), ACTIONS[activeOperation], "after", hold(world), hold(cache), hex(currentU));
            emitState("operation_" + activeOperation + "_after", "World_r", after);
            math("operation_" + activeOperation + "_after");
            lastBoundary = after; beforeOperation = null; activeOperation = -1; nextOperation++;
        }
        boolean supportedDraw(Method method) {
            return switch (method.name() + method.signature()) {
                case "next(I)I", "nextInt()I", "nextInt(I)I", "nextFloat()F", "nextDouble()D", "nextLong()J",
                     "nextBoolean()Z", "nextGaussian()D", "setSeed(J)V",
                     "writeObject(Ljava/io/ObjectOutputStream;)V" -> true;
                default -> false;
            };
        }
        String primitive(Value value) {
            if (value instanceof IntegerValue integer) return "int:" + integer.value();
            if (value instanceof LongValue integer) return "long:" + hex(integer.value());
            if (value instanceof BooleanValue bool) return "boolean:" + (bool.value() ? "1" : "0");
            if (value instanceof FloatValue real) return "float:" + String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(real.value()));
            if (value instanceof DoubleValue real) return "double:" + hex(Double.doubleToRawLongBits(real.value()));
            if (value instanceof VoidValue) return "void";
            throw new Rejected("unsupported-draw-value-type");
        }
        void drawEntry(BreakpointEvent event) throws Exception {
            mainThread(event.thread()); Method method = event.location().method();
            need(far && activeOperation == 12 && activeConstructor == null && !method.isStatic()
                    && method.declaringType().equals(random) && supportedDraw(method),
                    "unsupported-or-out-of-scope-World-r-method:" + method.declaringType().name()
                    + "." + method.name() + method.signature() + ":static=" + method.isStatic()
                    + ":constructor=" + (activeConstructor != null));
            StackFrame frame = event.thread().frame(0);
            need(worldRandom.equals(frame.thisObject()) && draws.size() < DRAW_DEPTH_LIMIT, "instance-filter-owner-or-draw-depth");
            State before = state(worldRandom); need(before.seed48() == expectedFarSeed, "unobserved-World-r-step-before-draw");
            if (!draws.isEmpty()) {
                need(!draws.peek().method.name().equals("writeObject"), "Random-entry-nested-in-passive-serialization");
                need(event.thread().frameCount() > 1 && event.thread().frame(1).location().method().equals(draws.peek().method),
                        "World-r-nested-method-order");
            } else need(lastDraw.same(before), "World-r-root-draw-boundary-state");
            List<Value> arguments = new ArrayList<>(frame.getArgumentValues());
            boolean serialization = method.name().equals("writeObject");
            if (serialization) {
                need(draws.isEmpty() && arguments.size() == 1, "observer-serialization-scope");
                bootstrap(reference(arguments.get(0)).referenceType(), "java.io.ObjectOutputStream");
                boolean snapshot = false;
                for (StackFrame caller : frames(event.thread())) {
                    Method called = caller.location().method();
                    if (called.declaringType().name().equals("NaturalPopulationObservation$RandomFields")
                            && appLoader.equals(called.declaringType().classLoader())
                            && called.name().equals("snapshot")
                            && called.signature().equals("(Ljava/util/Random;)Ljava/util/Map;")) snapshot = true;
                }
                need(snapshot, "serialization-without-pinned-passive-Random-snapshot");
            }
            Draw draw = new Draw(++drawEntriesCount, draws.size(), method, before, arguments);
            out.row("draw", number(draw.index), "entry", number(draw.depth), draws.isEmpty() ? "-" : number(draws.peek().index),
                    method.name(), method.signature(), hold(worldRandom));
            for (int i = 0; i < arguments.size(); ++i) out.row("draw_argument", number(draw.index), number(i),
                    serialization ? "reference:" + hold(reference(arguments.get(i))) : primitive(arguments.get(i)));
            emitState("draw_" + draw.index + "_entry", "World_r", before);
            stack("draw_" + draw.index + "_entry", event.thread());
            if (draws.isEmpty()) drawExits.enable();
            draws.push(draw);
        }
        void drawExit(MethodExitEvent event) throws Exception {
            mainThread(event.thread());
            need(far && activeConstructor == null && !draws.isEmpty() && worldRandom.equals(event.thread().frame(0).thisObject()),
                    "World-r-draw-return-scope-and-owner");
            Draw draw = draws.peek(); Method method = event.method(); need(draw.method.equals(method), "World-r-draw-return-nesting");
            State after = state(worldRandom); Value returned = event.returnValue();
            if (method.name().equals("next") && method.signature().equals("(I)I")) {
                need(draw.arguments.size() == 1, "next-argument-count"); int bits = intValue(draw.arguments.get(0));
                need(bits >= 1 && bits <= 32 && draw.before.seed48() == expectedFarSeed, "next-bit-range-and-entry-state");
                expectedFarSeed = (expectedFarSeed * MULTIPLIER + 11) & MASK;
                need(intValue(returned) == (int) (expectedFarSeed >>> (48 - bits))
                        && after.cached() == draw.before.cached() && after.flag() == draw.before.flag(), "actual-next-step-and-result");
            } else if (method.name().equals("setSeed") && method.signature().equals("(J)V")) {
                need(draw.arguments.size() == 1 && returned instanceof VoidValue, "setSeed-arguments-and-return");
                expectedFarSeed = (longValue(draw.arguments.get(0)) ^ MULTIPLIER) & MASK;
                need(after.cached() == draw.before.cached() && !after.flag(), "actual-setSeed-cache-effects");
            } else if (method.name().equals("writeObject")) {
                need(returned instanceof VoidValue && draw.before.same(after), "passive-serialization-changed-World-r");
            }
            need(after.seed48() == expectedFarSeed && after.seed().equals(worldSeed), "unobserved-World-r-step-or-owner-change");
            out.row("draw", number(draw.index), "return", number(draw.depth), method.name(), method.signature(), primitive(returned));
            emitState("draw_" + draw.index + "_return", "World_r", after);
            draws.pop(); drawReturns++; lastDraw = after;
            if (draws.isEmpty()) drawExits.disable();
        }
        void exitTarget(BreakpointEvent event) throws Exception {
            mainThread(event.thread()); idle();
            need(!targetExit && nextOperation == ACTIONS.length && activeOperation == -1 && !far
                    && event.thread().frameCount() >= 2, "target-System-exit-order");
            Method caller = event.thread().frame(1).location().method();
            need(caller.declaringType().equals(target) && caller.name().equals("main")
                    && caller.signature().equals("([Ljava/lang/String;)V"), "System-exit-actual-target-main-caller");
            List<Value> args = event.thread().frame(0).getArgumentValues();
            need(args.size() == 1 && intValue(args.get(0)) == 0, "target-must-exit-successfully");
            worldBinding(experiment); State finalState = state(worldRandom);
            need(lastBoundary.same(finalState), "World-r-changed-after-final-operation");
            emitState("target_exit", "World_r", finalState); math("target_exit");
            out.row("life", "target_System_exit", "0", hex(currentU)); targetExit = true;
        }
        void start(VMStartEvent event) {
            need(!startedVM, "duplicate-VM-start"); startedVM = true; main = event.thread();
            need(main != null, "missing-VM-main-thread"); out.row("life", "VM_start", hold(main));
            List<ReferenceType> types = vm.allClasses(); need(types.size() <= 16384, "startup-loaded-types-bound");
            out.row("meta", "startup_loaded_types", number(types.size()));
            for (String name : new String[]{RANDOM, SYSTEM, TARGET, EXPERIMENT}) {
                ClassPrepareRequest prepare = requests.createClassPrepareRequest(); prepare.addClassFilter(name);
                prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); prepare.enable();
            }
            // Bind helpers before resolving any stack scope. No target code is invoked.
            for (ReferenceType type : types) if (type.name().equals(TARGET) && type.isPrepared()) bindTarget(type);
            for (ReferenceType type : types)
                if (!type.name().equals(TARGET) && type.isPrepared()
                        && List.of(RANDOM, SYSTEM, EXPERIMENT).contains(type.name())) bind(type);
            if (random != null && random.isInitialized()) stableU();
            math("VM_start");
        }
        void breakpoint(BreakpointEvent event) throws Exception {
            breakpointEvents++; Object role = event.request().getProperty("role");
            need(role instanceof String, "breakpoint-role");
            switch ((String) role) {
                case "default-entry" -> defaultBegin(event);
                case "uniquifier-entry" -> uniqueBegin(event);
                case "delegate-entry" -> delegateBegin(event);
                case "operation-before" -> operationBefore(event);
                case "operation-after" -> operationAfter(event);
                case "target-exit" -> exitTarget(event);
                case "World-r-draw-entry" -> drawEntry(event);
                default -> throw new Rejected("unknown-breakpoint-role");
            }
        }
        void complete() {
            need(startedVM && targetExit && nextOperation == ACTIONS.length && activeOperation == -1
                    && activeConstructor == null && draws.isEmpty() && !far && farScopes == 1, "incomplete-target-trajectory");
            need(uWitnessed && defaults > 0 && defaults == uniqueReturns && defaults == clockReturns
                    && defaults == delegateReturns && defaults == ordinaryDefaults + subclassDefaults
                    && validatedScrambleReturns == ordinaryDefaults + validatedBaseSetSeedReturns
                    && constructed.size() == defaults, "incomplete-default-constructor-inputs");
            need(drawEntriesCount == drawReturns && worldRandom != null && worldSeed != null, "incomplete-World-r-draws");
        }
        void run() throws Exception {
            out.row("meta", "protocol", "dedicated-random-observation-2");
            out.row("meta", "scope", "passive-original-only-instrumented-constructor-inputs-and-World-r-far-draws");
            out.row("meta", "tested_commit", "null"); out.row("meta", "native_comparison", "false");
            out.row("meta", "source_context", "outer-capture-report");
            out.row("meta", "mechanical_capture_only", "true");
            out.text("vm_name", vm.name()); out.text("vm_version", vm.version()); out.text("vm_description", vm.description());
            need(vm.canGetMethodReturnValues() && vm.canGetBytecodes() && vm.canUseInstanceFilters(), "required-JDI-capabilities");
            out.row("meta", "can_get_method_return_values", "1"); out.row("meta", "can_get_bytecodes", "1");
            out.row("meta", "can_use_instance_filters", "1");
            out.row("meta", "mirror_policy", "strong_and_disable_collection_until_VM_death");
            out.row("meta", "clock_policy", "observe_actual_native_returns_debugger_pauses_change_samples");
            out.row("meta", "bounds", number(EVENT_LIMIT), number(CONSTRUCTOR_LIMIT), number(MIRROR_LIMIT),
                    number(STACK_LIMIT), number(OUTPUT_LIMIT), "180_seconds");
            EventQueue queue = vm.eventQueue();
            while (!disconnected) {
                EventSet set = queue.remove(Math.min(1000, remainingMillis())); if (set == null) continue;
                for (Event event : set) {
                    need(++delivered <= EVENT_LIMIT, "delivered-event-bound"); remainingMillis();
                    if (event instanceof VMStartEvent start) {
                        need(set.suspendPolicy() == EventRequest.SUSPEND_ALL, "VM-start-suspend-all"); start(start);
                    } else if (event instanceof VMDeathEvent) {
                        need(!died, "duplicate-VM-death"); complete(); died = true; out.row("life", "VM_death");
                    } else if (event instanceof VMDisconnectEvent) {
                        need(died && !disconnected, "disconnect-before-complete-VM-death"); disconnected = true;
                        out.row("life", "VM_disconnect");
                    } else {
                        need(startedVM && !died && set.suspendPolicy() == EventRequest.SUSPEND_ALL, "passive-event-lifecycle-and-suspension");
                        if (event instanceof ClassPrepareEvent prepare) { prepareEvents++; mainThread(prepare.thread()); bind(prepare.referenceType()); }
                        else if (event instanceof BreakpointEvent point) breakpoint(point);
                        else if (event instanceof MethodExitEvent exit) {
                            Object role = exit.request().getProperty("role");
                            if ("constructor-exit".equals(role)) constructorExit(exit);
                            else if ("clock-exit".equals(role)) clockExit(exit);
                            else if ("World-r-draw-exit".equals(role)) drawExit(exit);
                            else throw new Rejected("method-exit-request-role");
                        } else throw new Rejected("unexpected-JDI-event-kind");
                    }
                }
                if (!disconnected) {
                    try { set.resume(); }
                    catch (VMDisconnectedException failure) { need(died, "resume-disconnected-before-VM-death"); }
                }
            }
            complete();
            out.row("count", "events", number(delivered), "breakpoints", number(breakpointEvents), "class_prepares", number(prepareEvents));
            out.row("count", "default_constructors", number(defaults), "uniquifier_returns", number(uniqueReturns),
                    "nano_time_returns", number(clockReturns), "delegate_returns", number(delegateReturns));
            out.row("count", "ordinary_defaults", number(ordinaryDefaults), "subclass_base_defaults", number(subclassDefaults),
                    "validated_subclass_base_setSeed_returns", number(validatedBaseSetSeedReturns));
            out.row("count", "World_r_method_entries", number(drawEntriesCount), "World_r_method_returns", number(drawReturns),
                    "validated_constructor_scramble_returns", number(validatedScrambleReturns));
            out.row("count", "operations", number(nextOperation), "far_scopes", number(farScopes), "held_mirrors", number(held.size()));
            out.row("result", "complete");
        }
    }

    static int port(String text) {
        need(text.length() >= 1 && text.length() <= 5, "port-syntax"); int result = 0;
        for (int i = 0; i < text.length(); ++i) {
            char c = text.charAt(i); need(c >= '0' && c <= '9', "port-syntax"); result = result * 10 + c - '0';
        }
        need(result >= 1 && result <= 65535, "port-range"); return result;
    }
    public static void main(String[] arguments) {
        try {
            need(arguments.length == 1, "one-owned-loopback-port-required"); int port = port(arguments[0]);
            long started = System.nanoTime(); AttachingConnector connector = null;
            for (AttachingConnector candidate : Bootstrap.virtualMachineManager().attachingConnectors()) {
                if (candidate.name().equals("com.sun.jdi.SocketAttach")) {
                    need(connector == null, "duplicate-socket-connector"); connector = candidate;
                }
            }
            need(connector != null, "socket-connector-unavailable");
            Map<String,Connector.Argument> options = connector.defaultArguments();
            need(options.containsKey("hostname") && options.containsKey("port") && options.containsKey("timeout"), "socket-connector-arguments");
            options.get("hostname").setValue("127.0.0.1"); options.get("port").setValue(Integer.toString(port));
            options.get("timeout").setValue("10000");
            VirtualMachine vm = connector.attach(options); new Session(vm, new Trace(), started).run();
        } catch (Rejected failure) {
            System.err.println("population-dedicated-random-observer: " + failure.getMessage()); System.exit(1);
        } catch (Throwable failure) {
            System.err.println("population-dedicated-random-observer: observer-or-JDI-failure " + failure.getClass().getName());
            failure.printStackTrace(System.err); System.exit(1);
        }
    }
}
