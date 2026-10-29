import java.awt.image.BufferedImage;
import java.awt.image.DataBufferInt;
import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.lang.reflect.Field;

/* Authored CPU input grids. Calls the supplied original rr without changing
 * its classes, using no original images, window, asset provider or network. */
public class RrAlphaGrid {
    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            throw new IllegalArgumentException("RrAlphaGrid <existing-private-output-directory>");
        }
        for (int type : new int[] {
                BufferedImage.TYPE_INT_ARGB, BufferedImage.TYPE_4BYTE_ABGR}) {
            rr processor = new rr();
            Field alias = rr.class.getDeclaredField("a");
            alias.setAccessible(true);
            try (DataOutputStream out = new DataOutputStream(new BufferedOutputStream(
                    new FileOutputStream(args[0] + "/type" + type + ".bin")))) {
                for (int batch = 0; batch < 128; batch++) {
                    BufferedImage input = new BufferedImage(64, 32, type);
                    for (int y = 0; y < 32; y++) {
                        for (int x = 0; x < 64; x++) {
                            input.setRGB(x, y, 0xff123456);
                        }
                    }
                    for (int slot = 0; slot < 512; slot++) {
                        int code = batch * 512 + slot;
                        int alpha = code >>> 8;
                        int channel = code & 255;
                        input.setRGB(slot % 32, slot / 32,
                                (alpha << 24) | (channel << 16)
                                | ((255 - channel) << 8) | ((channel * 73 + 19) & 255));
                    }
                    BufferedImage result = processor.a(input);
                    int[] pixels = ((DataBufferInt) result.getRaster().getDataBuffer()).getData();
                    if (result.getWidth() != 64 || result.getHeight() != 32
                            || alias.get(processor) != pixels) {
                        throw new AssertionError("rr output dimensions/array identity");
                    }
                    for (int pixel : pixels) {
                        out.writeInt(pixel);
                    }
                }
            }
            System.out.println("type=" + type
                    + " batches=128 alpha_component_pairs=65536 full_pixels=262144 sameArray=true");
        }
    }
}
