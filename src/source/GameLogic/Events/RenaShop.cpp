#include "stdafx.h"
#include "GameLogic/Events/RenaShop.h"

#include "GameLogic/Events/EndlessTowerLines.h"
#include "Network/Server/HiddenChatLines.h"

#include <algorithm>

namespace GameLogic::RenaShop
{
// The tower's line helpers (bar-separated fields, chat commands) serve this window too.
using GameLogic::EndlessTower::CanSendCommand;
using GameLogic::EndlessTower::Rest;
using GameLogic::EndlessTower::SendCommand;
using GameLogic::EndlessTower::Split;
using GameLogic::EndlessTower::ToInt;
using GameLogic::EndlessTower::ToLong;

namespace
{
constexpr std::wstring_view Open = L"#RNO|";
constexpr std::wstring_view Header = L"#RN0|";
constexpr std::wstring_view ShopLine = L"#RNS|";
constexpr std::wstring_view PrizeLine = L"#RNL|";
constexpr std::wstring_view MilestoneLine = L"#RNM|";
constexpr std::wstring_view WeekLine = L"#RNW|";
constexpr std::wstring_view Footer = L"#RN.|";
constexpr std::wstring_view Answer = L"#RNK|";
constexpr std::wstring_view SpinResultLine = L"#RNR|";
constexpr std::wstring_view InfoCommand = L"/renainfo";
constexpr std::wstring_view GiveCommand = L"/renagive";
constexpr std::wstring_view BuyCommand = L"/renabuy ";
constexpr std::wstring_view SpinCommand = L"/renaspin ";
constexpr size_t HeaderFields = 8;
constexpr size_t ShopFields = 8;
constexpr size_t PrizeFields = 10;
constexpr size_t MilestoneFields = 5;
constexpr size_t WeekFields = 3;
// Repeated requests within this time are dropped, so a window that opens and closes does not flood the server.
constexpr auto RequestInterval = std::chrono::seconds(2);
// An action the server did not answer counts as answered after this time, so the buttons come back.
constexpr auto BusyTimeout = std::chrono::seconds(8);
// The spin results kept for the window.
constexpr size_t MaxResults = 10;
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

void Store::ClearIncoming()
{
    m_incomingStatus = {};
    m_incomingLines.clear();
    m_incomingPrizes.clear();
    m_incomingMilestones.clear();
    m_incomingWeek.clear();
}

void Store::TakeHeader(const std::vector<std::wstring>& fields)
{
    ClearIncoming();
    m_receiving = fields.size() >= HeaderFields;
    if (!m_receiving)
        return;

    m_incomingStatus.Balance = ToLong(fields[0]);
    m_incomingStatus.InBag = ToLong(fields[1]);
    m_incomingStatus.HandedCharacter = ToLong(fields[2]);
    m_incomingStatus.HandedWeek = ToLong(fields[3]);
    m_incomingStatus.WeekPlace = ToInt(fields[4]);
    m_incomingStatus.SpinPrice = ToLong(fields[5]);
    m_incomingStatus.Near = fields[6] == L"1";
    m_incomingStatus.Waiting = ToLong(fields[7]);
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Open))
    {
        // A new talk to the Golden Archer: the last answer is old news.
        m_openRequested = true;
        if (!m_busy)
        {
            m_lastOk = false;
            m_lastText.clear();
        }
        return true;
    }

    if (view.starts_with(Header))
    {
        TakeHeader(Split(view.substr(Header.size())));
        return true;
    }

    if (view.starts_with(ShopLine))
    {
        const auto fields = Split(view.substr(ShopLine.size()));
        if (m_receiving && fields.size() >= ShopFields && !fields[0].empty())
        {
            Line line;
            line.Key = fields[0];
            line.Name = fields[1];
            line.Price = ToLong(fields[2]);
            line.Zen = ToLong(fields[3]);
            line.Group = ToInt(fields[4]);
            line.Number = ToInt(fields[5]);
            line.Level = ToInt(fields[6]);
            line.Count = (std::max)(1, ToInt(fields[7]));
            m_incomingLines.push_back(std::move(line));
        }
        return true;
    }

    if (view.starts_with(PrizeLine))
    {
        const auto fields = Split(view.substr(PrizeLine.size()));
        if (m_receiving && fields.size() >= PrizeFields)
        {
            Prize prize;
            prize.Key = fields[0];
            prize.Name = fields[1];
            prize.Chance = fields[2];
            prize.Announce = fields[3] == L"1";
            prize.Group = ToInt(fields[4]);
            prize.Number = ToInt(fields[5]);
            prize.Level = ToInt(fields[6]);
            prize.Count = (std::max)(1, ToInt(fields[7]));
            prize.ZenMin = ToLong(fields[8]);
            prize.ZenMax = ToLong(fields[9]);
            m_incomingPrizes.push_back(std::move(prize));
        }
        return true;
    }

