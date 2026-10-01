# Hero3DS tęstinumas

Darbinė projekto kopija yra šio katalogo šaknyje. Vidinis `fheroes2/` yra senas snapshot'as, paliktas palyginimui. `.git` istorijos vietoje nėra. Pakeitimai siunčiami į `Karolynaz/Hero3Ds` šaką `port/nintendo-3ds`; draft PR: https://github.com/Karolynaz/Hero3Ds/pull/1.

Pagrindinės instrukcijos: [platform/3ds/README.md](platform/3ds/README.md).
Planavimas ir agentų darbai: [platform/3ds/PLAN.md](platform/3ds/PLAN.md).

Dabar integruojamas tikras fheroes2 variklis: native libctru vaizdas, valdymas, dual-screen nuotykių sąsaja, SD keliai ir NDSP WAV efektai. `platform/3ds/Makefile` ir `3ds-initial.patch` yra ankstesnio sintetinio bandymo artefaktai; naujam žaidimui naudokite `platform/3ds/CMakeLists.txt`. Seno patch'o nebetaikykite.

Visi fork'o 963 tracked failai patikrinti pagal GitHub commit `5affbfbba6bcc38eedbfa91cc0e4494cda2c3eb3`. Projekto pakeitimai dar nepatikrinti devkitARM ir tikroje konsolėje. Išorinių OGG takelių srautinis atkūrimas įgyvendintas, bet dar laukia ARM/hardware patikros; MIDI/MP3/FLAC nepalaikomi.

Kitas būtinas etapas: native cross-build, tada Homebrew Launcher patikra su originaliais žaidimo duomenimis pagal README. Host patikros ir hardware probe nėra įrodymas, kad visas žaidimas veikia.
