#include <doctest.h>

#include "UI/HUD/PadButtonNames.h"

using UI::HUD::PadButtonNames::Fill;
using UI::HUD::PadButtonNames::Names;

namespace
{
Names PlayStation()
{
    Names names;
    names.a = L"Cross";
    names.b = L"Circle";
    names.x = L"Square";
    names.y = L"Triangle";
    names.lb = L"L1";
    names.rb = L"R1";
    names.lt = L"L2";
    names.rt = L"R2";
    names.ls = L"L3";
    names.rs = L"R3";
    names.menu = L"Options";
    names.view = L"Create";
    return names;
}
} // namespace

TEST_CASE("pad button names: the Xbox letters by default [ui][gamepad]")
{
    CHECK(Fill(L"{X}, {Y}, {B}: skills. Hold {RT}: {X} casts 4", Names{}) == L"X, Y, B: skills. Hold RT: X casts 4");
    CHECK(Fill(L"{LS} and {RS}, {MENU}, {VIEW}", Names{}) == L"LS and RS, Menu, View");
}

TEST_CASE("pad button names: the pad in hand's names go in [ui][gamepad]")
{
    CHECK(Fill(L"{A}: attack, {LB}/{RB}: cursor, {MENU}: Enter", PlayStation()) ==
          L"Cross: attack, L1/R1: cursor, Options: Enter");
    CHECK(Fill(L"{A}{B}", PlayStation()) == L"CrossCircle");
}

TEST_CASE("pad button names: the sticks by their job [ui][gamepad]")
{
    CHECK(Fill(L"{WALKSTICK}: walk. {AIMSTICK}: pick", Names{}) == L"Left stick: walk. Right stick: pick");
    CHECK(Fill(L"let go of the {WALKSTICK_OBJ}, the {AIMSTICK_OBJ} picks", Names{}) ==
          L"let go of the left stick, the right stick picks");
    Names leftHanded;
    leftHanded.walkStick = L"Right stick";
    leftHanded.walkStickObject = L"right stick";
    CHECK(Fill(L"{WALKSTICK}, the {WALKSTICK_OBJ}", leftHanded) == L"Right stick, the right stick");
}

TEST_CASE("pad button names: other braces and text stay as they are [ui][gamepad]")
{
    CHECK(Fill(L"", Names{}) == L"");
    CHECK(Fill(L"no buttons here", Names{}) == L"no buttons here");
    CHECK(Fill(L"{Q} {a} {} {A", Names{}) == L"{Q} {a} {} {A");
    CHECK(Fill(L"{{A}}", Names{}) == L"{A}");
    CHECK(Fill(L"end {A}", Names{}) == L"end A");
}
