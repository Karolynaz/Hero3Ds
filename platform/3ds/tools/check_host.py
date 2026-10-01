#!/usr/bin/env python3

###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2026                                                    #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

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
