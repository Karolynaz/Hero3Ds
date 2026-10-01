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
