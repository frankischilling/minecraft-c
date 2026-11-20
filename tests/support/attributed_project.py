"""Small real source trees for tests whose JDK and child processes are mocked."""
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch


class AttributedProject:
    """Keep source hashing real without repeatedly inventorying the checkout.

    The driver and its declared inputs retain real bytes and canonical relative
    paths. Already loaded helpers remain attributed at their actual paths. This
    fixture applies only inside mocked execution tests; production collectors,
    source-binding controls and native replays still inventory the real project.
    """

    def __init__(self, module, directory, required):
        self.module = module
        self.root = Path(directory).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        source = Path(module.ROOT).resolve()
        self.driver = Path(module.__file__).resolve().relative_to(source)
        for relative in {self.driver, Path("CMakeLists.txt"), *map(Path, required)}:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((source / relative).read_bytes())
        self.native_inputs = set()
        for relative in ("src/guard.c", "src/guard.h", "include/guard.h"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"/* Authored source input for execution guards. */\n")
            if relative != "src/guard.h":
                self.native_inputs.add(str(path))
        self.stack = None

    def __enter__(self):
        if self.stack is not None:
            raise RuntimeError("attributed project already entered")
        stack = ExitStack()
        try:
            stack.enter_context(patch.object(self.module, "ROOT", self.root))
            stack.enter_context(patch.object(self.module, "__file__", str(self.root / self.driver)))
        except BaseException:
            stack.close()
            raise
        self.stack = stack
        return self

    def __exit__(self, *exception):
        stack, self.stack = self.stack, None
        return stack.__exit__(*exception)

    def change_source(self):
        """A real same-length change must be found by the final hash audit."""
        path = self.root / "src/guard.c"
        before = path.read_bytes()
        path.write_bytes(bytes([before[0] ^ 1]) + before[1:])
