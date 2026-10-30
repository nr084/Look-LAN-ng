"""Local ARM64 build with mandatory stable compiler / IEEE preflight."""
import subprocess,sys
from pathlib import Path
subprocess.run([sys.executable,str(Path(__file__).with_name('build-current.py')),'--arch','arm64'],check=True)
