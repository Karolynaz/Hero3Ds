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

# SPDX-License-Identifier: GPL-2.0-or-later
"""Export local Heroes II AGG and H2D graphics to a categorized PNG library.

ICN/TIL/H2D decoding follows src/engine/image_tool.cpp and h2d_file.cpp.
No third-party modules are required. Original assets stay local.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib


def png(path, width, height, pixels, channels=4):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + pixels[y * width * channels:(y + 1) * width * channels] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6 if channels == 4 else 0, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def agg_entries(data):
    count, = struct.unpack_from('<H', data)
    names = len(data) - count * 15
    for i in range(count):
        _, offset, size = struct.unpack_from('<III', data, 2 + i * 12)
        name = data[names + i * 15:names + (i + 1) * 15].split(b'\0')[0].decode('ascii').lower()
        assert offset + size <= names, name
        yield name, data[offset:offset + size]


def decode_icn(data):
    count, total = struct.unpack_from('<HI', data)
    headers = [struct.unpack_from('<hhHHBI', data, 6 + i * 13) for i in range(count)]
    for i, (ox, oy, width, height, animation, offset) in enumerate(headers):
        end = headers[i + 1][-1] if i + 1 < count else total
        stream = data[6 + offset:6 + end]
        pixels, mask = bytearray(width * height), bytearray([1]) * (width * height)
        x = y = pos = 0
        def put(values, transform=0):
            nonlocal x
            n = len(values)
            assert y < height and x + n <= width, (i, width, height, x, y, n)
            start = y * width + x
            pixels[start:start + n] = values
            mask[start:start + n] = bytes([transform]) * n
            x += n
        while pos < len(stream):
            op = stream[pos]; pos += 1
            if op == 0:
                y += 1; x = 0
            elif op == 128:
                break
            elif animation & 32:
                if op < 128:
                    put(bytes(op))
                else:
                    x += op - 128
            elif op < 128:
                put(stream[pos:pos + op]); pos += op
            elif op < 192:
                x += op - 128
            elif op == 192:
                value = stream[pos]; pos += 1
                n = value & 3
                if not n:
                    n = stream[pos]; pos += 1
                transform = ((value & 60) >> 2) + 2
                if value & 64 and transform < 16:
                    put(bytes(n), transform)
                else:
                    x += n
            else:
                n = op - 192
                if op == 193:
                    n = stream[pos]; pos += 1
                color = stream[pos]; pos += 1
                put(bytes([color]) * n)
        yield i, width, height, ox, oy, animation, pixels, mask


def decode_til(data):
    count, width, height = struct.unpack_from('<HHH', data)
    size = width * height
    assert len(data) == 6 + count * size
    for i in range(count):
        yield i, width, height, 0, 0, 0, data[6 + i * size:6 + (i + 1) * size], bytes(size)


def h2d_entries(data):
    assert data[:4] == b'H2D\x02'
    count, = struct.unpack_from('<I', data, 4)
    pos = 8
    for _ in range(count):
        offset, size, length = struct.unpack_from('<III', data, pos); pos += 12
        name = data[pos:pos + length].decode(); pos += length
        raw = zlib.decompress(data[offset:offset + size])
        width, height, ox, oy, single = struct.unpack_from('<iiiiB', raw)
        n = width * height
        assert len(raw) == 17 + n * (1 if single else 2), name
        yield name, [(0, width, height, ox, oy, 0, raw[17:17 + n], bytes(n) if single else raw[17 + n:])]


def category(name):
    s = Path(name).stem.upper()
    races = {'B': 'barbarian', 'K': 'knight', 'S': 'sorceress', 'W': 'warlock', 'Z': 'wizard', 'N': 'necromancer'}
    if name.lower().endswith('.til'):
        return 'map/tiles'
    if name.lower().endswith('.image'):
        if '_CASTLE_' in s or 'WIZARD_BAY' in s:
            race = s.split('_')[0].lower()
            return 'cities/' + race
        if s.startswith(('MAIN_MENU', 'GRAPHICS_ICON', 'HOTKEYS_ICON', 'RESOLUTION_ICON')):
            return 'menus'
        if s.startswith(('KEYBOARD_', 'TOGGLE-')):
            return 'dialogs'
        if s.startswith(('PETRIFICATION_', 'MASS-DISPEL', 'BOOK_CORNER')):
            return 'spells/interface'
        if s.startswith('SWORDSMAN'):
            return 'battle/units'
        if s == 'MONOCLE':
            return 'resources_artifacts'
        if s.startswith(('TOWNBKG', 'TWN')):
            return 'cities/interface'
        return 'map/objects'
    if s.startswith('TWN') and len(s) > 3 and s[3] in races:
        return 'cities/' + races[s[3]]
    if s.startswith(('TOWN', 'CAST', 'CASL', 'CSTL', 'MAGEGLD', 'BUYBUIL', 'BUILD', 'BLDG', 'WELL', 'TAV', 'RECR', 'TRAD')):
        return 'cities/interface'
    if s.startswith(('PORT', 'HERO', 'MINIHERO', 'MINIPORT', 'QWIKHERO', 'SWAP', 'PRIMSKIL', 'SECSKILL', 'MOBILITY', 'EXPMRL')):
        return 'heroes'
    if s.startswith(('FONT', 'SMALFONT', 'LETTER', 'TEXT')):
        return 'fonts'
    if s in {'MORALEG', 'MORALEB', 'CLOUDLUK', 'MINILKMR'}:
        return 'heroes'
    if s.startswith(('ART', 'RESOURCE', 'RESSMALL', 'TREASURY')):
        return 'resources_artifacts'
    if s.startswith(('OBJ', 'MTN', 'TRE', 'GROUND', 'ROAD', 'STREAM', 'BOAT', 'ROUTE', 'SHADOW32', 'FROTH', 'COBJ', 'COVR', 'FRNG', 'CLO', 'TERRAINS', 'FLAG', 'VWFLAG')) or '-FLAG' in s or '-BFLG' in s:
        return 'map/tiles' if name.lower().endswith('.til') else 'map/objects'
    if s.startswith(('ADV', 'RADAR', 'QWIK', 'SUNMOON', 'HOURGLAS', 'LOCATOR', 'APAN', 'SPANEL', 'SPANB', 'CSPAN', 'ESPAN', 'OVERVIEW', 'OVERBACK', 'VIEW', 'EVIW', 'LGND', 'MINITOWN', 'MINICAPT', 'MINISS')):
        return 'map/interface'
    if s.startswith(('CMBT', 'CBKG', 'CPAN', 'WINCMB', 'SURDR', 'SURREND', 'CATAPULT', 'BOULDER', 'MOAT', 'CFLG')):
        return 'battle/interface'
    if s.startswith(('BTN', 'NG', 'HS', 'NET', 'SCEN', 'CAMP', 'CTRACK', 'CONGRAT', 'WINLOSE', 'BROTHERS')):
        return 'menus'
    if s.startswith(('REQ', 'SYSTEM', 'SCROLL', 'STONE', 'STON', 'RED', 'LIST', 'DROPL', 'CELL', 'BIGBAR', 'SMALLBAR', 'STRIP', 'CREST', 'BRCREST')):
        return 'dialogs'
    if s.startswith(('EDITOR', 'EDIT', 'BORD', 'ESCROLL', 'OVERLAY', 'ECPAN')):
        return 'editor'
    if s in {'SPELLS', 'SPELLINF', 'SPELLINL', 'BOOK', 'MANA'}:
        return 'spells/interface'
    if s.startswith('MONH') or s in {'MONS32', 'MINIMON', 'MINILKMR'}:
        return 'battle/portraits'
    if s.endswith('32'):
        return 'map/heroes'
    if s in {'PUZZLE', 'REDBACK'}:
        return 'menus'
    units = {'AELEM', 'ARCHER', 'ARCHER2', 'BOAR', 'CAVALRYB', 'CAVALRYR', 'CENTAUR', 'CYCLOPS', 'DRAGBLAK', 'DRAGBONE', 'DRAGGREE', 'DRAGRED', 'DRUID', 'DRUID2', 'DWARF', 'DWARF2', 'EELEM', 'ELF', 'ELF2', 'FELEM', 'GARGOYLE', 'GENIE', 'GHOST', 'GOBLIN', 'GOLEM', 'GOLEM2', 'GRIFFIN', 'HALFLING', 'HYDRA', 'LICH', 'LICH2', 'MAGE1', 'MAGE2', 'MEDUSA', 'MINOTAUR', 'MINOTAU2', 'MUMMYW', 'MUMMY2', 'NOMAD', 'OGRE', 'OGRE2', 'ORC', 'ORC2', 'PALADIN', 'PALADIN2', 'PEASANT', 'PHOENIX', 'PIKEMAN', 'PIKEMAN2', 'ROC', 'ROGUE', 'SKELETON', 'SPRITE', 'SWORDSMN', 'SWORDSM2', 'TITANBLA', 'TITANBLU', 'TROLL', 'TROLL2', 'UNICORN', 'VAMPIRE', 'VAMPIRE2', 'WELEM', 'WOLF', 'ZOMBIE', 'ZOMBIE2'}
    return 'battle/units' if s in units else 'battle/effects'


def export(archive, output, h2d):
    entries = dict(agg_entries(archive.read_bytes()))
    pal = entries['kb.pal']
    assert len(pal) == 768
    colors = [bytes([v << 2 for v in pal[i * 3:i * 3 + 3]]) + b'\xff' for i in range(256)]
    output.mkdir(parents=True, exist_ok=True)
    (output / 'palette.pal').write_bytes(pal)
    bundles, errors = [], []
    sources = [{'name': archive.name, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}]
    graphics = [(name, 'heroes2.agg', decode_icn(raw) if name.endswith('.icn') else decode_til(raw))
                for name, raw in sorted(entries.items()) if name.endswith(('.icn', '.til'))]
    if h2d:
        sources.append({'name': h2d.name, 'sha256': hashlib.sha256(h2d.read_bytes()).hexdigest()})
        graphics += [(name, 'resurrection.h2d', frames) for name, frames in h2d_entries(h2d.read_bytes())]
    for name, source, frames in graphics:
        cat = category(name)
        stem = re.sub(r'[^a-z0-9_-]', '_', Path(name).stem.lower())
        folder = output / cat / (stem if source == 'heroes2.agg' else 'h2d_' + stem)
        folder.mkdir(parents=True, exist_ok=True)
        metadata = {'source': source, 'asset': name, 'category': cat, 'frames': []}
        try:
            for index, w, h, ox, oy, animation, pixels, mask in frames:
                if w <= 0 or h <= 0:
                    errors.append({'asset': name, 'frame': index, 'reason': 'empty dimensions'}); continue
                rgba = b''.join(colors[p] if m == 0 else b'\0\0\0\0' if m == 1 else b'\0\0\0\x60' for p, m in zip(pixels, mask))
                filename = f'{index:04d}.png'
                png(folder / filename, w, h, rgba)
                item = {'index': index, 'file': filename, 'width': w, 'height': h, 'offset_x': ox, 'offset_y': oy, 'animation_flags': animation}
                if any(m > 1 for m in mask):
                    item['transform_mask'] = f'{index:04d}.transform.png'
                    png(folder / item['transform_mask'], w, h, bytes(mask), 1)
                metadata['frames'].append(item)
        except Exception as e:
            errors.append({'asset': name, 'reason': str(e)})
        (folder / 'metadata.json').write_text(json.dumps(metadata, indent=2))
        if metadata['frames']:
            bundles.append({'asset': name, 'source': source, 'category': cat, 'folder': folder.relative_to(output).as_posix(), 'frames': len(metadata['frames']), 'preview': metadata['frames'][0]['file']})
        if len(bundles) % 100 == 0:
            print(f'Exported {len(bundles)} sprite sets', flush=True)
    manifest = {'sources': sources, 'bundles': bundles, 'errors': errors, 'total_frames': sum(b['frames'] for b in bundles)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    gallery(output, bundles)
    write_readme(output, bundles)
    print(json.dumps({'bundles': len(bundles), 'frames': manifest['total_frames'], 'errors': errors}, indent=2), flush=True)


def write_readme(output, bundles):
    counts = Counter()
    for b in bundles:
        counts[b['category']] += b['frames']
    text = '# Heroes II UI ir grafikos biblioteka\n\nAtverkite `index.html`: kategorijų filtras, paieška, kadrų peržiūra.\n\n'
    text += '\n'.join(f'* `{cat}`: {count} PNG kadrų.' for cat, count in sorted(counts.items()))
    text += '\n\nKiekviename rinkinyje yra numeruoti PNG ir `metadata.json` su originaliais poslinkiais.\n'
    text += 'PNG išsaugo originalų dydį ir skaidrumą. Šešėliai / transformacijos peržiūroje apytiksliai tamsūs; tikslūs transformacijų kodai išsaugoti atskiruose `.transform.png` failuose (pilkumo reikšmė 0–15).\n'
    text += 'Tai šaltinių grafikos elementai, ne sukomponuotų žaidimo langų ekrano nuotraukos. fheroes2 vykdymo metu generuojami ar taisomi elementai gali skirtis.\n'
    text += 'Originalaus žaidimo grafika laikoma tik lokaliai; katalogas neįtraukiamas į Git. Šaltinių kontrolinės sumos ir eksporto klaidos: `manifest.json`.\n'
    (output / 'README.md').write_text(text)


def gallery(output, bundles):
    data = json.dumps(bundles).replace('</', '<\\/')
    page = '''<!doctype html><html lang="lt"><meta charset="utf-8"><title>Heroes II UI assets</title>
<style>body{background:#171b23;color:#eee;font:16px system-ui;margin:24px}header{position:sticky;top:0;background:#171b23;padding:12px 0}input,select,button{padding:10px;margin:4px;background:#303746;color:white;border:1px solid #667;border-radius:6px}#sets{display:grid;grid-template-columns:repeat(auto-fill,minmax(210px,1fr));gap:14px}.set{background:#252c38;border-radius:8px;padding:12px;cursor:pointer}.set img{height:120px;width:100%;object-fit:contain;image-rendering:pixelated;background:repeating-conic-gradient(#444 0% 25%,#555 0% 50%) 0/16px 16px}small{display:block;color:#abb}dialog{background:#171b23;color:white;width:90vw;height:85vh;border:1px solid #777}#frames{display:flex;flex-wrap:wrap;gap:16px}figure{margin:0;padding:8px;background:#303746}figure img{max-width:640px;max-height:480px;image-rendering:pixelated;background:repeating-conic-gradient(#444 0% 25%,#555 0% 50%) 0/16px 16px}a{color:#bcd}</style>
<header><h1>Heroes II — UI ir grafika</h1><input id="search" placeholder="Ieškoti rinkinio..."><select id="category"><option value="">Visos kategorijos</option></select><span id="count"></span></header><main id="sets"></main>
<dialog id="detail"><button id="close">Uždaryti</button><h2 id="title"></h2><a id="meta">metadata.json</a><div id="frames"></div></dialog><script>
const bundles=DATA, search=document.querySelector('#search'),cat=document.querySelector('#category'),sets=document.querySelector('#sets');
[...new Set(bundles.map(b=>b.category))].sort().forEach(c=>{let o=document.createElement('option');o.value=o.textContent=c;cat.append(o)});
function render(){sets.replaceChildren();let filtered=bundles.filter(b=>(!cat.value||b.category===cat.value)&&(`${b.asset} ${b.category}`.toLowerCase().includes(search.value.toLowerCase())));document.querySelector('#count').textContent=`${filtered.length} rinkiniai`;filtered.forEach(b=>{let card=document.createElement('article');card.className='set';let img=document.createElement('img');img.loading='lazy';img.src=`${b.folder}/${b.preview}`;let label=document.createElement('strong');label.textContent=b.asset;let info=document.createElement('small');info.textContent=`${b.category} · ${b.frames} kadrai`;card.append(img,label,info);card.onclick=()=>show(b);sets.append(card)})}
function show(b){document.querySelector('#title').textContent=b.asset;document.querySelector('#meta').href=`${b.folder}/metadata.json`;let frames=document.querySelector('#frames');frames.replaceChildren();for(let i=0;i<b.frames;i++){let f=document.createElement('figure'),im=document.createElement('img'),caption=document.createElement('figcaption');im.loading='lazy';im.src=`${b.folder}/${String(i).padStart(4,'0')}.png`;caption.textContent=String(i).padStart(4,'0');f.append(im,caption);frames.append(f)}document.querySelector('#detail').showModal()}
document.querySelector('#close').onclick=()=>document.querySelector('#detail').close();search.oninput=cat.onchange=render;render();</script></html>'''
    (output / 'index.html').write_text(page.replace('DATA', data))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agg', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--h2d', type=Path)
    args = parser.parse_args()
    export(args.agg, args.output, args.h2d)
