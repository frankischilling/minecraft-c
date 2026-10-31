import java.io.InputStream;
import java.io.Writer;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Properties;
import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;

/* Calls unchanged supplied classes. Only genuine configuration setters receive
 * selected inputs; no decoder field, format, expected byte or flag is installed. */
public final class WavOriginalProbe {
    private static Object codec;
    private static Class<?> type;
    private static Path output;
    private static int sequence;

    private static void emit(String operation, Object buffer, Throwable failure) throws Exception {
        Field field = type.getDeclaredField("myAudioInputStream");
        field.setAccessible(true);
        AudioInputStream stream = (AudioInputStream) field.get(codec);
        AudioFormat format = (AudioFormat) type.getMethod("getAudioFormat").invoke(codec);
        byte[] data = buffer == null ? null
            : (byte[]) buffer.getClass().getField("audioData").get(buffer);
        String fields = format == null ? "NONE\t0\t0\t0\t0\t0\t-1"
            : format.getEncoding() + "\t" + format.getChannels() + "\t"
              + format.getSampleSizeInBits() + "\t" + format.getFrameSize() + "\t"
              + Integer.toUnsignedString(Float.floatToRawIntBits(format.getSampleRate())) + "\t"
              + (format.isBigEndian() ? 1 : 0) + "\t" + stream.getFrameLength();
        System.out.println(operation + "\t" + sequence + "\t"
            + type.getMethod("initialized").invoke(codec) + "\t"
            + type.getMethod("endOfStream").invoke(codec) + "\t"
            + (buffer != null) + "\t" + (data == null ? -1 : data.length) + "\t"
            + (failure == null ? "NONE" : failure.getClass().getSimpleName()) + "\t" + fields);
        if (data != null)
            Files.write(output.resolve("pcm-" + sequence + ".bin"), data);
        sequence++;
    }

    private static void call(String method) throws Exception {
        try {
            emit(method, type.getMethod(method).invoke(codec), null);
        } catch (InvocationTargetException failure) {
            emit(method, null, failure.getCause());
        }
    }

    private static String hex(byte[] data) {
        StringBuilder text = new StringBuilder(data.length * 2);
        for (byte value : data)
            text.append(String.format("%02x", value & 255));
        return text.toString();
    }

    private static void resources(ClassLoader loader) throws Exception {
        String[] names = {
            "paulscode/sound/codecs/CodecWav.class", "paulscode/sound/ICodec.class",
            "paulscode/sound/SoundBuffer.class", "paulscode/sound/SoundSystemConfig.class",
            "paulscode/sound/SoundSystemLogger.class", "javax/sound/sampled/AudioInputStream.class",
            "javax/sound/sampled/AudioSystem.class", "javax/sound/sampled/AudioFormat.class",
            "com/sun/media/sound/WaveFileReader.class",
            "com/sun/media/sound/WaveExtensibleFileReader.class",
            "com/sun/media/sound/WaveFloatFileReader.class", "com/sun/media/sound/RIFFReader.class",
            "java/io/BufferedInputStream.class", "java/io/FileInputStream.class"
        };
        StringBuilder rows = new StringBuilder();
        for (String name : names) {
            try (InputStream stream = loader.getResourceAsStream(name)) {
                if (stream == null)
                    throw new IllegalStateException("Missing class resource: " + name);
                byte[] bytes = stream.readAllBytes();
                rows.append(name).append('\t').append(bytes.length).append('\t')
                    .append(hex(MessageDigest.getInstance("SHA-256").digest(bytes))).append('\n');
            }
        }
        Files.writeString(output.resolve("class-resources.tsv"), rows, StandardCharsets.US_ASCII);
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 7 || !(args[3].equals("read") || args[3].equals("all")))
            throw new IllegalArgumentException("jar input output read|all streaming_size chunk_size maximum");
        if (Runtime.version().feature() != 17 || ByteOrder.nativeOrder() != ByteOrder.LITTLE_ENDIAN)
            throw new IllegalStateException("This comparison requires little-endian JDK17");
        output = Path.of(args[2]);
        Properties environment = new Properties();
        for (String key : new String[] {"java.vendor", "java.version", "java.runtime.version",
                "java.specification.version", "os.arch", "os.name"})
            environment.setProperty(key, System.getProperty(key));
        environment.setProperty("native.byte.order", ByteOrder.nativeOrder().toString());
        try (Writer writer = Files.newBufferedWriter(output.resolve("environment.properties"), StandardCharsets.UTF_8)) {
            environment.store(writer, "Measured reference runtime");
        }
        try (URLClassLoader loader = new URLClassLoader(
                new URL[] {Path.of(args[0]).toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Class<?> config = loader.loadClass("paulscode.sound.SoundSystemConfig");
            Class<?> logger = loader.loadClass("paulscode.sound.SoundSystemLogger");
            config.getMethod("setLogger", logger).invoke(null, logger.getConstructor().newInstance());
            config.getMethod("setStreamingBufferSize", int.class).invoke(null, Integer.parseInt(args[4]));
            config.getMethod("setFileChunkSize", int.class).invoke(null, Integer.parseInt(args[5]));
            config.getMethod("setMaxFileSize", int.class).invoke(null, Integer.parseInt(args[6]));
            type = loader.loadClass("paulscode.sound.codecs.CodecWav");
            codec = type.getConstructor().newInstance();
            boolean initialized = false;
            try {
                initialized = (Boolean) type.getMethod("initialize", URL.class).invoke(
                    codec, Path.of(args[1]).toUri().toURL());
                emit("INIT-" + initialized, null, null);
            } catch (InvocationTargetException failure) {
                emit("INIT-false", null, failure.getCause());
            }
            if (initialized) {
                if (args[3].equals("all")) {
                    call("readAll");
                } else {
                    for (int i = 0; i < 10; ++i) {
                        call("read");
                        if ((Boolean) type.getMethod("endOfStream").invoke(codec))
                            break;
                    }
                }
                call("read");
                call("readAll");
            }
            type.getMethod("cleanup").invoke(codec);
            emit("CLEANUP", null, null);
            resources(loader);
        }
    }
}
