import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/* Observes unchanged genuine constructors only. No replacement game class,
 * constructor instrumentation, Unsafe receiver or copied geometry builder. */
public final class BipedObservation {
    static void require(boolean value) { if (!value) throw new AssertionError("Biped observer contract"); }
    static Object get(Object object, Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name); field.setAccessible(true); return field.get(object);
    }
    static String f(Object o, Class<?> c, String n) throws Exception { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits((Float)get(o,c,n))); }
    static int b(Object o, Class<?> c, String n) throws Exception { return (Boolean)get(o,c,n) ? 1 : 0; }
    static Object[] parts(fh model) throws Exception {
        Object[] result = new Object[9];
        for (int i = 0; i < 9; ++i) result[i] = get(model,fh.class,String.valueOf((char)('a'+i)));
        return result;
    }
    static Set<Object> identitySet() { return Collections.newSetFromMap(new IdentityHashMap<>()); }
    static Set<Object> graph(fh model) throws Exception {
        Set<Object> all = identitySet(), positions = identitySet(), vertices = identitySet();
        all.add(model);
        for (Object part : parts(model)) {
            require(all.add(part)); Object[] originals = (Object[])get(part,ps.class,"j"), quads = (Object[])get(part,ps.class,"k");
            require(all.add(originals) && all.add(quads));
            for (Object v : originals) { require(vertices.add(v) && all.add(v)); Object p = get(v,ib.class,"a"); require(positions.add(p) && all.add(p)); }
            for (Object quad : quads) {
                require(all.add(quad)); Object[] array = (Object[])get(quad,tz.class,"a"); require(all.add(array));
                require((Integer)get(quad,tz.class,"b") == 4 && !(Boolean)get(quad,tz.class,"c"));
                for (Object v : array) {
                    require(vertices.add(v) && all.add(v)); boolean found = false;
                    for (Object original : originals) found |= get(v,ib.class,"a") == get(original,ib.class,"a");
                    require(found);
                }
            }
        }
        require(positions.size() == 72 && vertices.size() == 288);
        return all;
    }
    static void geometry(fh model) throws Exception {
        System.out.println("BIPED "+f(model,ko.class,"m")+" "+b(model,ko.class,"n")+" "+b(model,fh.class,"j")+" "+b(model,fh.class,"k")+" "+b(model,fh.class,"l"));
        Object[] parts = parts(model);
        for (int p = 0; p < 9; ++p) {
            Object part = parts[p]; StringBuilder row = new StringBuilder("PART "+p+" "+get(part,ps.class,"l")+" "+get(part,ps.class,"m"));
            for (String n : new String[]{"g","h","i","n"}) row.append(' ').append(b(part,ps.class,n));
            row.append(' ').append(get(part,ps.class,"o"));
            for (String n : new String[]{"a","b","c","d","e","f"}) row.append(' ').append(f(part,ps.class,n));
            System.out.println(row);
            Object[] originals = (Object[])get(part,ps.class,"j"), quads = (Object[])get(part,ps.class,"k");
            for (int i = 0; i < 8; ++i) {
                Object v = originals[i], position = get(v,ib.class,"a"); row = new StringBuilder("VERTEX "+p+" "+i);
                for (String n : new String[]{"a","b","c"}) row.append(String.format(Locale.ROOT," %016x",Double.doubleToRawLongBits((Double)get(position,bt.class,n))));
                row.append(' ').append(f(v,ib.class,"b")).append(' ').append(f(v,ib.class,"c")); System.out.println(row);
            }
            for (int q = 0; q < 6; ++q) {
                Object quad = quads[q]; row = new StringBuilder("QUAD "+p+" "+q+" "+get(quad,tz.class,"b")+" "+b(quad,tz.class,"c"));
                for (Object v : (Object[])get(quad,tz.class,"a")) {
                    int index = -1; boolean copy = true;
                    for (int i = 0; i < 8; ++i) { if (get(v,ib.class,"a") == get(originals[i],ib.class,"a")) index = i; if (v == originals[i]) copy = false; }
                    row.append(' ').append(index).append(' ').append(copy ? 1 : 0).append(' ').append(f(v,ib.class,"b")).append(' ').append(f(v,ib.class,"c"));
                }
                System.out.println(row);
            }
        }
    }
    public static void main(String[] args) throws Exception {
        require(args.length == 1); String mode = args[0];
        require(Set.of("cold","pair","pool","notice").contains(mode));
        Field counter = Random.class.getDeclaredField("seedUniquifier"); counter.setAccessible(true);
        AtomicLong uniquifier = (AtomicLong)counter.get(null);
        bt poolA = null, poolB = null; Object poolIdentity = null; int poolSize = 0, poolCursor = 0;
        if (mode.equals("pool")) {
            poolA = bt.b(1,2,3); poolB = bt.b(4,5,6); poolIdentity = get(null,bt.class,"d");
            poolSize = ((List<?>)poolIdentity).size(); poolCursor = (Integer)get(null,bt.class,"e");
        }
        vu notice = null; fh noticeModel = null;
        if (mode.equals("notice")) {
            notice = new vu(null); Object item = get(notice,vu.class,"h"); noticeModel = (fh)get(item,bw.class,"a");
            require(get(item,bw.class,"e") != get(item,bb.class,"e") && get(item,bb.class,"f") != null);
            System.out.println("CASE notice-base"); geometry(noticeModel); graph(noticeModel);
        }
        long before = uniquifier.get(); fh first = new fh(0.0f);
        System.out.println("CASE "+mode); geometry(first); Set<Object> firstGraph = graph(first); System.out.println("ALIASES 72 216 1");
        if (mode.equals("pair")) {
            fh second = new fh(0.0f); Set<Object> secondGraph = graph(second);
            for (Object object : firstGraph) require(!secondGraph.contains(object));
            System.out.println("CASE pair-second"); geometry(second); System.out.println("DISTINCT 1");
        }
        require(before == uniquifier.get()); System.out.println("RANDOM unchanged 1");
        if (mode.equals("pool")) {
            require(get(null,bt.class,"d") == poolIdentity && ((List<?>)poolIdentity).size() == poolSize && (Integer)get(null,bt.class,"e") == poolCursor);
            require(poolA.a == 1 && poolA.b == 2 && poolA.c == 3 && poolB.a == 4 && poolB.b == 5 && poolB.c == 6);
            System.out.println("POOL 2 2 unchanged 1");
        }
        if (mode.equals("notice")) {
            Set<Object> other = graph(noticeModel); for (Object object : firstGraph) require(!other.contains(object));
            System.out.println("NOTICE distinct 1");
            notice = null; noticeModel = null; other = null; graph(first);
            /* Java drops these references; this is not deterministic GC or a
             * Java destructor observation. Native retirement is separate. */
            System.out.println("LIFETIME notice-first 1");
        }
        System.out.println("END clean");
    }
}
