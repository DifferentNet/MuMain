#include "Network/Server/HiddenChatLines.h"
#include "UI/Core/WindowAccess.h"

#include "doctest.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
std::vector<std::wstring> Calls;
std::vector<mu::ui::window::INTERFACE_LIST> Shown;
bool WindowVisible = false;
bool OpenRequested = false;

void Clear()
{
    Calls.clear();
    Shown.clear();
    WindowVisible = false;
    OpenRequested = false;
}

// Registered out of their prefixes' order, as the link order may put them.
[[maybe_unused]] const bool LastByPrefix = Network::HiddenChatLines::Register({
    .name = L"Zzz",
    .prefixes = {L"#ZZZ|"},
    .take =
        [](const std::wstring&)
    {
        Calls.push_back(L"zzz");
        return true;
    },
    .enteredWorld = [] { Calls.push_back(L"enter zzz"); },
});

[[maybe_unused]] const bool Short = Network::HiddenChatLines::Register({
    .name = L"Short",
    .prefixes = {L"#TST|"},
    .take =
        [](const std::wstring&)
    {
        Calls.push_back(L"short");
        return true;
    },
    .enteredWorld = [] { Calls.push_back(L"enter short"); },
});

// Shares the start of Short's prefix; declines one line, which then goes on to Short.
[[maybe_unused]] const bool Long = Network::HiddenChatLines::Register({
    .name = L"Long",
    .prefixes = {L"#TST|X|"},
    .take =
        [](const std::wstring& text)
    {
        Calls.push_back(L"long");
        return text != L"#TST|X|decline";
    },
});

// A window the server opens: "#WIO|" asks for it, "#WIN|" only carries its state.
[[maybe_unused]] const bool Window = Network::HiddenChatLines::Register({
    .name = L"Window",
    .prefixes = {L"#WIN|", L"#WIO|"},
    .take =
        [](const std::wstring& text)
    {
        Calls.push_back(L"window");
        OpenRequested = OpenRequested || text.starts_with(L"#WIO|");
        return true;
    },
    .takeOpenRequest = [] { return std::exchange(OpenRequested, false); },
    .window = mu::ui::window::INTERFACE_TIER_MAPS,
    .enteredWorld = [] { Calls.push_back(L"enter window"); },
});

// Takes nothing, like a feature that wants "#PCK|" but gets "#PCK" alone.
[[maybe_unused]] const bool Picky = Network::HiddenChatLines::Register({
    .name = L"Picky",
    .prefixes = {L"#PCK"},
    .take =
        [](const std::wstring&)
    {
        Calls.push_back(L"picky");
        return false;
    },
});

[[maybe_unused]] const bool FirstByPrefix = Network::HiddenChatLines::Register({
    .name = L"Aaa",
    .prefixes = {L"#AAA|"},
    .take =
        [](const std::wstring&)
    {
        Calls.push_back(L"aaa");
        return true;
    },
    .enteredWorld = [] { Calls.push_back(L"enter aaa"); },
});
} // namespace

// The window functions HiddenChatLines.cpp calls, recorded instead of shown.
namespace UI::Windows
{
void Show(WindowId id)
{
    Shown.push_back(id);
    WindowVisible = true;
}

bool IsVisible(WindowId)
{
    return WindowVisible;
}
} // namespace UI::Windows

TEST_CASE("Hidden chat lines: registration succeeds")
{
    CHECK(LastByPrefix);
    CHECK(Short);
    CHECK(Long);
    CHECK(Window);
    CHECK(Picky);
    CHECK(FirstByPrefix);
}

TEST_CASE("Hidden chat lines: a line no feature registered stays in the chat")
{
    Clear();
    CHECK_FALSE(Network::HiddenChatLines::Take(L"Hello, Lorencia"));
    CHECK_FALSE(Network::HiddenChatLines::Take(L"#UNK|1"));
    CHECK_FALSE(Network::HiddenChatLines::Take(L""));
    CHECK_FALSE(Network::HiddenChatLines::Take(L"say #TST|1")); // a prefix counts only at the start
    CHECK(Calls.empty());
}

TEST_CASE("Hidden chat lines: the feature with the prefix takes the line")
{
    Clear();
    CHECK(Network::HiddenChatLines::Take(L"#TST|1|2"));
    CHECK(Network::HiddenChatLines::Take(L"#ZZZ|"));
    CHECK(Network::HiddenChatLines::Take(L"#AAA|x"));
    CHECK(Calls == std::vector<std::wstring>{L"short", L"zzz", L"aaa"});
}

TEST_CASE("Hidden chat lines: a longer prefix is asked first")
{
    Clear();
    CHECK(Network::HiddenChatLines::Take(L"#TST|X|1"));
    CHECK(Calls == std::vector<std::wstring>{L"long"});
}

TEST_CASE("Hidden chat lines: a line the feature declines goes on to the next prefix")
{
    Clear();
    CHECK(Network::HiddenChatLines::Take(L"#TST|X|decline"));
    CHECK(Calls == std::vector<std::wstring>{L"long", L"short"});
}

TEST_CASE("Hidden chat lines: a line every feature declines goes to the chat")
{
    Clear();
    CHECK_FALSE(Network::HiddenChatLines::Take(L"#PCK"));
    CHECK(Calls == std::vector<std::wstring>{L"picky"});
}

TEST_CASE("Hidden chat lines: a window opens on the server's word only")
{
    Clear();
    CHECK(Network::HiddenChatLines::Take(L"#WIN|state"));
    CHECK(Shown.empty());

    CHECK(Network::HiddenChatLines::Take(L"#WIO|"));
    CHECK(Shown == std::vector{mu::ui::window::INTERFACE_TIER_MAPS});

    // Already shown: asked again, it isn't shown twice.
    CHECK(Network::HiddenChatLines::Take(L"#WIO|"));
    CHECK(Shown.size() == 1);
    CHECK_FALSE(OpenRequested); // each request is taken once
}

TEST_CASE("Hidden chat lines: entering the world goes to every feature once, in prefix order")
{
    Clear();
    Network::HiddenChatLines::EnteredWorld();
    CHECK(Calls == std::vector<std::wstring>{L"enter aaa", L"enter short", L"enter window", L"enter zzz"});
}
