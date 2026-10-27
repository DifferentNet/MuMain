#include "stdafx.h"
#include "GameLogic/Events/BattlePass.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <algorithm>
#include <cwchar>

namespace GameLogic::BattlePass
{
namespace
{
constexpr std::wstring_view Header = L"#BP0|";
constexpr std::wstring_view LevelLine = L"#BPL|";
constexpr std::wstring_view QuestLine = L"#BPQ|";
constexpr std::wstring_view Footer = L"#BP.|";
constexpr std::wstring_view Signal = L"#BP!|";
constexpr const wchar_t* InfoCommand = L"/bpinfo";
// A request this soon after the last one waits (Pump()), so a window that opens and closes, or a signal that
// follows a button, doesn't flood the server.
constexpr auto RequestInterval = std::chrono::milliseconds(1500);

// Splits "a|b|c" (or "a;b", "a:b") into its fields.
std::vector<std::wstring> Split(std::wstring_view text, wchar_t separator = L'|')
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true)
    {
        const size_t bar = text.find(separator, start);
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

// The rest of a line from a field on, joined back: a text field comes last, so it may hold anything.
std::wstring Rest(const std::vector<std::wstring>& fields, size_t from)
{
    std::wstring text;
    for (size_t i = from; i < fields.size(); ++i)
    {
        if (i > from)
            text += L"|";
        text += fields[i];
    }
    return text == L"-" ? std::wstring{} : text;
}

// "i:14:13:0:5;e:3:0:0:1" -> its items; anything it doesn't know is left out.
std::vector<RewardItem> ParseItems(const std::wstring& field)
{
    std::vector<RewardItem> items;
    if (field.empty() || field == L"-")
        return items;

    for (const auto& entry : Split(field, L';'))
    {
        const auto parts = Split(entry, L':');
        if (parts.size() < 5 || parts[0].size() != 1)
            continue;

        RewardItem item;
        item.Count = std::max(1, ToInt(parts[4]));
        switch (parts[0][0])
        {
        case L'i':
            item.Kind = ItemKind::Item;
            item.Group = ToInt(parts[1]);
            item.Number = ToInt(parts[2]);
            item.Level = ToInt(parts[3]);
            break;
        case L'e':
            item.Kind = ItemKind::Excellent;
            item.Options = ToInt(parts[1]);
            item.Luck = parts[2] == L"1";
            item.Level = ToInt(parts[3]);
            break;
        case L'm':
            item.Kind = ItemKind::Mark;
            item.Mark = parts[1];
            item.Season = ToInt(parts[2]);
            break;
        case L'p':
            item.Kind = ItemKind::Placeholder;
            break;
        default:
            continue;
        }
        items.push_back(std::move(item));
    }
    return items;
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
    if (view.starts_with(Signal))
    {
        if (view.substr(Signal.size()).starts_with(L"off"))
        {
            m_off = true;
            m_hasInfo = false;
            m_receiving = false;
            ++m_revision;
        }
        else
        {
            m_refreshWanted = true;
        }
        return true;
    }

    if (view.starts_with(Header))
    {
        // #BP0|season|name|phase|secondsLeft|level|points|levelPoints|perLevel|maxLevel|bonus|bonusMax|bonusPer|
        // claimable|waiting|reroll|multiplier|week
        const auto f = Split(view.substr(Header.size()));
        m_incomingLevels.clear();
        m_incomingQuests.clear();
        m_incomingStatus = {};
        m_receiving = f.size() >= 17;
        if (m_receiving)
        {
            Status& s = m_incomingStatus;
            s.Season = ToInt(f[0]);
            s.Name = f[1];
            s.SeasonPhase = static_cast<Phase>(std::clamp(ToInt(f[2]), 0, 2));
            s.SecondsLeft = ToInt(f[3]);
            s.Level = ToInt(f[4]);
            s.Points = ToInt(f[5]);
            s.LevelPoints = ToInt(f[6]);
            s.PerLevel = std::max(1, ToInt(f[7]));
            s.MaxLevel = std::max(0, ToInt(f[8]));
            s.Bonus = ToInt(f[9]);
            s.BonusMax = ToInt(f[10]);
            s.BonusPer = std::max(1, ToInt(f[11]));
            s.Claimable = ToInt(f[12]);
            s.Waiting = ToInt(f[13]);
            s.RerollAvailable = f[14] == L"1";
            s.Multiplier = std::max(1, ToInt(f[15]));
            s.Week = ToInt(f[16]);
        }
        return true;
    }

    if (view.starts_with(Footer))
    {
        if (m_receiving)
        {
            m_status = m_incomingStatus;
            m_status.ReceivedAt = std::chrono::steady_clock::now();
            m_levels = std::move(m_incomingLevels);
            m_quests = std::move(m_incomingQuests);
            m_incomingLevels.clear();
            m_incomingQuests.clear();
            m_receiving = false;
            m_hasInfo = true;
            m_off = false;
            ++m_revision;
        }
        return true;
    }

    if (view.starts_with(LevelLine))
    {
        // #BPL|level|state|items|text
        const auto f = Split(view.substr(LevelLine.size()));
        if (!m_receiving || f.size() < 4)
            return true;

        Level level;
        level.Number = ToInt(f[0]);
        level.State = static_cast<LevelState>(std::clamp(ToInt(f[1]), 0, 2));
        level.Items = ParseItems(f[2]);
        level.Text = Rest(f, 3);
        m_incomingLevels.push_back(std::move(level));
        return true;
    }

    if (view.starts_with(QuestLine))
    {
        // #BPQ|kind|week|slot|id|state|progress|target|points|text
        const auto f = Split(view.substr(QuestLine.size()));
        if (!m_receiving || f.size() < 9)
            return true;

        Quest quest;
        quest.Kind = f[0] == L"w" ? QuestKind::Weekly : f[0] == L"s" ? QuestKind::Seasonal : QuestKind::Daily;
        quest.Week = ToInt(f[1]);
        quest.Slot = ToInt(f[2]);
        quest.Id = f[3];
        quest.Done = f[4] == L"1";
        quest.Progress = ToInt(f[5]);
        quest.Target = std::max(1, ToInt(f[6]));
        quest.Points = ToInt(f[7]);
        quest.Text = Rest(f, 8);
        m_incomingQuests.push_back(std::move(quest));
        return true;
    }

    return false;
}

void Store::Request()
{
    m_requestPending = true;
    m_refreshWanted = false;
    Pump();
}

void Store::Pump()
{
    if (!m_requestPending)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (m_lastRequest != std::chrono::steady_clock::time_point{} && now - m_lastRequest < RequestInterval)
        return;
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    m_lastRequest = now;
    m_requestPending = false;
    SendCommand(InfoCommand);
}

void Store::Claim(int level)
{
    SendCommand(level > 0 ? L"/bpclaim " + std::to_wstring(level) : std::wstring(L"/bpclaim"));
}

void Store::TakeWaiting()
{
    SendCommand(L"/bptake");
}

void Store::Reroll(int slot)
{
    SendCommand(L"/bpreroll " + std::to_wstring(slot));
}

void Store::Reset()
{
    m_status = {};
    m_levels.clear();
    m_quests.clear();
    m_incomingStatus = {};
    m_incomingLevels.clear();
    m_incomingQuests.clear();
    m_receiving = false;
    m_hasInfo = false;
    m_off = false;
    m_refreshWanted = false;
    m_requestPending = false;
    ++m_revision;
    m_lastRequest = {};
}
} // namespace GameLogic::BattlePass
