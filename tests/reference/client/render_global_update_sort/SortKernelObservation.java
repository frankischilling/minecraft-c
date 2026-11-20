import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.HexFormat;

/** Authored integer comparator controls on the real, unmodified JDK sort. */
public final class SortKernelObservation {
    static final class Value {
        int id, key, ordinal, visible;
    }
    static final class Stop extends RuntimeException {
        private static final long serialVersionUID = 1;
    }
    static final class Integers implements AutoCloseable {
        final InputStream input;
        Integers(String path) throws Exception {
            input = new BufferedInputStream(new FileInputStream(path));
        }
        int next() throws Exception {
            int c;
            do { c = input.read(); } while (c >= 0 && c <= 32);
            if (c < 0) return -1;
            if (c < '0' || c > '9') throw new AssertionError("invalid fixture integer");
            int value = 0;
            do {
                value = Math.addExact(Math.multiplyExact(value,10),c-'0');
                c = input.read();
            } while (c >= '0' && c <= '9');
            if (c > 32) throw new AssertionError("invalid fixture separator");
            return value;
        }
        @Override public void close() throws java.io.IOException { input.close(); }
    }
    static long hash;
    static int calls;
    static StringBuilder trace;
    static void add(int value) { hash = (hash ^ (long)value) * 1099511628211L; }
    static void result(int value) { add(value); trace.append(' ').append(value); }
    static int compare(Value a, Value b, int mode, int stop) {
        ++calls;
        add(a.id); add(b.id);
        trace.append(' ').append(a.id).append(' ').append(b.id);
        if (calls == stop) { result(91); throw new Stop(); }
        int order;
        if (mode == 0) {
            if (a.visible != b.visible) order = a.visible > b.visible ? 1 : -1;
            else if (a.key != b.key) order = a.key < b.key ? 1 : -1;
            else order = a.ordinal < b.ordinal ? 1 : -1;
        } else if (mode == 1) {
            order = Integer.compare(a.key,b.key);
        } else if (mode == 2) {
            int relation = (a.key-b.key+3072)%3;
            order = relation == 1 ? 1 : relation == 2 ? -1 : 0;
        } else {
            order = a.key < b.key ? 1 : -1;
        }
        result(order);
        return order;
    }
    static void classIdentity(String name) throws Exception {
        Class<?> type = Class.forName(name,false,null);
        try (InputStream bytes = type.getResourceAsStream(type.getSimpleName()+".class")) {
            if (bytes == null || type.getClassLoader() != null)
                throw new AssertionError("bootstrap class identity unavailable");
            System.out.println("CLASS "+name+" "+HexFormat.of().formatHex(
                MessageDigest.getInstance("SHA-256").digest(bytes.readAllBytes())));
        }
    }
    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 1 || Runtime.version().feature() != 17)
            throw new AssertionError("fixture path and JDK17 required");
        System.out.println("RUNTIME "+System.getProperty("java.runtime.version")+" | "+
            System.getProperty("java.vendor")+" | "+System.getProperty("java.vm.name"));
        classIdentity("java.util.Arrays");
        classIdentity("java.util.TimSort");
        int cases = 0;
        try (Integers input = new Integers(arguments[0])) {
            int count;
            while ((count = input.next()) >= 0) {
                final int mode = input.next(), stop = input.next();
                if (count > 4096 || mode < 0 || mode > 3 || stop < 0)
                    throw new AssertionError("fixture header extent");
                Value[] values = new Value[count];
                for (int i = 0; i < count; ++i) {
                    Value value = new Value(); value.id = i;
                    value.key = input.next(); value.ordinal = input.next(); value.visible = input.next();
                    if (value.key < 0 || value.key > 1024 || value.ordinal < 0 ||
                        value.ordinal > 1024 || value.visible < 0 || value.visible > 1)
                        throw new AssertionError("fixture value extent");
                    values[i] = value;
                }
                hash = 0xcbf29ce484222325L; calls = 0; trace = new StringBuilder();
                int status = 0;
                try { Arrays.sort(values,(a,b) -> compare(a,b,mode,stop)); }
                catch (Stop stopped) { status = 1; }
                catch (IllegalArgumentException contract) {
                    if (!"Comparison method violates its general contract!".equals(contract.getMessage()))
                        throw contract;
                    status = 2;
                }
                StringBuilder row = new StringBuilder();
                row.append(status).append(' ').append(calls).append(' ')
                   .append(String.format(java.util.Locale.ROOT,"%016x",hash));
                for (Value value : values) row.append(' ').append(value.id);
                row.append(trace);
                System.out.println(row);
                ++cases;
            }
        }
        if (cases == 0) throw new AssertionError("empty fixture");
        System.out.println("Sort kernel fixture: "+cases+" cases");
    }
}
