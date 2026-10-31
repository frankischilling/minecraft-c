import com.sun.jdi.ArrayReference;
import com.sun.jdi.BooleanValue;
import com.sun.jdi.Bootstrap;
import com.sun.jdi.DoubleValue;
import com.sun.jdi.Field;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.LongValue;
import com.sun.jdi.Method;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.StackFrame;
import com.sun.jdi.ThreadReference;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.VoidValue;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventQueue;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.MethodEntryEvent;
import com.sun.jdi.event.MethodExitEvent;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.event.VMStartEvent;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import com.sun.jdi.request.EventRequestManager;
import com.sun.jdi.request.MethodEntryRequest;
import com.sun.jdi.request.MethodExitRequest;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Read-only JDI observation. The outer recorder owns both JVM processes. */
public final class RandomConstructorObserver {
    static final String TARGET = "RandomConstructorTarget";
    static final String RANDOM = "java.util.Random";
    static final String MATH = "java.lang.Math";
    static final String HOLDER = "java.lang.Math$RandomNumberGeneratorHolder";
    static final String[] STAGES = {"bootstrap", "default_a", "explicit_e", "gaussian_e",
        "reseed_e", "math_first", "math_later", "default_b", "math_after_b", "done"};
    static final String[] ROLES = {"A", "E", "M", "B"};
    static final long TIME_LIMIT = TimeUnit.SECONDS.toNanos(90);
    static final int EVENT_LIMIT = 4096, MIRROR_LIMIT = 64, OUTPUT_LIMIT = 1024 * 1024;

    static final class Rejected extends RuntimeException {
        private static final long serialVersionUID = 1L;
        Rejected(String reason) { super(reason); }
    }

    static void need(boolean value, String reason) {
        if (!value) throw new Rejected(reason);
    }

    static String hex(long value) { return String.format(Locale.ROOT, "%016x", value); }

    static final class Trace {
        int bytes;
        void row(String... fields) {
            StringBuilder line = new StringBuilder();
            for (String field : fields) {
                need(field != null && field.length() <= 4096, "trace-field-bound");
                for (int i = 0; i < field.length(); i++) {
                    char c = field.charAt(i);
                    need(c >= 32 && c <= 126, "trace-field-ascii");
                }
                if (line.length() != 0) line.append('\t');
                line.append(field);
            }
            need(line.length() <= 8192, "trace-line-bound");
            line.append('\n');
            byte[] encoded = line.toString().getBytes(StandardCharsets.US_ASCII);
            need(encoded.length <= OUTPUT_LIMIT - bytes, "trace-total-bound");
            bytes += encoded.length;
            System.out.write(encoded, 0, encoded.length);
            need(!System.out.checkError(), "trace-output-error");
        }
        void text(String key, String value) {
            byte[] encoded = value.getBytes(StandardCharsets.UTF_8);
            need(encoded.length <= 2048, "metadata-text-bound");
            StringBuilder bits = new StringBuilder(encoded.length * 2);
            for (byte b : encoded) bits.append(String.format(Locale.ROOT, "%02x", b & 255));
            row("meta", key + "_utf8hex", bits.toString());
        }
    }

    static final class Held {
        final ObjectReference mirror;
        final String label;
        Held(ObjectReference mirror, String label) { this.mirror = mirror; this.label = label; }
    }

    record State(ObjectReference owner, ObjectReference seed, String ownerID, String seedID,
                 long state, long cached, boolean flag) {
        boolean same(State other) {
            return other != null && owner.equals(other.owner) && seed.equals(other.seed)
                && state == other.state && cached == other.cached && flag == other.flag;
        }
    }

    static final class Session {
        final VirtualMachine vm;
        final EventRequestManager requests;
        final Trace out;
        final long started;
        final Map<Long,Held> held = new LinkedHashMap<>();
        final int[] defaults = new int[STAGES.length], draws = new int[STAGES.length];
        final Map<Integer,State> constructed = new LinkedHashMap<>();
        final Map<String,State> beforeOwners = new LinkedHashMap<>();
        ReferenceType target, random, math, holder;
        ThreadReference main;
        ObjectReference uniquifier, mathOwner;
        MethodExitRequest clocks;
        Field randomSeed, randomCache, randomFlag, randomU, atomicValue;
        Field targetA, targetE, targetB, targetGaussian, targetMath, holderOwner;
        State drawEntry;
        State pendingDefault, explicitState;
        ObjectReference drawOwner;
        int active = -1, nextStage, markerStage = -1;
        String marker;
        long currentU, beforeU, committedU, clock, drawResult, mathResult, gaussianResult;
        int defaultPhase, nextPart, nextBits;
        boolean uReady, seededOpen, gaussianOpen, reseedOpen, mathOpen, drawOpen, nextOpen;
        boolean holderOpen, holderDone, startedVM, died, disconnected;
        int delivered, entries, exits, ignored, prepares, randomPrepares, mathPrepares, targetPrepares;
        int holderPrepares, holderEntries, holderExits, beforeCount, afterCount, markerExits;
        int mainEntries, mainExits, defaultsTotal, uniqueReturns, clockReturns, delegateReturns;
        int explicitEntries, explicitExits, gaussianEntries, gaussianExits, reseedEntries, reseedExits;
        int mathEntries, mathExits, drawReturns, nextReturns;