    if (view.starts_with(MilestoneLine))
    {
        const auto fields = Split(view.substr(MilestoneLine.size()));
        if (m_receiving && fields.size() >= MilestoneFields)
        {
            Milestone milestone;
            milestone.Count = ToLong(fields[0]);
            milestone.TitleId = ToInt(fields[1]);
            milestone.English = fields[2];
            milestone.Russian = fields[3];
            milestone.Reached = fields[4] == L"1";
            m_incomingMilestones.push_back(std::move(milestone));
        }
        return true;
    }

    if (view.starts_with(WeekLine))
    {
        const auto fields = Split(view.substr(WeekLine.size()));
        if (m_receiving && fields.size() >= WeekFields)
        {
            WeekEntry entry;
            entry.Place = ToInt(fields[0]);
            entry.Name = fields[1];
            entry.Handed = ToLong(fields[2]);
            m_incomingWeek.push_back(std::move(entry));
        }
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_lines = std::move(m_incomingLines);
            m_prizes = std::move(m_incomingPrizes);
            m_milestones = std::move(m_incomingMilestones);
            m_week = std::move(m_incomingWeek);
            ClearIncoming();
            m_receiving = false;
            m_hasInfo = true;
        }
        return true;
    }

    if (view.starts_with(SpinResultLine))
    {
        // "#RNR|prize key|text": the text comes last, so it may hold a bar.
        const auto fields = Split(view.substr(SpinResultLine.size()));
        if (m_newSpin)
            m_results.clear();
        m_newSpin = false;
        if (fields.size() >= 2 && m_results.size() < MaxResults)
            m_results.push_back({fields[0], Rest(fields, 1)});
        return true;
    }

    if (view.starts_with(Answer))
    {
        const auto fields = Split(view.substr(Answer.size()));
        m_busy = false;
        m_lastOk = !fields.empty() && fields[0] == L"1";
        m_lastText = Rest(fields, 1);
        ++m_answers;
        return true;
    }

    return false;
}

bool Store::TakeOpenRequest()
{
    const bool requested = m_openRequested;
    m_openRequested = false;
    return requested;
}

void Store::ExpireBusy()
{
    if (m_busy && std::chrono::steady_clock::now() - m_busySentAt > BusyTimeout)
        m_busy = false;
}

void Store::Request()
{
    ExpireBusy();
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (!CanSendCommand())
        return;

    m_lastRequest = now;
    SendCommand(std::wstring(InfoCommand));
}

void Store::Send(const std::wstring& command)
{
    if (!CanSendCommand())
        return;

    m_busy = true;
    m_busySentAt = std::chrono::steady_clock::now();
    m_lastText.clear();
    SendCommand(command);
}

void Store::Give()
{
    ExpireBusy();
    if (m_busy)
        return;
    Send(std::wstring(GiveCommand));
}

void Store::Buy(const std::wstring& key, int count)
{
    ExpireBusy();
    if (m_busy || key.empty() || count < 1 || count > MaximumBuyCount)
        return;
    Send(std::wstring(BuyCommand) + key + L" " + std::to_wstring(count));
}

void Store::Spin(int count)
{
    ExpireBusy();
    if (m_busy || count < 1 || count > MaximumSpins)
        return;
    // The results of this spin replace the last ones when they come.
    m_newSpin = true;
    Send(std::wstring(SpinCommand) + std::to_wstring(count));
}

int Store::MostBuyable(const Line& line) const
{
    if (line.Price <= 0 || m_status.Balance <= 0)
        return 0;
    return static_cast<int>((std::min)(m_status.Balance / line.Price, static_cast<long long>(MaximumBuyCount)));
}

int Store::MostSpins() const
{
    if (m_status.SpinPrice <= 0 || m_status.Balance <= 0)
        return 0;
    return static_cast<int>((std::min)(m_status.Balance / m_status.SpinPrice, static_cast<long long>(MaximumSpins)));
}

void Store::Reset()
{
    m_status = {};
    m_lines.clear();
    m_prizes.clear();
    m_milestones.clear();
    m_week.clear();
    m_results.clear();
    ClearIncoming();
    m_receiving = false;
    m_hasInfo = false;
    m_openRequested = false;
    m_busy = false;
    m_newSpin = true;
    m_lastOk = false;
    m_lastText.clear();
    m_lastRequest = {};
}

namespace
{
// The Rena shop window's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"RenaShop",
    .prefixes = {Open, Header, ShopLine, PrizeLine, MilestoneLine, WeekLine, Footer, Answer, SpinResultLine},
    .take = [](const std::wstring& text) { return Rena().TakeMessage(text); },
    // The Golden Archer opens the window (#RNO|); the data follows in the next lines.
    .takeOpenRequest = [] { return Rena().TakeOpenRequest(); },
    .window = mu::ui::window::INTERFACE_RENA_SHOP,
    .enteredWorld = [] { Rena().Reset(); },
});
} // namespace
} // namespace GameLogic::RenaShop
