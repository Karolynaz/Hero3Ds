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

import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('prepare_sd', Path(__file__).resolve().parents[1] / 'tools' / 'prepare_sd.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PrepareSDTests(unittest.TestCase):
    def test_missing_data_does_not_touch_card(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaises(ValueError):
                module.stage(root / 'missing', root / 'sd', root)
            self.assertFalse((root / 'sd').exists())

    def test_case_insensitive_assets_and_save_preservation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            game, repo, sd = root / 'game', root / 'repo', root / 'sd'
            (game / 'DaTa').mkdir(parents=True)
            (game / 'DaTa' / 'HEROES2.AGG').write_bytes(b'owned-test-placeholder')
            (game / 'MAPS').mkdir()
            (game / 'MAPS' / 'MAP.MP2').write_bytes(b'map')
            (repo / 'files' / 'data').mkdir(parents=True)
            (repo / 'files' / 'data' / 'resurrection.h2d').write_bytes(b'engine')
            target, count = module.stage(game, sd, repo)
            self.assertEqual(count, 3)
            self.assertTrue((target / 'data' / 'HEROES2.AGG').is_file())
            saved = target / 'files' / 'save' / 'my.sav'
            saved.write_bytes(b'keep')
            with self.assertRaises(ValueError):
                module.stage(game, sd, repo)
            module.stage(game, sd, repo, overwrite=True)
            self.assertEqual(saved.read_bytes(), b'keep')


if __name__ == '__main__':
    unittest.main()