        Session(VirtualMachine vm, Trace out, long started) {
            this.vm = vm; this.requests = vm.eventRequestManager(); this.out = out; this.started = started;
        }

        long remainingMillis() {
            long left = TIME_LIMIT - (System.nanoTime() - started);
            need(left > 0, "event-deadline");
            return Math.max(1, TimeUnit.NANOSECONDS.toMillis(left));
        }

        String stage() { return active < 0 ? "startup" : STAGES[active]; }
        boolean mathStage() { return active == 5 || active == 6 || active == 8; }
        boolean defaultStage() { return active == 1 || active == 5 || active == 7; }
        void event(String name, String... data) {
            String[] fields = new String[data.length + 3];
            fields[0] = "event"; fields[1] = stage(); fields[2] = name;
            System.arraycopy(data, 0, fields, 3, data.length); out.row(fields);
        }
        void mainThread(ThreadReference thread) {
            need(main != null && main.equals(thread), "unexpected-event-thread");
        }
        String hold(ObjectReference value) {
            long id = value.uniqueID();
            Held known = held.get(id);
            if (known != null) { need(known.mirror.equals(value), "mirror-identity-mismatch"); return known.label; }
            need(held.size() < MIRROR_LIMIT, "mirror-bound");
            // Keep both the actual mirror and a target collection pin until VM death.
            value.disableCollection();
            String label = "o" + (held.size() + 1);
            held.put(id, new Held(value, label));
            return label;
        }
        Field field(ReferenceType type, String name, String signature, boolean isStatic) {
            Field found = null;
            for (Field value : type.fields()) {
                if (value.name().equals(name) && value.signature().equals(signature)
                        && value.declaringType().equals(type) && value.isStatic() == isStatic) {
                    need(found == null, "duplicate-field-binding"); found = value;
                }
            }
            need(found != null, "missing-field-binding");
            out.row("field", type.name(), name, signature, isStatic ? "static" : "instance");
            return found;
        }
        void method(ReferenceType type, String name, String signature, boolean isStatic) {
            Method found = null;
            for (Method value : type.methodsByName(name, signature)) {
                if (value.declaringType().equals(type) && value.isStatic() == isStatic) {
                    need(found == null, "duplicate-method-binding"); found = value;
                }
            }
            need(found != null, "missing-method-binding");
            out.row("method", type.name(), name, signature, isStatic ? "static" : "instance",
                found.isNative() ? "native" : "java");
        }
        void bootstrap(ReferenceType type, String name) {
            need(type.name().equals(name) && type.classLoader() == null, "bootstrap-type-binding");
        }
        void bindRandom(ReferenceType type) {
            bootstrap(type, RANDOM);
            if (random != null) { need(random.equals(type), "random-type-changed"); return; }
            random = type;
            randomSeed = field(type, "seed", "Ljava/util/concurrent/atomic/AtomicLong;", false);
            randomCache = field(type, "nextNextGaussian", "D", false);
            randomFlag = field(type, "haveNextNextGaussian", "Z", false);
            randomU = field(type, "seedUniquifier", "Ljava/util/concurrent/atomic/AtomicLong;", true);
            method(type, "<init>", "()V", false); method(type, "<init>", "(J)V", false);
            method(type, "seedUniquifier", "()J", true); method(type, "setSeed", "(J)V", false);
            method(type, "nextDouble", "()D", false); method(type, "next", "(I)I", false);
            method(type, "nextGaussian", "()D", false);
        }
        void bindMath(ReferenceType type) {
            bootstrap(type, MATH);
            if (math != null) { need(math.equals(type), "math-type-changed"); return; }
            math = type; method(type, "random", "()D", true);
        }
        void bindTarget(ReferenceType type) {
            need(type.name().equals(TARGET) && type.classLoader() != null, "target-type-binding");
            if (target != null) { need(target.equals(type), "target-type-changed"); return; }
            target = type;
            targetA = field(type, "A", "Ljava/util/Random;", true);
            targetE = field(type, "E", "Ljava/util/Random;", true);
            targetB = field(type, "B", "Ljava/util/Random;", true);
            targetGaussian = field(type, "lastGaussian", "D", true);
            targetMath = field(type, "lastMath", "D", true);
            method(type, "before", "(I)V", true); method(type, "after", "(I)V", true);
            method(type, "main", "([Ljava/lang/String;)V", true);
        }
        ObjectReference reference(Value value) {
            need(value instanceof ObjectReference, "nonnull-object-field"); return (ObjectReference) value;
        }
        long longValue(Value value) {
            need(value instanceof LongValue, "long-value-type"); return ((LongValue) value).value();
        }
        long doubleBits(Value value) {
            need(value instanceof DoubleValue, "double-value-type");
            return Double.doubleToRawLongBits(((DoubleValue) value).value());
        }
        long readU() {
            need(random != null && random.isInitialized() && !random.failedToInitialize(), "random-not-initialized");
            ObjectReference value = reference(random.getValue(randomU));
            bootstrap(value.referenceType(), "java.util.concurrent.atomic.AtomicLong");
            if (uniquifier == null) {
                uniquifier = value; hold(value);
                atomicValue = field(value.referenceType(), "value", "J", false);
            }
            need(value.equals(uniquifier), "uniquifier-owner-changed");
            return longValue(value.getValue(atomicValue));
        }
        void stableU() { need(uReady && readU() == currentU, "unexpected-uniquifier-change"); }
        State state(ObjectReference value) {
            need(random != null && value.referenceType().equals(random), "ordinary-random-required");
            ObjectReference seed = reference(value.getValue(randomSeed));
            need(atomicValue != null && seed.referenceType().equals(uniquifier.referenceType()), "seed-atomic-type");
            String ownerID = hold(value), seedID = hold(seed);
            long seedValue = longValue(seed.getValue(atomicValue));
            need((seedValue & ~0xffffffffffffL) == 0, "seed-width");
            Value flag = value.getValue(randomFlag);
            need(flag instanceof BooleanValue, "gaussian-flag-type");
            return new State(value, seed, ownerID, seedID, seedValue,
                doubleBits(value.getValue(randomCache)), ((BooleanValue) flag).value());
        }
        ObjectReference owner(String role) {
            if (role.equals("M")) {
                if (mathOwner == null) return null;
                need(holder != null && holder.isInitialized() && !holder.failedToInitialize(), "holder-not-ready");
                ObjectReference value = reference(holder.getValue(holderOwner));
                need(value.equals(mathOwner), "math-holder-owner-changed"); return value;
            }
            Field slot = role.equals("A") ? targetA : role.equals("E") ? targetE : targetB;
            Value value = target.getValue(slot);
            return value == null ? null : reference(value);
        }
        void emitState(String boundary, String role, State value) {
            if (value == null) out.row("state", stage(), boundary, role, "-", "-", "-", "-", "-");
            else out.row("state", stage(), boundary, role, value.ownerID, value.seedID,
                hex(value.state), hex(value.cached), value.flag ? "1" : "0");
        }
        Map<String,State> snapshot(String boundary, int completed) {
            Map<String,State> values = new LinkedHashMap<>();
            for (String role : ROLES) {
                ObjectReference value = owner(role);
                boolean expected = completed >= (role.equals("A") ? 1 : role.equals("E") ? 2 : role.equals("M") ? 5 : 7);
                need((value != null) == expected, "owner-publication-order");
                State entry = value == null ? null : state(value);
                values.put(role, entry); emitState(boundary, role, entry);
            }
            if (uReady) { stableU(); out.row("u", stage(), boundary, hold(uniquifier), hex(currentU)); }
            else out.row("u", stage(), boundary, "-", "-");
            return values;
        }
        void fresh(State value) { need(value != null && value.cached == 0 && !value.flag, "fresh-cache-state"); }
        void unchanged(State left, State right) {
            need(left == null ? right == null : left.same(right), "unaffected-owner-changed");
        }
        void afterStage(Map<String,State> values) {
            String changed = active == 1 ? "A" : active >= 2 && active <= 4 ? "E"
                : mathStage() ? "M" : active == 7 ? "B" : "";
            for (String role : ROLES) if (!role.equals(changed)) unchanged(beforeOwners.get(role), values.get(role));
            for (String role : ROLES) {
                State old = beforeOwners.get(role), now = values.get(role);
                if (old != null) need(now != null && old.owner.equals(now.owner) && old.seed.equals(now.seed),
                    "published-owner-identity-changed");
            }
            if (active == 1 || active == 7) {
                need(defaults[active] == 1 && defaultPhase == 0, "external-default-receipts");
                State completed = constructed.get(active);
                fresh(values.get(changed)); need(completed != null && completed.same(values.get(changed)), "default-owner-publication");
            } else if (active == 2) {
                need(explicitEntries == 1 && explicitExits == 1 && !seededOpen, "explicit-constructor-receipts");
                fresh(values.get("E")); need(explicitState.same(values.get("E")), "explicit-owner-publication");
            } else if (active == 3) {
                need(gaussianEntries == 1 && gaussianExits == 1 && !gaussianOpen
                    && draws[active] >= 2 && draws[active] % 2 == 0, "gaussian-receipts");
                need(values.get("E").flag && doubleBits(target.getValue(targetGaussian)) == gaussianResult,
                    "gaussian-publication");
            } else if (active == 4) {
                State old = beforeOwners.get("E"), now = values.get("E");
                need(reseedEntries == 1 && reseedExits == 1 && !reseedOpen && old.flag && !now.flag
                    && old.cached == now.cached && old.owner.equals(now.owner) && old.seed.equals(now.seed),
                    "reseed-prefix");
            } else if (mathStage()) {
                need(draws[active] == 1 && !mathOpen && !drawOpen
                    && doubleBits(target.getValue(targetMath)) == mathResult, "math-publication");
                if (active == 5) need(defaults[active] == 1 && holderDone, "first-math-default-receipts");
            }
            if (active != 0 && !defaultStage()) need(currentU == beforeU, "nondefault-uniquifier-change");
            State a = values.get("A"), e = values.get("E"), b = values.get("B"), m = values.get("M");
            State[] all = {a, e, b, m};
            for (int i = 0; i < all.length; i++) for (int j = i + 1; j < all.length; j++) {
                if (all[i] != null && all[j] != null) need(!all[i].owner.equals(all[j].owner)
                    && !all[i].seed.equals(all[j].seed), "distinct-owner-identity");
            }
        }
        List<Value> arguments(ThreadReference thread) throws Exception { return thread.frame(0).getArgumentValues(); }
        ObjectReference receiver(ThreadReference thread) throws Exception {
            return reference(thread.frame(0).thisObject());
        }
        long caller(ThreadReference thread, ReferenceType type, String name, String signature) throws Exception {
            for (StackFrame frame : thread.frames()) {
                Method value = frame.location().method();
                if (value.declaringType().equals(type) && value.name().equals(name) && value.signature().equals(signature))
                    return frame.location().codeIndex();
            }
            throw new Rejected("required-caller-not-visible");
        }
        void markerEntry(MethodEntryEvent event) throws Exception {
            mainThread(event.thread());
            need(marker == null && defaultPhase == 0 && !drawOpen && !mathOpen && !gaussianOpen
                && !reseedOpen && !seededOpen, "marker-operation-overlap");
            List<Value> args = arguments(event.thread());
            need(args.size() == 1 && args.get(0) instanceof IntegerValue, "marker-argument");
            int index = ((IntegerValue) args.get(0)).value();
            need(index >= 0 && index < STAGES.length, "marker-stage-range");
            String name = event.method().name();
            if (name.equals("before")) {
                need(active == -1 && index == nextStage, "before-marker-order");
                active = index; beforeCount++;
                out.row("marker", stage(), "before");
                beforeOwners.clear(); beforeOwners.putAll(snapshot("before", index - 1));
                if (uReady) beforeU = currentU;
            } else {
                need(name.equals("after") && active == index && index == nextStage, "after-marker-order");
                if (index == 0) { currentU = readU(); uReady = true; }
                Map<String,State> values = snapshot("after", index);
                afterStage(values); afterCount++;
                out.row("marker", stage(), "after");
                active = -1; nextStage++;
            }
            marker = name; markerStage = index;
        }
        void markerExit(MethodExitEvent event) {
            mainThread(event.thread());
            need(marker != null && marker.equals(event.method().name()) && event.returnValue() instanceof VoidValue,
                "marker-exit-order");
            out.row("marker", STAGES[markerStage], marker + "_return");
            marker = null; markerStage = -1; markerExits++;
        }
        void install() {
            for (String name : new String[]{TARGET, RANDOM, MATH, HOLDER}) {
                ClassPrepareRequest prepare = requests.createClassPrepareRequest(); prepare.addClassFilter(name);
                prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); prepare.enable();
                MethodEntryRequest enter = requests.createMethodEntryRequest(); enter.addClassFilter(name);
                enter.setSuspendPolicy(EventRequest.SUSPEND_ALL); enter.enable();
                MethodExitRequest exit = requests.createMethodExitRequest(); exit.addClassFilter(name);
                exit.setSuspendPolicy(EventRequest.SUSPEND_ALL); exit.enable();
            }
            clocks = requests.createMethodExitRequest(); clocks.addClassFilter("java.lang.System");
            clocks.addThreadFilter(main); clocks.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        }
        void start(VMStartEvent event) {
            need(!startedVM && active == -1, "duplicate-vm-start"); startedVM = true; main = event.thread();
            need(main != null, "missing-vm-start-thread");
            out.row("meta", "main_thread", hold(main));
            List<ReferenceType> types = vm.allClasses();
            need(types.size() <= 8192, "startup-class-inventory-bound");
            for (ReferenceType type : types) need(!type.name().equals(HOLDER), "holder-not-cold-at-vm-start");
            out.row("meta", "vm_start_loaded_types", Integer.toString(types.size()));
            out.row("meta", "holder_absent_at_vm_start", "1");
            for (ReferenceType type : types) {
                if (type.name().equals(RANDOM)) bindRandom(type);
                else if (type.name().equals(MATH)) bindMath(type);
                else if (type.name().equals(TARGET)) bindTarget(type);
            }
            install();
        }
        void prepare(ClassPrepareEvent event) {
            need(startedVM && !died, "prepare-lifecycle"); prepares++;
            ReferenceType type = event.referenceType();
            if (type.name().equals(HOLDER)) {
                mainThread(event.thread()); bootstrap(type, HOLDER);
                need(active == 5 && mathOpen && ++holderPrepares == 1 && holder == null
                    && type.isPrepared() && !type.isInitialized() && !type.failedToInitialize(), "holder-prepare-order");
                holder = type; holderOwner = field(type, "randomNumberGenerator", "Ljava/util/Random;", true);
                method(type, "<clinit>", "()V", true); event("holder_prepare");
            } else if (type.name().equals(RANDOM)) {
                need(++randomPrepares == 1, "duplicate-random-prepare"); bindRandom(type);
            } else if (type.name().equals(MATH)) {
                need(++mathPrepares == 1, "duplicate-math-prepare"); bindMath(type);
            } else if (type.name().equals(TARGET)) {
                need(++targetPrepares == 1, "duplicate-target-prepare"); bindTarget(type);
            } else throw new Rejected("unexpected-class-prepare");
        }
        void defaultEnter(MethodEntryEvent event) throws Exception {
            need(defaultStage() && marker == null && defaultPhase == 0 && defaults[active] == 0,
                "unexpected-default-constructor");
            stableU();
            caller(event.thread(), active == 5 ? holder : target, active == 5 ? "<clinit>" : "main",
                active == 5 ? "()V" : "([Ljava/lang/String;)V");
            defaults[active]++; defaultsTotal++; defaultPhase = 1;
            event("default_enter", hex(currentU)); clocks.enable();
        }
        void drawEnter(MethodEntryEvent event) throws Exception {
            need(!drawOpen && defaultPhase == 0 && marker == null && (mathStage() && mathOpen || active == 3 && gaussianOpen),
                "unexpected-next-double");
            ObjectReference value = receiver(event.thread());
            if (mathStage()) {
                need(holderDone && holder != null && holder.isInitialized() && !holder.failedToInitialize(), "draw-before-holder-ready");
                ObjectReference published = reference(holder.getValue(holderOwner));
                need(published.equals(value), "math-receiver-holder-mismatch");
                if (mathOwner == null) { need(active == 5, "missing-first-math-owner"); mathOwner = value; }
                need(value.equals(mathOwner) && draws[active] == 0, "math-receiver-changed");
            } else need(value.equals(owner("E")), "gaussian-draw-receiver");
            stableU(); drawOpen = true; drawOwner = value; drawEntry = state(value); nextPart = 0;
            draws[active]++;
            if (active == 5) {
                State completed = constructed.get(active);
                fresh(drawEntry); need(completed != null && completed.same(drawEntry), "holder-constructor-owner-publication");
            }
            if (mathStage() && active != 5) need(beforeOwners.get("M").same(drawEntry), "math-continuation-entry");
            emitState("draw_enter", mathStage() ? "M" : "E", drawEntry);
        }
        void randomEntry(MethodEntryEvent event) throws Exception {
            Method method = event.method(); String name = method.name(), sig = method.signature();
            need(random != null && method.declaringType().equals(random), "random-method-owner");
            mainThread(event.thread());
            if (name.equals("<init>") && sig.equals("()V")) defaultEnter(event);
            else if (name.equals("<init>") && sig.equals("(J)V")) {
                List<Value> args = arguments(event.thread()); need(args.size() == 1, "constructor-argument-count");
                long argument = longValue(args.get(0));
                if (defaultPhase == 4) {
                    need(argument == (committedU ^ clock), "delegate-argument-clock-binding");
                    defaultPhase = 5; event("delegate_enter", hex(argument));
                } else {
                    need(active == 2 && defaultPhase == 0 && !seededOpen && ++explicitEntries == 1 && argument == 0,
                        "unexpected-explicit-constructor");
                    stableU(); seededOpen = true; event("explicit_enter", hex(argument));
                }
            } else if (name.equals("seedUniquifier") && sig.equals("()J")) {
                need(defaultPhase == 1, "uniquifier-entry-order"); stableU(); defaultPhase = 2;
                event("uniquifier_enter", hex(currentU));
            } else if (name.equals("nextGaussian") && sig.equals("()D")) {
                need(active == 3 && !gaussianOpen && ++gaussianEntries == 1
                    && receiver(event.thread()).equals(owner("E")), "unexpected-gaussian");
                stableU(); gaussianOpen = true; event("gaussian_enter");
            } else if (name.equals("setSeed") && sig.equals("(J)V")) {
                List<Value> args = arguments(event.thread());
                need(active == 4 && !reseedOpen && ++reseedEntries == 1 && args.size() == 1
                    && longValue(args.get(0)) == -1 && receiver(event.thread()).equals(owner("E")), "unexpected-reseed");
                stableU(); reseedOpen = true; event("reseed_enter", hex(longValue(args.get(0))));
            } else if (name.equals("nextDouble") && sig.equals("()D")) drawEnter(event);
            else if (name.equals("next") && sig.equals("(I)I")) {
                List<Value> args = arguments(event.thread());
                need(drawOpen && !nextOpen && nextPart < 2 && args.size() == 1 && args.get(0) instanceof IntegerValue
                    && receiver(event.thread()).equals(drawOwner), "unexpected-next");
                nextBits = ((IntegerValue) args.get(0)).value();
                need(nextBits == (nextPart == 0 ? 26 : 27), "next-bit-order");
                nextOpen = true; event("next_enter", hex(nextBits), hex(state(drawOwner).state));
            } else if (name.equals("initialScramble") && sig.equals("(J)J")) {
                need(defaultPhase == 5 || seededOpen || reseedOpen, "unexpected-scramble"); ignored++;
            } else if (name.equals("<clinit>") && sig.equals("()V")) {
                need(active == 0, "unexpected-random-initialization"); event("random_clinit_enter");
            } else throw new Rejected("unexpected-random-entry");
        }
        void randomExit(MethodExitEvent event) throws Exception {
            Method method = event.method(); String name = method.name(), sig = method.signature();
            need(random != null && method.declaringType().equals(random), "random-method-owner");
            mainThread(event.thread());
            if (name.equals("<init>") && sig.equals("()V")) {
                need(defaultPhase == 6 && readU() == committedU && event.returnValue() instanceof VoidValue, "default-return-order");
                State completed = state(receiver(event.thread()));
                need(pendingDefault != null && pendingDefault.same(completed), "default-delegate-owner-mismatch");
                constructed.put(active, completed); pendingDefault = null;
                emitState("default_return", active == 1 ? "A" : active == 7 ? "B" : "M", completed);
                clocks.disable(); defaultPhase = 0; event("default_return", hex(currentU));
            } else if (name.equals("<init>") && sig.equals("(J)V")) {
                need(event.returnValue() instanceof VoidValue, "constructor-return-type");
                if (defaultPhase == 5) {
                    need(pendingDefault == null, "overlapping-default-owner");
                    pendingDefault = state(receiver(event.thread())); fresh(pendingDefault);
                    emitState("delegate_return", active == 1 ? "A" : active == 7 ? "B" : "M", pendingDefault);
                    defaultPhase = 6; delegateReturns++; event("delegate_return");
                }
                else {
                    need(active == 2 && seededOpen && ++explicitExits == 1, "explicit-return-order");
                    explicitState = state(receiver(event.thread())); fresh(explicitState);
                    emitState("explicit_return", "E", explicitState);
                    seededOpen = false; stableU(); event("explicit_return");
                }
            } else if (name.equals("seedUniquifier") && sig.equals("()J")) {
                need(defaultPhase == 2, "uniquifier-return-order"); committedU = longValue(event.returnValue());
                need(readU() == committedU, "uniquifier-return-field-mismatch"); currentU = committedU;
                defaultPhase = 3; uniqueReturns++;
                event("uniquifier_return", hex(committedU), hex(caller(event.thread(), random, "<init>", "()V")));
            } else if (name.equals("nextGaussian") && sig.equals("()D")) {
                need(active == 3 && gaussianOpen && !drawOpen && !nextOpen, "gaussian-return-order");
                gaussianResult = doubleBits(event.returnValue()); gaussianOpen = false; gaussianExits++;
                stableU(); event("gaussian_return", hex(gaussianResult));
            } else if (name.equals("setSeed") && sig.equals("(J)V")) {
                need(active == 4 && reseedOpen && event.returnValue() instanceof VoidValue, "reseed-return-order");
                reseedOpen = false; reseedExits++; stableU(); event("reseed_return");
            } else if (name.equals("next") && sig.equals("(I)I")) {
                need(drawOpen && nextOpen && event.returnValue() instanceof IntegerValue, "next-return-order");
                int actual = ((IntegerValue) event.returnValue()).value();
                event("next_return", hex(nextBits), hex(Integer.toUnsignedLong(actual)), hex(state(drawOwner).state));
                nextOpen = false; nextPart++; nextReturns++; stableU();
            } else if (name.equals("nextDouble") && sig.equals("()D")) {
                need(drawOpen && !nextOpen && nextPart == 2, "draw-return-order");
                drawResult = doubleBits(event.returnValue());
                event("draw_return", hex(drawResult)); emitState("draw_return", mathStage() ? "M" : "E", state(drawOwner));
                drawOpen = false; drawOwner = null; drawEntry = null; drawReturns++; stableU();
            } else if (name.equals("initialScramble") && sig.equals("(J)J")) {
                need(defaultPhase == 5 || seededOpen || reseedOpen, "unexpected-scramble-return"); ignored++;
            } else if (name.equals("<clinit>") && sig.equals("()V")) {
                need(active == 0, "unexpected-random-init-return"); event("random_clinit_return");
            } else throw new Rejected("unexpected-random-exit");
        }
        void enter(MethodEntryEvent event) throws Exception {
            need(startedVM && !died, "entry-lifecycle"); entries++;
            Method method = event.method(); ReferenceType type = method.declaringType();
            if (type.name().equals(TARGET)) {
                need(target != null && type.equals(target), "target-method-owner");
                if ((method.name().equals("before") || method.name().equals("after")) && method.signature().equals("(I)V")) markerEntry(event);
                else if (method.name().equals("main") && method.signature().equals("([Ljava/lang/String;)V")) {
                    mainThread(event.thread()); List<Value> args = arguments(event.thread());
                    need(++mainEntries == 1 && args.size() == 1 && args.get(0) instanceof ArrayReference
                        && ((ArrayReference) args.get(0)).length() == 0, "target-main-entry");
                    out.row("life", "main_enter");
                } else throw new Rejected("unexpected-target-entry");
            } else if (type.name().equals(RANDOM)) randomEntry(event);
            else if (type.name().equals(HOLDER)) {
                mainThread(event.thread());
                need(holder != null && type.equals(holder) && method.name().equals("<clinit>") && method.signature().equals("()V")
                    && active == 5 && mathOpen && !holderOpen && !holderDone && ++holderEntries == 1, "holder-init-entry-order");
                holderOpen = true; event("holder_clinit_enter");
            } else if (type.name().equals(MATH)) {
                need(math != null && type.equals(math), "math-method-owner");
                if (method.name().equals("random") && method.signature().equals("()D")) {
                    mainThread(event.thread());
                    need(mathStage() && !mathOpen && marker == null, "unexpected-math-random");
                    mathOpen = true; mathEntries++; stableU(); event("math_enter");
                } else ignored++;
            } else throw new Rejected("unexpected-method-entry-class");
        }
        void exit(MethodExitEvent event) throws Exception {
            need(startedVM && !died, "exit-lifecycle"); exits++;
            Method method = event.method(); ReferenceType type = method.declaringType();
            if (type.name().equals(TARGET)) {
                need(target != null && type.equals(target), "target-method-owner");
                if ((method.name().equals("before") || method.name().equals("after")) && method.signature().equals("(I)V")) markerExit(event);
                else if (method.name().equals("main") && method.signature().equals("([Ljava/lang/String;)V")) {
                    mainThread(event.thread());
                    need(nextStage == STAGES.length && marker == null && ++mainExits == 1
                        && event.returnValue() instanceof VoidValue, "target-main-return"); out.row("life", "main_return");
                } else throw new Rejected("unexpected-target-exit");
            } else if (type.name().equals(RANDOM)) randomExit(event);
            else if (type.name().equals("java.lang.System")) {
                mainThread(event.thread()); bootstrap(type, "java.lang.System");
                need(defaultPhase == 3 && method.name().equals("nanoTime") && method.signature().equals("()J")
                    && method.isNative() && method.isStatic(), "unexpected-system-return");
                if (clockReturns == 0) method(type, "nanoTime", "()J", true);
                clock = longValue(event.returnValue()); clockReturns++; defaultPhase = 4;
                event("nano_time_return", hex(clock), hex(caller(event.thread(), random, "<init>", "()V")));
            } else if (type.name().equals(HOLDER)) {
                mainThread(event.thread());
                need(holder != null && type.equals(holder) && method.name().equals("<clinit>") && method.signature().equals("()V")
                    && holderOpen && !holderDone && defaultPhase == 0 && active == 5
                    && ++holderExits == 1 && event.returnValue() instanceof VoidValue, "holder-init-return-order");
                holderOpen = false; holderDone = true; event("holder_clinit_return");
            } else if (type.name().equals(MATH)) {
                need(math != null && type.equals(math), "math-method-owner");
                if (method.name().equals("random") && method.signature().equals("()D")) {
                    mainThread(event.thread());
                    need(mathStage() && mathOpen && !drawOpen && draws[active] == 1, "math-return-order");
                    mathResult = doubleBits(event.returnValue()); need(mathResult == drawResult, "math-forwarded-result");
                    mathOpen = false; mathExits++; stableU(); event("math_return", hex(mathResult));
                } else ignored++;
            } else throw new Rejected("unexpected-method-exit-class");
        }
        void complete() {
            need(startedVM && nextStage == STAGES.length && active == -1 && marker == null && mainEntries == 1 && mainExits == 1
                && beforeCount == 10 && afterCount == 10 && markerExits == 20, "incomplete-target-markers");
            need(defaultsTotal == 3 && uniqueReturns == 3 && clockReturns == 3 && delegateReturns == 3
                && explicitEntries == 1 && explicitExits == 1 && gaussianEntries == 1 && gaussianExits == 1
                && reseedEntries == 1 && reseedExits == 1 && mathEntries == 3 && mathExits == 3,
                "incomplete-api-receipts");
            need(holderPrepares == 1 && holderEntries == 1 && holderExits == 1 && holderDone && !holderOpen,
                "incomplete-holder-receipts");
            need(defaultPhase == 0 && !seededOpen && !gaussianOpen && !reseedOpen && !mathOpen && !drawOpen && !nextOpen
                && pendingDefault == null && constructed.size() == 3 && explicitState != null
                && nextReturns == 2 * drawReturns && draws[5] == 1 && draws[6] == 1 && draws[8] == 1,
                "incomplete-draw-receipts");
        }
        void run() throws Exception {
            out.row("meta", "protocol", "random-constructor-1");
            out.text("vm_name", vm.name()); out.text("vm_version", vm.version()); out.text("vm_description", vm.description());
            need(vm.canGetMethodReturnValues(), "method-return-values-unsupported");
            out.row("meta", "can_get_method_return_values", "1");
            out.row("meta", "mirror_policy", "strong_and_disable_collection");
            EventQueue queue = vm.eventQueue();
            while (!disconnected) {
                EventSet set = queue.remove(Math.min(1000, remainingMillis()));
                if (set == null) continue;
                for (Event event : set) {
                    need(++delivered <= EVENT_LIMIT, "delivered-event-bound"); remainingMillis();
                    if (event instanceof VMStartEvent start) {
                        need(set.suspendPolicy() == EventRequest.SUSPEND_ALL, "vm-start-not-suspend-all"); start(start);
                    } else if (event instanceof VMDeathEvent) {
                        need(!died, "duplicate-vm-death"); complete(); died = true; out.row("life", "vm_death");
                    } else if (event instanceof VMDisconnectEvent) {
                        need(died && !disconnected, "disconnect-before-death"); disconnected = true; out.row("life", "vm_disconnect");
                    } else {
                        need(set.suspendPolicy() == EventRequest.SUSPEND_ALL, "observation-not-suspend-all");
                        if (event instanceof ClassPrepareEvent prepare) prepare(prepare);
                        else if (event instanceof MethodEntryEvent enter) enter(enter);
                        else if (event instanceof MethodExitEvent exit) exit(exit);
                        else throw new Rejected("unexpected-event-kind");
                    }
                }
                remainingMillis();
                if (!disconnected) {
                    try { set.resume(); }
                    catch (VMDisconnectedException error) { need(died, "resume-disconnected-before-death"); }
                }
            }
            remainingMillis();
            complete();
            out.row("count", "delivered", Integer.toString(delivered));
            out.row("count", "method_entries", Integer.toString(entries));
            out.row("count", "method_exits", Integer.toString(exits));
            out.row("count", "ignored_method_events", Integer.toString(ignored));
            out.row("count", "class_prepares", Integer.toString(prepares));
            out.row("count", "held_pinned_mirrors", Integer.toString(held.size()));
            out.row("count", "marker_entries", Integer.toString(beforeCount + afterCount));
            out.row("count", "marker_exits", Integer.toString(markerExits));
            out.row("count", "default_constructors", Integer.toString(defaultsTotal));
            out.row("count", "uniquifier_returns", Integer.toString(uniqueReturns));
            out.row("count", "nano_time_returns", Integer.toString(clockReturns));
            out.row("count", "delegate_returns", Integer.toString(delegateReturns));
            out.row("count", "next_double_returns", Integer.toString(drawReturns));
            out.row("count", "next_returns", Integer.toString(nextReturns));
            out.row("result", "complete");
        }
    }

    static int port(String text) {
        need(text.length() >= 1 && text.length() <= 5, "port-syntax"); int value = 0;
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i); need(c >= '0' && c <= '9', "port-syntax"); value = value * 10 + c - '0';
        }
        need(value >= 1 && value <= 65535, "port-range"); return value;
    }
    public static void main(String[] args) {
        try {
            need(args.length == 1, "one-loopback-port-required"); int port = port(args[0]);
            long started = System.nanoTime(); AttachingConnector socket = null;
            for (AttachingConnector candidate : Bootstrap.virtualMachineManager().attachingConnectors()) {
                if (candidate.name().equals("com.sun.jdi.SocketAttach")) {
                    need(socket == null, "duplicate-socket-connector"); socket = candidate;
                }
            }
            need(socket != null, "socket-connector-unavailable");
            Map<String,Connector.Argument> arguments = socket.defaultArguments();
            need(arguments.containsKey("hostname") && arguments.containsKey("port") && arguments.containsKey("timeout"),
                "socket-connector-arguments");
            arguments.get("hostname").setValue("127.0.0.1"); arguments.get("port").setValue(Integer.toString(port));
            arguments.get("timeout").setValue("10000");
            VirtualMachine vm = socket.attach(arguments);
            new Session(vm, new Trace(), started).run();
        } catch (Rejected error) {
            System.err.println("random-constructor-observer: " + error.getMessage()); System.exit(1);
        } catch (Throwable error) {
            // Do not emit transport exception messages, commands, addresses, paths, or stack traces.
            System.err.println("random-constructor-observer: jdi-or-observer-failure " + error.getClass().getName()); System.exit(1);
        }
    }
}
