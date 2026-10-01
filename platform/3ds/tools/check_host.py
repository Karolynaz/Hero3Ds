#!/usr/bin/env python3
"""Run portable 3DS tests. This does not cross-compile the game."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
compiler = os.environ.get('CXX', 'c++')
tests = ('platform/3ds/tests/framebuffer.cpp', 'tests/3ds/input_test.cpp', 'platform/3ds/tests/viewport.cpp')
with tempfile.TemporaryDirectory(prefix='hero3ds-tests-') as temp:
    for index, source in enumerate(tests):
        output = str(Path(temp) / f'test-{index}')
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I' + str(root / 'src/engine'), str(root / source), '-o', output], check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        subprocess.run([output], env=environment, check=True)
        print('PASS:', source, flush=True)
subprocess.run(['python3', '-m', 'unittest', 'discover', '-s', str(root / 'platform/3ds/tests'), '-p', 'test_*.py'], check=True)
