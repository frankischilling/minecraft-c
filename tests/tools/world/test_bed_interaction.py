"""Run bounded native World Bed interaction controls and the original projection."""
import importlib.util
from pathlib import Path


def main():
    source = (Path(__file__).resolve().parents[3] /
              'tests/reference/world/bed_interaction/run_native.py')
    spec = importlib.util.spec_from_file_location('bed_interaction_native', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main()


if __name__ == '__main__':
    main()
