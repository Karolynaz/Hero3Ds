# Hero3DS įgyvendinimo planas

Tikslas: Heroes II nuotykių žemėlapį pritaikyti dviem Nintendo 3DS ekranams, išlaikant originalaus fheroes2 žaidimo logiką.

1. **Šaltinių patikra.** Palyginti vietinius failus su Karolynaz/Hero3Ds fork'o GitHub medžiu. Atkurti paslėptus konfigūracijos failus. Pagrindinė darbo kopija yra šio katalogo šaknyje; vidinis `fheroes2/` yra ankstesnio snapshot'o atsarginė kopija.
2. **Platformos agentas.** Prijungti libctru inicializavimą, programos gyvavimo ciklą, paletinio vaizdo konvertavimą į abiejų ekranų framebuffer'ius, SD kelius ir native CMake target'ą. Atskirti SDL priklausomybes; pridėti WAV efektus per NDSP.
3. **Valdymo agentas.** Circle Pad valdo žymeklį; D-pad slenka kamerą; A patvirtina; B atšaukia. Išsaugoti paspaudimo ir atleidimo įvykius. Apatiniame ekrane palaikyti tiesioginį lietimą.
4. **Sąsajos agentas.** Viršuje native 400×240 nuotykių žemėlapis, resursai ir ėjimo pabaiga. Apačioje 320×240 minižemėlapis, herojai, miestai ir data. Automatinė objekto informacija po 1,5 s. Standartiniams dialogams laikinai įjungti viso vaizdo pateikimą.
5. **Integracija.** Vienas `TARGET_NINTENDO_3DS` platformos makro. Patikrinti visą native kodo kompiliavimą/linkavimą, desktop sąlygines šakas ir portable valdymo bei framebuffer testus. Paruošti SD kopijavimo įrankį ir instrukcijas.
6. **Native patikra.** Kompiliuoti devkitARM + libctru + zlib aplinkoje. Įkelti `.3dsx` į Homebrew Launcher. Su naudotojo originaliais duomenimis patikrinti meniu, žemėlapį, kovas, miestus, kelią, informaciją, ėjimų keitimą, išsaugojimą, konsolės uždarymą ir atnaujinimą.
7. **Suderinamumas ir užbaigimas.** Išmatuoti RAM/FPS tiek originaliame, tiek New 3DS. Pridėti muzikos dekoderį, pagerinti dialogų įskaitomumą ir bet kokias hardware patikroje rastas problemas. Tik tada žymėti portą kaip tinkamą žaisti.

Kodas ir portable testai neatstoja 6–7 punktų. Sintetinis hardware probe taip pat neatstoja tikro žaidimo.
