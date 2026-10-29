#include "stdafx.h"
#include "GameLogic/Events/EndlessTower.h"

#include "GameLogic/Events/EndlessTowerLines.h"

namespace GameLogic::EndlessTower
{
namespace
{
constexpr std::wstring_view Open = L"#ETO|";
constexpr std::wstring_view Header = L"#ET0|";
constexpr std::wstring_view MemberLine = L"#ETP|";
constexpr std::wstring_view TopLine = L"#ETR|";
constexpr std::wstring_view BossLine = L"#ETB|";
constexpr std::wstring_view HeightLine = L"#ETW|";
constexpr std::wstring_view Footer = L"#ET.|";
constexpr std::wstring_view EnterAnswer = L"#ETE|";
constexpr const wchar_t* InfoCommand = L"/towerinfo";
constexpr const wchar_t* EnterCommand = L"/towerenter";
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

void Store::ClearIncoming()
{
    m_incomingStatus = {};
    m_incomingMembers.clear();
    m_incomingTop.clear();
    m_incomingBossRewards.clear();
    m_incomingHeightRewards.clear();
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(Open))
    {
        // A new talk to Lugard: the last refusal is old news, the window shows today's reason instead.
        m_openRequested = true;
        if (!m_entering)
        {
            m_lastEnterOk = false;
            m_lastEnterText.clear();
            ++m_version;
        }
        return true;
    }

    if (view.starts_with(Header))
    {
        const auto fields = Split(view.substr(Header.size()));
        ClearIncoming();
        m_receiving = fields.size() >= 9;
        if (m_receiving)
        {
            m_incomingStatus.Near = fields[0] == L"1";
            m_incomingStatus.Leader = fields[1] == L"1";
            m_incomingStatus.PartySize = ToInt(fields[2]);
            m_incomingStatus.MinimumParty = ToInt(fields[3]);
            m_incomingStatus.MaximumParty = ToInt(fields[4]);
            m_incomingStatus.Price = ToLong(fields[5]);
            m_incomingStatus.RunsPerDay = ToInt(fields[6]);
            m_incomingStatus.SecondsToReset = ToInt(fields[7]);
            m_incomingStatus.InRun = fields[8] == L"1";
        }
        return true;
    }

    if (view.starts_with(MemberLine))
    {
        const auto fields = Split(view.substr(MemberLine.size()));
        if (m_receiving && fields.size() >= 5)
        {
            Member member;
            member.Name = fields[0];
            member.RunsLeft = ToInt(fields[1]);
            member.Near = fields[2] == L"1";
            member.HasZen = fields[3] == L"1";
            member.BestHeight = ToInt(fields[4]);
            m_incomingMembers.push_back(std::move(member));
        }
        return true;
    }

    if (view.starts_with(TopLine))
    {
        const auto fields = Split(view.substr(TopLine.size()));
        if (m_receiving && fields.size() >= 6)
        {
            TopEntry entry;
            entry.Place = ToInt(fields[0]);
            entry.Name = fields[1];
            entry.ClassName = fields[2];
            entry.Height = ToInt(fields[3]);
            entry.Seconds = ToInt(fields[4]);
            entry.PartySize = ToInt(fields[5]);
            m_incomingTop.push_back(std::move(entry));
        }
        return true;
    }

    if (view.starts_with(BossLine) || view.starts_with(HeightLine))
    {
        const bool boss = view.starts_with(BossLine);
        const auto fields = Split(view.substr(BossLine.size()));
        if (m_receiving && fields.size() >= 2)
        {
            Reward reward;
            reward.FromWave = ToInt(fields[0]);
            reward.Text = Rest(fields, 1);
            (boss ? m_incomingBossRewards : m_incomingHeightRewards).push_back(std::move(reward));
        }
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_status.ReceivedAt = std::chrono::steady_clock::now();
            m_members = std::move(m_incomingMembers);
            m_top = std::move(m_incomingTop);
            m_bossRewards = std::move(m_incomingBossRewards);
            m_heightRewards = std::move(m_incomingHeightRewards);
            ClearIncoming();
            m_receiving = false;
            m_hasInfo = true;
            ++m_version;
        }
        return true;
    }

    if (view.starts_with(EnterAnswer))
    {
        // "#ETE|ok|text": the text comes last, so it may hold anything but a line break.
        const auto fields = Split(view.substr(EnterAnswer.size()));
        m_entering = false;
        m_lastEnterOk = !fields.empty() && fields[0] == L"1";
        m_lastEnterText = Rest(fields, 1);
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
    if (!CanSendCommand())
        return;

    m_lastRequest = now;
    SendCommand(InfoCommand);
}

void Store::Enter()
{
    if (m_entering)
        return;

    m_entering = true;
    m_enterSentAt = std::chrono::steady_clock::now();
    m_lastEnterText.clear();
    ++m_version;
    SendCommand(EnterCommand);
}

void Store::Reset()
{
    m_status = {};
    m_members.clear();
    m_top.clear();
    m_bossRewards.clear();
    m_heightRewards.clear();
    ClearIncoming();
    m_receiving = false;
    m_hasInfo = false;
    m_openRequested = false;
    m_entering = false;
    m_lastEnterOk = false;
    m_lastEnterText.clear();
    m_lastRequest = {};
    ++m_version;
}
} // namespace GameLogic::EndlessTower
