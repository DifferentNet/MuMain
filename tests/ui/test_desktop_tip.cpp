#include <doctest.h>

#include "UI/Mobile/DesktopTip.h"

using UI::Mobile::DesktopTip::NeedsMouseOrKeys;

TEST_CASE("tips that name the mouse or a key are found [ui][desktop-tip]")
{
    CHECK(NeedsMouseOrKeys(L"To use skills or magic, right click on monster."));
    CHECK(NeedsMouseOrKeys(L"Press Q, W, E to use the items registered under those keys."));
    CHECK(NeedsMouseOrKeys(L"Point the cursor on the skill button and press ctrl+number key to set hotkey."));
    CHECK(NeedsMouseOrKeys(L"To use the saved chatting macro, press Alt+number."));
    CHECK(NeedsMouseOrKeys(L"Press space bar to collect loot."));
    CHECK(NeedsMouseOrKeys(L"Click shortcut key T to check the quest information."));
    CHECK(NeedsMouseOrKeys(L"The supplementary magic of the Elf can be used by clicking the name in the party window."));
}

TEST_CASE("Portuguese and Spanish mouse and key tips are found [ui][desktop-tip]")
{
    CHECK(NeedsMouseOrKeys(L"Clique na tecla de atalho T para verificar as informa\u00e7\u00f5es da miss\u00e3o."));
    CHECK(NeedsMouseOrKeys(L"Para usar habilidades ou m\u00e1gica, clique com o bot\u00e3o direito do mouse no monstro."));
    CHECK(NeedsMouseOrKeys(L"Pressione a barra de espa\u00e7o para coletar o saque."));
    CHECK(NeedsMouseOrKeys(L"Para usar habilidades o magia, haz clic derecho sobre el monstruo."));
    CHECK(NeedsMouseOrKeys(L"Presiona Q, W, E, R para usar \u00edtems."));
    CHECK(NeedsMouseOrKeys(L"La magia suplementaria de la Elfa puede utilizarse al hacer clic en la ventana."));
    CHECK_FALSE(NeedsMouseOrKeys(L"Voc\u00ea recebe 5 pontos de atributos a cada vez que aumenta um n\u00edvel."));
    CHECK_FALSE(NeedsMouseOrKeys(L"Debes tener un nivel de al menos 15 para entrar en Devias"));
    CHECK_FALSE(NeedsMouseOrKeys(L"Utiliza la funci\u00f3n de bloqueo en tu b\u00f3veda para protegerla."));
}

TEST_CASE("tips that fit any controls stay [ui][desktop-tip]")
{
    CHECK_FALSE(NeedsMouseOrKeys(L"You must be at least level 15 to enter Devias"));
    CHECK_FALSE(NeedsMouseOrKeys(L"5 stat points are rewarded each time you level up."));
    CHECK_FALSE(NeedsMouseOrKeys(L"To send a message to guild members, type '@ message'"));
    CHECK_FALSE(NeedsMouseOrKeys(L"Excellent type item names are displayed in green."));
    CHECK_FALSE(NeedsMouseOrKeys(L""));
}
