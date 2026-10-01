# Hero3DS — Nintendo 3DS portas

Eksperimentinė tikro fheroes2 variklio integracija, orientuota į originalų Nintendo 3DS / 3DS XL / 2DS. Native `.3dsx` ir veikimas konsolėje dar nepatvirtinti. Ankstesnis `Makefile` kompiliuoja tik sintetinį hardware probe; žaidimui naudokite žemiau aprašytą CMake target'ą.

## Failų patikra

GitHub fork: https://github.com/Karolynaz/Hero3Ds

Patikrintas commit: `5affbfbba6bcc38eedbfa91cc0e4494cda2c3eb3`. Visi 963 tracked failai buvo rasti vietoje, atkūrus paslėptus projekto failus iš vidinės kopijos. Patikros įrašas: `source-audit.json`. Katalogas yra šaltinių kopija be `.git` istorijos. Darbinė kopija — projekto šaknis; vidinio `fheroes2/` pakeitimai nedaromi.

## Ekranai ir valdymas

Nuotykių žemėlapyje viršuje naudojama 400×240 sritis, apačioje — 320×240. Variklio 640×480 loginis paviršius leidžia išlaikyti esamus meniu, kovas ir miestus. Nuotykių režimu atskirai pateikiamos native ekranų sritys; standartiniai vaizdai sumažinami į ekraną.

| Valdiklis | Veiksmas |
| --- | --- |
| Circle Pad | Žymeklio judėjimas |
| D-pad | Kameros slinkimas |
| A | Pasirinkimas, kelio patvirtinimas, ataka / įėjimas pagal esamą žaidimo logiką |
| B | Atšaukimas; nuotykių žemėlapyje pašalina suplanuotą kelią |
| X | Baigti ėjimą (numatytasis E spartusis klavišas) |
| Y | Kitas herojus (numatytasis H spartusis klavišas) |
| Start | Enter / patvirtinimas |
| Select | Escape / atšaukimas |
| Lietimas | Minižemėlapio, portretų ir kitų apatinio ekrano elementų pasirinkimas |

Užvedus žymeklį ant objekto, po 1,5 s parodoma originali objekto informacija. Judėjimas arba A/B ją uždaro. D-pad ir papildomi mygtukai naudoja esamas fheroes2 klavišų nuostatas; pakeitus sparčiuosius klavišus gali keistis jų veiksmai.

## Kompiliavimas

Reikia CMake ≥3.24 ir oficialios devkitPro aplinkos su devkitARM, libctru, 3ds-zlib, 3ds-libvorbisidec, 3ds-cmake, devkitarm-cmake, 3ds-pkg-config bei 3dstools. SDL2 ir SDL2_mixer šiam native target'ui nereikia.

