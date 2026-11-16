import java.io.*;
import java.util.Arrays;

public final class FileLongPath {
    public static void main(String[] args) throws Exception {
        char[] units = new char[args[0].length() / 4];
        for (int i = 0; i < units.length; ++i)
            units[i] = (char)Integer.parseInt(args[0].substring(i * 4, i * 4 + 4), 16);
        File root = new File(new String(units));
        if (root.exists()) throw new AssertionError("fresh root");
        if (args.length == 2 && args[1].equals("absent-alias")) {
            System.out.println("PASS: missing File.exists returns false");
            return;
        }
        if (!root.mkdirs() || !root.exists() || !root.isDirectory())
            throw new AssertionError("directory creation");
        File file = new File(root, "data.bin");
        byte[] data = new byte[] {0, 1, 2, -1};
        try (FileOutputStream out = new FileOutputStream(file)) { out.write(data); }
        if (!file.isFile() || file.length() != data.length)
            throw new AssertionError("file attributes");
        try (FileInputStream in = new FileInputStream(file)) {
            if (!Arrays.equals(in.readAllBytes(), data)) throw new AssertionError("contents");
        }
        File renamed = new File(root, "renamed.bin");
        if (!file.renameTo(renamed) || file.exists() || !renamed.isFile())
            throw new AssertionError("rename");
        if (!renamed.delete() || renamed.exists()) throw new AssertionError("delete");
        String canonical = root.getCanonicalPath();
        if (canonical.startsWith("\\\\?\\") || !new File(canonical).equals(root.getAbsoluteFile()))
            throw new AssertionError("canonical path identity");
        System.out.println("PASS: mkdirs, attributes, streams, rename, delete, canonical identity");
    }
}
