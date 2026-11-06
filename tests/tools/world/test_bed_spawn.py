"""Run native Bed spawn controls and the sealed unchanged-original comparison."""
import importlib.util
from pathlib import Path


def main():
    source = Path(__file__).resolve().parents[3] / 'tests/reference/world/bed_spawn/run_native.py'
    spec = importlib.util.spec_from_file_location('bed_spawn_native_comparison', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main()


if __name__ == '__main__':
    main()