Oficialūs šaltiniai: [devkitPro diegimas](https://devkitpro.org/wiki/Getting_Started), [3DS CMake toolchain](https://github.com/devkitPro/pacman-packages/blob/master/cmake/3ds/3DS.cmake), [pakavimo funkcijos](https://github.com/devkitPro/pacman-packages/blob/master/cmake/3ds/Nintendo3DS.cmake).

Iš projekto šaknies:

```sh
cmake -S platform/3ds -B build/3ds \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/3ds -j4
```

Laukiami rezultatai: `build/3ds/Hero3DS.3dsx`, `build/3ds/Hero3DS.smdh`. Naudokite Homebrew Launcher; CIA diegimas šiame etape neparuoštas.

Šioje darbo aplinkoje devkitARM/libctru nėra. Bandymas pasiekti oficialų paketų serverį grąžino HTTP 403. Host kompiliatoriaus patikros su libctru API deklaracijomis tikrina C++ suderinamumą, bet ne ARM ABI, SDK linkavimą ar veikimą konsolėje.

## SD kortelė

SD kortelės šaknyje sukurkite:

```text
3ds/fheroes2/
  fheroes2.3dsx             ← sukompiliuotas Hero3DS.3dsx, pervadintas
  fheroes2.smdh             ← Hero3DS.smdh, pervadintas
  data/                    ← originalaus žaidimo DATA turinys
    HEROES2.AGG             ← būtinas
    HEROES2X.AGG            ← jei turite Price of Loyalty
  maps/                    ← originalaus žaidimo MAPS turinys
  anim/                    ← originalaus žaidimo ANIM turinys, jei yra
  music/                   ← originalaus žaidimo MUSIC turinys, jei yra
  files/
    data/
      resurrection.h2d     ← šio projekto files/data/resurrection.h2d
    save/                  ← žaidimo išsaugojimai; kuriamas automatiškai
```

Konsolėje šis kelias yra `sdmc:/3ds/fheroes2`. Konfigūracija ir spartieji klavišai saugomi toje pačioje žaidimo šaknyje. Originalūs komercinio žaidimo duomenys nepateikiami.

Galite paruošti kortelę automatiškai:

```sh
python3 platform/3ds/tools/prepare_sd.py \
  --game "/kelias/iki/Heroes II" \
  --sd "/Volumes/SD" \
  --binary "build/3ds/Hero3DS.3dsx"
```

Be `--binary` įrankis paruošia duomenis, bet nesukuria žaidimo programos. Jis atpažįsta DATA/MAPS/ANIM/MUSIC pavadinimus nepriklausomai nuo raidžių dydžio. Esamų failų neperrašo be `--overwrite`; esamų save/config failų nekopijuoja ir nekeičia. Prieš vykdydami nurodykite tikrą SD kortelės mount kelią.

## Dabartiniai apribojimai

- Įgyvendintas išorinių OGG takelių srautinis atkūrimas per Tremor/NDSP: trys 16 KiB PCM buferiai ir atskiras muzikos kanalas. Originalių MIDI takelių sintezė, MP3 ir FLAC atkūrimas nepalaikomi. Muzikos ir WAV efektų veikimą dar reikia išbandyti konsolėje; garsui reikalinga tinkama DSP aplinka. Senam 3DS bandymui galima konvertuoti muziką į 22050 Hz OGG; CPU/FPS vis tiek būtina išmatuoti.
- Meniu, kovos, miestai ir standartiniai dialogai kol kas naudoja sumažintą originalų išdėstymą. Jų įskaitomumą ir valdymą būtina tikrinti konsolėje.
- Išorinių PNG/BMP importas native backend nepalaikomas; originalūs AGG ir H2D ištekliai naudojami. Ekrano kopijos išsaugomos BMP formatu.
- RAM, FPS, originalaus 3DS suderinamumas, suspend/resume ir visi žaidimo scenarijai dar neišbandyti hardware.

Planą ir likusią hardware patikrą žr. `PLAN.md`. Iki realios patikros šio kodo nelaikykite užbaigtu, žaisti patvirtintu portu.

## Patikros ir GitHub build'as

Portable testams (kompiuteryje, be devkitPro):

```sh
python3 platform/3ds/tools/check_host.py
```

Praėjo trys C++ testų programos su AddressSanitizer/UndefinedBehaviorSanitizer ir du SD paruošimo testai. Visas native sąlyginis kodas (249 C++ failai + Smacker + zlib) sukompiliuotas ir sulinkuotas host aplinkoje naudojant laikinas libctru deklaracijas/testinius aprašus. Desktop sąlyginės pakeistų failų šakos taip pat patikrintos. Testinis host executable nėra žaidimui ar konsolėje naudojama programa.

Pridėtas `.github/workflows/3ds.yml`: įkėlus pakeitimus į fork'ą, GitHub Actions gali paleisti portable testus ir tikrą devkitARM build'ą oficialiame [devkitPro Docker atvaizde](https://github.com/devkitPro/docker/blob/master/devkitarm/Dockerfile). Sėkmės atveju workflow pateiks `Hero3DS-experimental` artefaktą su `.3dsx` ir `.smdh`. Workflow jau paleistas porto šakoje. Native konfigūracijos ir newlib endian antraštės klaidos pataisytos; galutinio ARM build’o rezultatas dar laukiamas.

GitHub darbo šaka: `port/nintendo-3ds`. Peržiūra: [draft PR #1](https://github.com/Karolynaz/Hero3Ds/pull/1). Main/master dar nepakeistas.

Jeigu jūsų MUSIC failai yra MP3 ar FLAC, juos konvertuokite į OGG išlaikydami bazinius takelių pavadinimus. Pavyzdys su įdiegtu FFmpeg: `ffmpeg -i TRACK01.mp3 -ar 22050 -c:a libvorbis TRACK01.ogg`. Originalius failus išsaugokite.

OGG dekoderio gyvavimo ciklo ir buferių testams (reikia `clang`, `clang++`, `ffmpeg` ir interneto):

```sh
python3 platform/3ds/tools/check_audio_host.py
```

Įrankis laikiname kataloge atsisiunčia konkrečių commit'ų Tremor/libogg šaltinius, sukuria sintetinį OGG takelį ir tikrina tikrą mūsų audio kodą su imituotu NDSP. Penkis kartus praėjo ASan/UBSan patikros: stop/join, atkūrimas nuo pozicijos, EOF, garsumas, mute, klaidų valymas ir maksimalus 48 KiB PCM buferis. Trečiosios šalies Tremor fixed-point shift diagnostika išjungta tik jo C šaltiniams; mūsų C++ kodui UBSan paliktas pilnas.

Jeigu konsolėje vaizdas veikia, bet nėra garso, libctru NDSP ieško konsolės DSP firmware failo SD kelyje `/3ds/dspfirm.cdc`. Žr. [oficialų libctru kodą](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c). Šis failas nepateikiamas kartu su portu.
