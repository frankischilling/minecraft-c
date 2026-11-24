"""Run the actual Death screen component and original-observation comparison."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[2] / 'reference/client/death_screen/run_native.py'),
                   run_name='__main__')
