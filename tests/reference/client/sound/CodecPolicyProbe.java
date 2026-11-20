import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import javax.sound.sampled.AudioFormat;

/* Executes unchanged supplied classes. No decoder field writes or devices. */
public final class CodecPolicyProbe {
    private static Object codec;
    private static Method initialized, eos, format;
    private static Field count, index;
    private static Path output;

    private static void emit(String operation, int sequence, Object buffer) throws Exception {
        byte[] data = buffer == null ? null
            : (byte[]) buffer.getClass().getField("audioData").get(buffer);
        AudioFormat audio = (AudioFormat) format.invoke(codec);
        System.out.println(operation + "\t" + sequence + "\t0\t" + (buffer != null ? 1 : 0)
            + "\t" + (data == null ? 1 : 0) + "\t" + (data == null ? 0 : data.length)
            + "\t" + ((Boolean) initialized.invoke(codec) ? 1 : 0)
            + "\t" + ((Boolean) eos.invoke(codec) ? 1 : 0)
            + "\t" + (audio == null ? 0 : audio.getChannels())
            + "\t" + (audio == null ? 0 : (long) audio.getSampleRate())
            + "\t" + count.getInt(codec) + "\t" + index.getInt(codec));
        if (data != null)
            Files.write(output.resolve("trace-" + sequence + ".pcm"), data);
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 3 || args.length > 6)
            throw new IllegalArgumentException("jar input output [streaming_size] [all] [max_file_size]");
        output = Path.of(args[2]);
        try (URLClassLoader loader = new URLClassLoader(
                new URL[] {new File(args[0]).toURI().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Class<?> config = loader.loadClass("paulscode.sound.SoundSystemConfig");
            Class<?> logger = loader.loadClass("paulscode.sound.SoundSystemLogger");
            config.getMethod("setLogger", logger).invoke(null, logger.getConstructor().newInstance());
            if (args.length >= 4)
                config.getMethod("setStreamingBufferSize", int.class).invoke(null, Integer.parseInt(args[3]));
            boolean all = args.length >= 5 && args[4].equals("all");
            if (args.length >= 6)
                config.getMethod("setMaxFileSize", int.class).invoke(null, Integer.parseInt(args[5]));
            Class<?> type = loader.loadClass("paulscode.sound.codecs.CodecJOrbis");
            codec = type.getConstructor().newInstance();
            initialized = type.getMethod("initialized");
            eos = type.getMethod("endOfStream");
            format = type.getMethod("getAudioFormat");
            count = type.getDeclaredField("count");
            index = type.getDeclaredField("index");
            count.setAccessible(true);
            index.setAccessible(true);
            if (!(Boolean) type.getMethod("initialize", URL.class).invoke(
                    codec, new File(args[1]).toURI().toURL()))
                throw new IllegalStateException("Original initialization failed");
            int sequence = 0;
            emit("INIT", sequence++, null);
            if (all) {
                emit("READ_ALL", sequence++, type.getMethod("readAll").invoke(codec));
                type.getMethod("cleanup").invoke(codec);
                emit("CLEANUP", sequence, null);
                return;
            }
            Method read = type.getMethod("read");
            int steps = 0;
            while (!(Boolean) eos.invoke(codec)) {
                if (++steps > 1024)
                    throw new IllegalStateException("Original read work bound");
                emit("READ", sequence++, read.invoke(codec));
            }
            emit("READ", sequence++, read.invoke(codec));
            emit("READ_ALL", sequence++, type.getMethod("readAll").invoke(codec));
            type.getMethod("cleanup").invoke(codec);
            emit("CLEANUP", sequence, null);
        }
    }
}
