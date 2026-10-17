# Build and tests

Use a C17 compiler, CMake3.24+, and Python3.10+ for tests. The initial baseline is
x86-64 native Linux and Windows. The shared core has no display, graphics,
audio, Java, or asset dependency. An optional JDK comparison tests numerical
primitives and does not establish gameplay parity.

Use the commands and environment notes in the
[README](https://github.com/frankischilling/minecraft-c#build-and-test) and
[testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md).
Code follows the repository `.clang-format` and system/subsystem folder layout.
The [resource guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/resources.md)
describes local import; original resources are never bundled.

There is currently no C game executable, supported save writer, or C server to
host. Client controls, original-compatible save workflows, and Linux/Windows
hosting/backup/shutdown instructions will be added with verified implementations.
