#include "stdafx.h"
#include "GameLogic/Events/GrandShop.h"

#include "GameLogic/Events/EndlessTowerLines.h"
#include "Network/Server/HiddenChatLines.h"

namespace GameLogic::GrandShop
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
constexpr std::wstring_view Open = L"#GSO|";
constexpr std::wstring_view ShopLine = L"#GS|";
constexpr std::wstring_view Footer = L"#GS.|";
constexpr std::wstring_view Refusal = L"#GS!|";
constexpr std::wstring_view Answer = L"#GSK|";
constexpr std::wstring_view BuyCommand = L"/grbuy ";
constexpr size_t HeaderFields = 3;
constexpr size_t LineFields = 7;
constexpr size_t AnswerFields = 3;
// The server's kind numbers.
constexpr int KindClassic = 0;
constexpr int KindHighTier = 1;
constexpr int KindJewel = 2;
// An action the server did not answer counts as answered after this time, so the button comes back.
constexpr auto BusyTimeout = std::chrono::seconds(8);
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

void Store::TakeHeader(const std::vector<std::wstring>& fields)
{
    m_incomingStatus = {};
    m_incomingLines.clear();
    m_receiving = fields.size() >= HeaderFields;
    if (!m_receiving)
        return;

    m_incomingStatus.Marks = ToLong(fields[0]);
    m_incomingStatus.GrandResets = ToInt(fields[1]);
    m_incomingStatus.SafeZone = fields[2] == L"1";
}

void Store::TakeLine(const std::vector<std::wstring>& fields)
{
    // The set's name comes last, so it may hold a bar.
    if (!m_receiving || fields.size() < LineFields || fields[0].empty())
        return;

    const int kind = ToInt(fields[1]);
    if (kind != KindClassic && kind != KindHighTier && kind != KindJewel)
        return;

    Line line;
    line.Key = fields[0];
    line.LineKind = static_cast<Kind>(kind);
    line.Group = ToInt(fields[2]);
    line.Number = ToInt(fields[3]);
    line.SetType = ToInt(fields[4]);
    line.Price = ToLong(fields[5]);
    line.SetName = Rest(fields, 6);
    m_incomingLines.push_back(std::move(line));
}

void Store::TakeAnswer(const std::vector<std::wstring>& fields)
{
    // "#GSK|1 or 0|marks|text": the text comes last, so it may hold a bar.
    m_busy = false;
    m_lastOk = !fields.empty() && fields[0] == L"1";
    if (fields.size() >= AnswerFields)
    {
        m_status.Marks = ToLong(fields[1]);
        m_lastText = Rest(fields, 2);
    }
    else
    {
        m_lastText.clear();
    }
    ++m_answers;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Open))
    {
        // A new shop: the last answer and refusal are old news.
        m_openRequested = true;
        m_failText.clear();
        if (!m_busy)
        {
            m_lastOk = false;
            m_lastText.clear();
        }
        TakeHeader(Split(view.substr(Open.size())));
        return true;
    }

    if (view.starts_with(ShopLine))
    {
        TakeLine(Split(view.substr(ShopLine.size())));
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_lines = std::move(m_incomingLines);
            m_incomingLines.clear();
            m_receiving = false;
            m_hasInfo = true;
        }
        return true;
    }

    if (view.starts_with(Refusal))
    {
        m_openRequested = true;
        m_receiving = false;
        m_hasInfo = false;
        m_lines.clear();
        m_failText = std::wstring(view.substr(Refusal.size()));
        return true;
    }

    if (view.starts_with(Answer))
    {
        TakeAnswer(Split(view.substr(Answer.size())));
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

bool Store::IsBusy() const
{
    return m_busy && std::chrono::steady_clock::now() - m_busySentAt <= BusyTimeout;
}

void Store::Buy(const std::wstring& key)
{
    if (IsBusy() || key.empty() || !CanSendCommand())
        return;

    m_busy = true;
    m_busySentAt = std::chrono::steady_clock::now();
    m_lastText.clear();
    SendCommand(std::wstring(BuyCommand) + key);
}

void Store::Reset()
{
    m_status = {};
    m_lines.clear();
    m_incomingStatus = {};
    m_incomingLines.clear();
    m_receiving = false;
    m_hasInfo = false;
    m_openRequested = false;
    m_failText.clear();
    m_busy = false;
    m_lastOk = false;
    m_lastText.clear();
}

namespace
{
// The Grand Reset shop window's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"GrandShop",
    .prefixes = {Open, ShopLine, Footer, Refusal, Answer},
    .take = [](const std::wstring& text) { return Grand().TakeMessage(text); },
    // "/grshop" answers with #GSO| (or #GS!|), which opens the window; the data follows in the next lines.
    .takeOpenRequest = [] { return Grand().TakeOpenRequest(); },
    .window = mu::ui::window::INTERFACE_GRAND_SHOP,
    .enteredWorld = [] { Grand().Reset(); },
});
} // namespace
} // namespace GameLogic::GrandShop
