#!/usr/bin/env python3
"""Stage owned Heroes II assets on an SD card; never download game data."""
import argparse
from pathlib import Path
import shutil


def find_child(parent, name):
    if not parent.is_dir():
        return None
    return next((p for p in parent.iterdir() if p.name.casefold() == name.casefold()), None)


def inspect_assets(source):
    folders = {name: find_child(source, name) for name in ('DATA', 'MAPS', 'ANIM', 'MUSIC')}
    data = folders['DATA']
    if data is None or not data.is_dir():
        raise ValueError('Nerastas DATA katalogas originalaus žaidimo kataloge.')
    agg = find_child(data, 'HEROES2.AGG')
    if agg is None or not agg.is_file() or agg.stat().st_size == 0:
        raise ValueError('DATA kataloge nerastas netuščias HEROES2.AGG.')
    maps = folders['MAPS']
    if maps is None or not maps.is_dir() or not any(p.is_file() and p.suffix.casefold() in ('.mp2', '.mx2', '.fh2m') for p in maps.iterdir()):
        raise ValueError('MAPS kataloge nerasta žemėlapių (.MP2, .MX2, .fh2m).')
    return folders


def stage(source, sd_root, repo_root, binary=None, overwrite=False):
    folders = inspect_assets(source)
    engine_data = repo_root / 'files' / 'data' / 'resurrection.h2d'
    if not engine_data.is_file():
        raise ValueError('Projekte trūksta files/data/resurrection.h2d.')
    if binary is not None and (not binary.is_file() or binary.suffix.lower() != '.3dsx'):
        raise ValueError('Programa turi būti esamas .3dsx failas.')
    target = sd_root / '3ds' / 'fheroes2'
    copies = []
    for name, folder in folders.items():
        if folder is not None and folder.is_dir():
            copies.extend((p, target / name.lower() / p.relative_to(folder)) for p in folder.rglob('*') if p.is_file())
    copies.append((engine_data, target / 'files' / 'data' / 'resurrection.h2d'))
    if binary is not None:
        copies.append((binary, target / 'fheroes2.3dsx'))
        smdh = binary.with_suffix('.smdh')
        if smdh.is_file():
            copies.append((smdh, target / 'fheroes2.smdh'))
    # Preflight the entire copy before touching the card. Existing saves/config are
    # never part of the source list, even when --overwrite is explicitly selected.
    conflicts = [str(dst) for _, dst in copies if dst.exists()]
    if conflicts and not overwrite:
        raise ValueError('Failai jau egzistuoja; naudokite --overwrite norėdami atnaujinti žaidimo failus: ' + conflicts[0])
    for src, dst in copies:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    (target / 'files' / 'save').mkdir(parents=True, exist_ok=True)
    return target, len(copies)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path, help='Originalaus Heroes II katalogas su DATA, MAPS ir kt.')
    parser.add_argument('--sd', required=True, type=Path, help='SD kortelės šakninis katalogas')
    parser.add_argument('--binary', type=Path, help='Sukompiliuotas žaidimo .3dsx (ne probe)')
    parser.add_argument('--overwrite', action='store_true', help='Atnaujinti esamus žaidimo failus; save/config neliečiami')
    args = parser.parse_args()
    try:
        target, count = stage(args.game, args.sd, Path(__file__).resolve().parents[3], args.binary, args.overwrite)
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    print(f'Paruošta: {target} ({count} failai).')
    music = find_child(args.game, 'MUSIC')
    if music is not None and music.is_dir() and any(p.suffix.casefold() in ('.mp3', '.flac') for p in music.iterdir() if p.is_file()):
        print('Pastaba: 3DS palaiko OGG muziką; MP3/FLAC takelius reikia konvertuoti į OGG, išlaikant jų pavadinimus.')
    if args.binary is None:
        print('Programa nenukopijuota: dar reikės sukompiliuoto fheroes2.3dsx.')


if __name__ == '__main__':
    main()
