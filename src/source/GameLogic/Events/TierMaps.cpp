#include "stdafx.h"
#include "GameLogic/Events/TierMaps.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>

namespace GameLogic::TierMaps
{
namespace
{
constexpr std::wstring_view Open = L"#TMO|";
constexpr std::wstring_view Header = L"#TM0|";
constexpr std::wstring_view TierLine = L"#TMT|";
constexpr std::wstring_view MemberLine = L"#TMP|";
constexpr std::wstring_view Footer = L"#TM.|";
constexpr std::wstring_view EnterAnswer = L"#TME|";
constexpr const wchar_t* InfoCommand = L"/tierinfo";
// Repeated requests within this time are dropped, so a window that opens and closes doesn't flood the server.
constexpr auto RequestInterval = std::chrono::seconds(2);
// An Enter the server didn't answer counts as answered after this time, so the button comes back.
constexpr auto EnterTimeout = std::chrono::seconds(8);

// Splits "a|b|c" into its fields.
std::vector<std::wstring> Split(std::wstring_view text)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true)
    {
        const size_t bar = text.find(L'|', start);
        fields.emplace_back(text.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start));
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }
    return fields;
}

int ToInt(const std::wstring& field)
{
    return static_cast<int>(std::wcstol(field.c_str(), nullptr, 10));
}

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}
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
    {
        const auto fields = Split(view.substr(Header.size()));
        m_incomingTiers.clear();
        m_incomingMembers.clear();
        m_incomingStatus = {};
        m_receiving = fields.size() >= 14;
        if (m_receiving)
        {
            m_incomingStatus.Near = fields[0] == L"1";
            m_incomingStatus.Leader = fields[1] == L"1";
            m_incomingStatus.PartySize = ToInt(fields[2]);
            m_incomingStatus.Ticket = fields[3] == L"1";
            m_incomingStatus.RunsPerDay = ToInt(fields[4]);
            m_incomingStatus.SecondsToReset = ToInt(fields[5]);
            m_incomingStatus.Enforced = fields[6] == L"1";
            m_incomingStatus.AverageCombatPower = ToInt(fields[7]);
            m_incomingStatus.LowestCombatPower = ToInt(fields[8]);
            m_incomingStatus.MemberSharePercent = ToInt(fields[9]);
            m_incomingStatus.Layout = fields[11];
            m_incomingStatus.Boss = fields[12];
            m_incomingStatus.InRun = fields[13] == L"1";
            // Since the health floor (2026-10-24); an older server leaves them out.
            if (fields.size() >= 16)
            {
                m_incomingStatus.TunedPartySize = ToInt(fields[14]);
                m_incomingStatus.SmallPartyPercent = ToInt(fields[15]);
            }
        }
        return true;
    }

    if (view.starts_with(TierLine))
    {
        const auto fields = Split(view.substr(TierLine.size()));
        if (m_receiving && fields.size() >= 9)
        {
            Tier tier;
            tier.Number = ToInt(fields[0]);
            tier.Name = fields[1];
            tier.CombatPower = ToInt(fields[2]);
            tier.Open = fields[3] == L"1";
            tier.RecordSeconds = ToInt(fields[4]);
            tier.RecordMembers = fields[5];
            tier.Layout = fields[7];
            tier.Boss = fields[8];
            tier.BaseCombatPower = tier.CombatPower;
            if (fields.size() >= 11)
            {
                tier.BaseCombatPower = ToInt(fields[9]);
                tier.MinimumHealth = ToInt(fields[10]);
            }
            m_incomingTiers.push_back(std::move(tier));
        }
        return true;
    }

    if (view.starts_with(MemberLine))
    {
        const auto fields = Split(view.substr(MemberLine.size()));
        if (m_receiving && fields.size() >= 4)
        {
            Member member;
            member.Name = fields[0];
            member.CombatPower = ToInt(fields[1]);
            member.RunsLeft = ToInt(fields[2]);
            member.Near = fields[3] == L"1";
            if (fields.size() >= 5)
                member.Health = ToInt(fields[4]);
            m_incomingMembers.push_back(std::move(member));
        }
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_status.ReceivedAt = std::chrono::steady_clock::now();
            m_tiers = std::move(m_incomingTiers);
            m_members = std::move(m_incomingMembers);
            m_incomingTiers.clear();
            m_incomingMembers.clear();
            m_receiving = false;
            m_hasInfo = true;
            ++m_version;
        }
        return true;
    }

    if (view.starts_with(EnterAnswer))
    {
        // "#TME|ok|text": the text comes last, so it may hold anything but a line break.
        const auto fields = Split(view.substr(EnterAnswer.size()));
        m_entering = false;
        m_lastEnterOk = !fields.empty() && fields[0] == L"1";
        m_lastEnterText.clear();
        for (size_t i = 1; i < fields.size(); ++i)
            m_lastEnterText += (i > 1 ? L"|" : L"") + fields[i];
        ++m_enterAnswers;
        ++m_version;
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

void Store::Request()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_entering && now - m_enterSentAt > EnterTimeout)
    {
        m_entering = false;
        ++m_version;
    }

    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    m_lastRequest = now;
    SendCommand(InfoCommand);
}

void Store::Enter(int tier)
{
    if (m_entering || tier <= 0)
        return;

    m_entering = true;
    m_enterSentAt = std::chrono::steady_clock::now();
    m_lastEnterText.clear();
    ++m_version;
    SendCommand(L"/tierenter " + std::to_wstring(tier));
}

void Store::Reset()
{
    m_status = {};
    m_tiers.clear();
    m_members.clear();
    m_incomingStatus = {};
    m_incomingTiers.clear();
    m_incomingMembers.clear();
    m_receiving = false;
    m_hasInfo = false;
    m_openRequested = false;
    m_entering = false;
    m_lastEnterOk = false;
    m_lastEnterText.clear();
    m_lastRequest = {};
    ++m_version;
}
} // namespace GameLogic::TierMaps
