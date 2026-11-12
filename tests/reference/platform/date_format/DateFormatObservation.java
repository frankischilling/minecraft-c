import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.time.Instant;
import java.time.ZoneId;
import java.time.zone.ZoneOffsetTransition;
import java.time.zone.ZoneRules;
import java.util.TreeSet;

/** Observe the selected JDK's real no-argument constructor and UTF16 result. */
public final class DateFormatObservation {
    private static String units(String text) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < text.length(); ++i) {
            if (i != 0) out.append(',');
            out.append(String.format(Locale.ROOT, "%04x", (int) text.charAt(i)));
        }
        return out.toString();
    }

    public static void main(String[] args) {
        if (args.length == 1 && args[0].equals("--vectors")) {
            vectors();
            return;
        }
        SimpleDateFormat format = new SimpleDateFormat();
        System.out.println("locale=" + Locale.getDefault(Locale.Category.FORMAT).toLanguageTag());
        System.out.println("timezone=" + format.getTimeZone().getID());
        System.out.println("pattern=" + units(format.toPattern()));
        for (String value : args) {
            long millis = Long.parseLong(value);
            String before = format.format(new Date(millis));
            Locale.setDefault(Locale.Category.FORMAT, Locale.JAPAN);
            TimeZone.setDefault(TimeZone.getTimeZone("GMT+11:00"));
            String after = format.format(new Date(millis));
            if (!before.equals(after)) throw new AssertionError("Constructor capture changed");
            System.out.println(millis + "=" + units(before));
        }
    }

    private static void vectors() {
        TreeSet<Long> values = new TreeSet<>();
        long start = Instant.parse("1967-01-01T00:00:00Z").toEpochMilli();
        long end = Instant.parse("2100-01-01T00:00:00Z").toEpochMilli();
        values.add(start);
        values.add(end - 1);
        values.add(-1L);
        values.add(0L);
        ZoneRules rules = ZoneId.of("America/New_York").getRules();
        ZoneOffsetTransition next = rules.nextTransition(Instant.ofEpochMilli(start - 1));
        while (next != null && next.getInstant().toEpochMilli() < end) {
            long millis = next.getInstant().toEpochMilli();
            for (long delta : new long[] {-60001, -1, 0, 1, 60000}) values.add(millis + delta);
            next = rules.nextTransition(next.getInstant().plusSeconds(1));
        }
        for (int year = 1967; year <= 2099; ++year) {
            for (String suffix : new String[] {"-01-01T00:00:00Z", "-03-01T00:00:00Z",
                                               "-07-01T00:00:00Z", "-12-31T23:59:59Z"}) {
                long millis = Instant.parse(year + suffix).toEpochMilli();
                for (long delta : new long[] {-1, 0, 1}) {
                    if (millis + delta >= start && millis + delta < end) values.add(millis + delta);
                }
            }
        }
        Locale.setDefault(Locale.Category.FORMAT, Locale.US);
        TimeZone.setDefault(TimeZone.getTimeZone("America/New_York"));
        SimpleDateFormat eastern = new SimpleDateFormat();
        TimeZone.setDefault(TimeZone.getTimeZone("UTC"));
        SimpleDateFormat utc = new SimpleDateFormat();
        System.out.println("# JDK " + System.getProperty("java.runtime.version"));
        System.out.println("# FORMAT en-US; America/New_York and UTC; pattern " + units(utc.toPattern()));
        for (long millis : values) {
            Date date = new Date(millis);
            System.out.println(millis + "\t" + units(eastern.format(date)) + "\t" + units(utc.format(date)));
        }
    }
}
