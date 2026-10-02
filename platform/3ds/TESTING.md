# Pirmas bandymas originaliame Nintendo 3DS / 3DS XL / 2DS

Reikalinga konsolė, galinti paleisti Homebrew Launcher, SD kortelė ir jūsų originalūs Heroes II failai. Kompiliatoriaus konsolės bandymui nereikia.

## Paruošimas

1. Iš sėkmingo `Nintendo 3DS` GitHub Actions paleidimo atsisiųskite `Hero3DS-experimental` artefaktą ir išarchyvuokite jį.
2. `Hero3DS.3dsx` pervadinkite į `fheroes2.3dsx`, `Hero3DS.smdh` — į `fheroes2.smdh`. Padėkite juos SD kortelėje į `/3ds/fheroes2/`.
3. Į `/3ds/fheroes2/data/` nukopijuokite originalaus DATA katalogo turinį, į `maps/` — MAPS, į `anim/` — ANIM ir į `music/` — MUSIC. Taip pat būtinas projekto
   `files/data/resurrection.h2d`, įdėtas į `/3ds/fheroes2/files/data/`.
4. Muzikai šiuo metu reikia OGG takelių. MP3/FLAC/MIDI nepalaikomi; failų baziniai pavadinimai po konvertavimo turi likti tie patys. Be muzikos galima atlikti
   pirmą vaizdo ir valdymo bandymą.
5. Garsui libctru NDSP ieško konsolės DSP firmware failo `/3ds/dspfirm.cdc`. Jis į šį projektą neįtrauktas.

Automatinis failų paruošimas aprašytas `README.md`; esamų išsaugojimų nekeiskite.

Azahar: paketo `3ds` katalogą padėkite į emuliatoriaus `sdmc`, o `fheroes2.3dsx` atverkite per File → Load File.
Meniu ir dialogai rodomi tik viršuje ir valdomi Circle Pad bei A/B. Lietimas veikia nuotykių žemėlapio apatiniame ekrane.

## Bandymo seka

* Homebrew Launcher paleiskite **Heroes II 3DS**. Patikrinkite, ar atsiranda meniu ir ar Circle Pad bei A/B veikia. Jei trūksta duomenų, pirmiausia patikrinkite
  `data/HEROES2.AGG` ir `files/data/resurrection.h2d` vietas.
* Pradėkite mažą single-player žemėlapį. Viršuje turi būti žemėlapis, resursai ir ėjimo pabaigos mygtukas; apačioje — minižemėlapis, portretai ir data.
* Circle Pad judinkite žymeklį, D-pad slinkite kamerą. A pasirinkite kelio tikslą, B panaikinkite kelią. Pakartotinai pasirinkę patikrinkite realų herojaus
  judėjimą.
* Užveskite žymeklį ant pastato ar priešo ir palaukite 1,5 s. Informaciją turi uždaryti judėjimas arba A/B.
* Lietimu pasirinkite kitą herojų, miestą ir minižemėlapio vietą. Po lietimo judindami Circle Pad patikrinkite, kad žymeklis grįžta į viršutinį ekraną.
* Atidarykite miestą, herojaus langą ir kovą. Jie kol kas turi sumažintą originalų išdėstymą. Patikrinkite, kad dialoguose veikia Circle Pad ir A/B, o uždarius
  grįžta dviejų ekranų žemėlapis.
* Išsaugokite žaidimą. Ekraninėje klaviatūroje įveskite pavadinimą, išeikite ir vėl paleidę įkelkite išsaugojimą.
* Baikite ėjimą ir patikrinkite dienos bei resursų pasikeitimą. Su OGG muzika patikrinkite, ar herojaus/miesto/kitos muzikos pakeitimas neužkabina žaidimo.
* Uždarykite konsolės dangtelį ir vėl atidarykite. Patikrinkite vaizdą, žymeklį bei garsą.

Tai eksperimentinis build'as. Originalaus 3DS RAM, FPS, didelių žemėlapių ir garso veikimas iki šių bandymų nėra patvirtintas. Praneškite, ties kuriuo punktu
kyla problema ir kas matoma abiejuose ekranuose; tai leis atlikti konkretų pataisymą.
