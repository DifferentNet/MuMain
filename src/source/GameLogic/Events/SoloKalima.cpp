#include "stdafx.h"
#include "GameLogic/Events/SoloKalima.h"

#include "GameLogic/Events/EndlessTowerLines.h"
#include "Network/Server/HiddenChatLines.h"

namespace GameLogic::SoloKalima
{
namespace
{
// The bar-separated fields and the typed commands work as the Endless Tower's (EndlessTowerLines.h).
using EndlessTower::CanSendCommand;
using EndlessTower::Rest;
using EndlessTower::SendCommand;
using EndlessTower::Split;
using EndlessTower::ToInt;

constexpr std::wstring_view Open = L"#SKO|";
constexpr std::wstring_view Header = L"#SK0|";
constexpr std::wstring_view TierLine = L"#SKT|";
constexpr std::wstring_view CurseLine = L"#SKC|";
constexpr std::wstring_view Footer = L"#SK.|";
constexpr std::wstring_view EnterAnswer = L"#SKE|";
constexpr const wchar_t* InfoCommand = L"/soloinfo";
constexpr const wchar_t* EnterCommand = L"/soloenter ";
// The fields each line needs at least.
constexpr size_t HeaderFields = 9;
constexpr size_t TierFields = 9;
constexpr size_t CurseFields = 3;
// Repeated requests within this time are dropped, so a window that opens and closes doesn't flood the server.
constexpr auto RequestInterval = std::chrono::seconds(2);
// An Enter the server didn't answer counts as answered after this time, so the button comes back.
constexpr auto EnterTimeout = std::chrono::seconds(8);
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Open))
    {
        m_openRequested = true;
        return true;
    }

    if (view.starts_with(Header))
        return TakeHeader(view.substr(Header.size()));

    if (view.starts_with(TierLine))
    {
        TakeTier(view.substr(TierLine.size()));
        return true;
    }

    if (view.starts_with(CurseLine))
    {
        TakeCurse(view.substr(CurseLine.size()));
        return true;
    }

    if (view.starts_with(Footer))
    {
        TakeFooter();
        return true;
    }

    if (view.starts_with(EnterAnswer))
    {
        TakeEnterAnswer(view.substr(EnterAnswer.size()));
        return true;
    }

    return false;
}

bool Store::TakeHeader(std::wstring_view text)
{
    const auto fields = Split(text);
    m_incomingTiers.clear();
    m_incomingCurses.clear();
    m_incomingStatus = {};
    m_receiving = fields.size() >= HeaderFields;
    if (!m_receiving)
        return true;
    m_enterTextStale = !m_lastEnterText.empty();

    m_incomingStatus.Near = fields[0] == L"1";
    m_incomingStatus.Tickets = ToInt(fields[1]);
    m_incomingStatus.RunsLeft = ToInt(fields[2]);
    m_incomingStatus.SecondsToReset = ToInt(fields[3]);
    m_incomingStatus.Enforced = fields[4] == L"1";
    m_incomingStatus.CombatPower = ToInt(fields[5]);
    m_incomingStatus.Health = ToInt(fields[6]);
    m_incomingStatus.InRun = fields[7] == L"1";
    m_incomingStatus.MaxCurses = ToInt(fields[8]);
    return true;
}

void Store::TakeTier(std::wstring_view text)
{
    const auto fields = Split(text);
    if (!m_receiving || fields.size() < TierFields)
        return;

    Tier tier;
    tier.Number = ToInt(fields[0]);
    tier.Name = fields[1];
    tier.CombatPower = ToInt(fields[2]);
    tier.Health = ToInt(fields[3]);
    tier.Open = fields[4] == L"1";
    tier.Kalima = ToInt(fields[5]);
    tier.MyBestSeconds = ToInt(fields[6]);
    tier.ServerBestSeconds = ToInt(fields[7]);
    // The name comes last, so it may hold a bar.
    tier.ServerBestName = Rest(fields, 8);
    m_incomingTiers.push_back(std::move(tier));
}

void Store::TakeCurse(std::wstring_view text)
{
    const auto fields = Split(text);
    if (!m_receiving || fields.size() < CurseFields || fields[0].empty())
        return;

    // The description comes last, so it may hold a bar.
    m_incomingCurses.push_back({fields[0], fields[1], Rest(fields, 2)});
}

void Store::TakeFooter()
{
    if (!m_receiving)
        return;

    m_status = m_incomingStatus;
    m_status.ReceivedAt = std::chrono::steady_clock::now();
    m_tiers = std::move(m_incomingTiers);
    m_curses = std::move(m_incomingCurses);
    m_incomingTiers.clear();
    m_incomingCurses.clear();
    m_receiving = false;
    m_hasInfo = true;
    // An enter answer is news only until the state after it has come in.
    if (m_enterTextStale)
        m_lastEnterText.clear();
    m_enterTextStale = false;
}

void Store::TakeEnterAnswer(std::wstring_view text)
{
    // "#SKE|ok|text": the text comes last, so it may hold anything but a line break.
    const auto fields = Split(text);
    m_entering = false;
    m_lastEnterOk = !fields.empty() && fields[0] == L"1";
    m_lastEnterText = Rest(fields, 1);
    m_enterTextStale = false;
    ++m_enterAnswers;
}

bool Store::TakeOpenRequest()
{
    const bool requested = m_openRequested;
    m_openRequested = false;
    return requested;
}

void Store::Request()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_entering && now - m_enterSentAt > EnterTimeout)
        m_entering = false;

    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (!CanSendCommand())
        return;

    m_lastRequest = now;
    SendCommand(InfoCommand);
}

void Store::Enter(int tier, const std::vector<std::wstring>& curses)
{
    // Nothing would answer a command that can't go out, so don't wait for one.
    if (m_entering || tier <= 0 || !CanSendCommand())
        return;

    std::wstring command = EnterCommand + std::to_wstring(tier);
    for (size_t i = 0; i < curses.size(); ++i)
        command += (i == 0 ? L" " : L",") + curses[i];

    m_entering = true;
    m_enterSentAt = std::chrono::steady_clock::now();
    m_lastEnterText.clear();
    SendCommand(command);
}

void Store::Reset()
{
    m_status = {};
    m_tiers.clear();
    m_curses.clear();
    m_incomingStatus = {};
    m_incomingTiers.clear();
    m_incomingCurses.clear();
    m_receiving = false;
    m_hasInfo = false;
    m_openRequested = false;
    m_entering = false;
    m_lastEnterOk = false;
    m_lastEnterText.clear();
    m_enterTextStale = false;
    m_lastRequest = {};
}

namespace
{
// Kundun's Lair's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"SoloKalima",
    .prefixes = {Open, Header, TierLine, CurseLine, Footer, EnterAnswer},
    .take = [](const std::wstring& text) { return Lair().TakeMessage(text); },
    // The lair's NPC opens the window (#SKO|); the state follows in the next lines.
    .takeOpenRequest = [] { return Lair().TakeOpenRequest(); },
    .window = mu::ui::window::INTERFACE_SOLO_KALIMA,
    .enteredWorld = [] { Lair().Reset(); },
});
} // namespace
} // namespace GameLogic::SoloKalima
