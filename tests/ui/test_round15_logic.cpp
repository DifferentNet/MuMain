#include <doctest.h>

#include "UI/Chat/ChatSendGate.h"
#include "UI/Chat/InviterDetailText.h"

#include <cstdint>
#include <string>

using UI::Chat::ChatSendGate;
using EnterResult = UI::Chat::ChatSendGate::EnterResult;

namespace
{
constexpr std::uint64_t kCooldownMs = 1000;
constexpr std::uint64_t kStartMs = 50000;
} // namespace

TEST_CASE("the first chat line goes at once [ui][round15]")
{
    ChatSendGate gate(kCooldownMs);
    CHECK(gate.OnEnter(kStartMs) == EnterResult::SendNow);
    CHECK_FALSE(gate.IsPending());
}

TEST_CASE("a line sent too soon waits and goes after the cooldown [ui][round15]")
{
    ChatSendGate gate(kCooldownMs);
    REQUIRE(gate.OnEnter(kStartMs) == EnterResult::SendNow);

    CHECK(gate.OnEnter(kStartMs + 200) == EnterResult::Pending);
    CHECK(gate.IsPending());
    CHECK(gate.OnEnter(kStartMs + 400) == EnterResult::AlreadyPending);

    CHECK_FALSE(gate.TakeDue(kStartMs + kCooldownMs));
    CHECK(gate.TakeDue(kStartMs + kCooldownMs + 1));
    CHECK_FALSE(gate.IsPending());
    CHECK_FALSE(gate.TakeDue(kStartMs + kCooldownMs + 2));

    // The waiting line counts as sent: the next one waits a cooldown from it.
    CHECK(gate.OnEnter(kStartMs + kCooldownMs + 500) == EnterResult::Pending);
}

TEST_CASE("a cancelled line is not sent [ui][round15]")
{
    ChatSendGate gate(kCooldownMs);
    REQUIRE(gate.OnEnter(kStartMs) == EnterResult::SendNow);
    REQUIRE(gate.OnEnter(kStartMs + 100) == EnterResult::Pending);

    gate.Cancel();
    CHECK_FALSE(gate.IsPending());
    CHECK_FALSE(gate.TakeDue(kStartMs + 5 * kCooldownMs));
    CHECK(gate.OnEnter(kStartMs + 5 * kCooldownMs) == EnterResult::SendNow);
}

TEST_CASE("the inviter line shows the class and the guild [ui][round15]")
{
    using UI::Chat::InviterDetail::Compose;
    CHECK(Compose(L"Blade Knight", L"Guild", L"Wolves") == L"Blade Knight \u00B7 Guild Wolves");
    CHECK(Compose(L"Soul Master", L"Guild", L"") == L"Soul Master");
    CHECK(Compose(L"", L"Guild", L"Wolves").empty());
}
