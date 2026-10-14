# New UI elements

Sugeneruota integruotu imagegen įrankiu. Tikslūs generavimo aprašai: prompts.json. Pilno dydžio šaltiniai source/, paruošti žaidimui PNG šiame kataloge.

* panel-top.png: 380×220, dialogo rėmelis viršuje. 400×240 ekrane lieka po10px kraštuose.
* panel-bottom.png: 320×240, apatinio ekrano fonas.
* button-default.png, button-focused.png, button-pressed.png, button-disabled.png: 144×40. Žaidimas pritaiko plotį ir aukštį pagal mygtukų išdėstymą; užrašai piešiami atskirai normalfontu.

PNG galite keisti. Kad pakeitimai patektų į kitą buildą, su Pillow paleiskite platform/3ds/tools/pack_ui_assets.py iš projekto. Skriptas naudoja vietinį ui-assets/palette.pal ir atnaujina src/engine/ui_3ds_assets.h. Šio buildo grafika įterpta į .3dsx — SD papildomų UI failų kopijuoti nereikia.
