#include "stdafx.h"
#include "GameLogic/Events/Titles.h"

#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <algorithm>
#include <cwchar>

namespace GameLogic::Titles
{
namespace
{
constexpr std::wstring_view WornLine = L"#TTL|";
constexpr std::wstring_view ListLine = L"#TTW";

// Splits "a|b|c" into its fields.
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

long long ToNumber(const std::wstring& field)
{
    return std::wcstoll(field.c_str(), nullptr, 10);
}

// "3", "2.5": a percent with a dot whatever the C locale says, kept within 0..100.
float ToPercent(const std::wstring& field)
{
    float value = 0.f;
    float scale = 0.f;
    for (const wchar_t c : field)
    {
        if (c == L'.' && scale == 0.f)
            scale = 0.1f;
        else if (c >= L'0' && c <= L'9')
        {
            if (scale == 0.f)
                value = value * 10.f + static_cast<float>(c - L'0');
            else
            {
                value += scale * static_cast<float>(c - L'0');
                scale *= 0.1f;
            }
        }
        else
            break;
    }
    return std::clamp(value, 0.f, 100.f);
}

Rarity ToRarity(const std::wstring& field)
{
    return static_cast<Rarity>(std::clamp<long long>(ToNumber(field), 0, RarityCount - 1));
}

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}
} // namespace

const std::wstring& Worn::Text() const
{
    return Store::Russian() && !Russian.empty() ? Russian : English;
}

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::Russian()
{
    const std::wstring& locale = GameConfig::GetInstance().GetUILocale();
    return locale.size() >= 2 && (locale[0] == L'r' || locale[0] == L'R') && (locale[1] == L'u' || locale[1] == L'U');
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(WornLine))
    {
        TakeWorn(Split(view.substr(WornLine.size())));
        return true;
    }

    // "#TTW0|", "#TTW|", "#TTW.|"
    if (!view.starts_with(ListLine))
        return false;

    const size_t bar = view.find(L'|');
    if (bar == std::wstring_view::npos || bar > ListLine.size() + 1)
        return false;

    TakeListLine(view.substr(ListLine.size(), bar - ListLine.size()), Split(view.substr(bar + 1)));
    return true;
}

void Store::TakeWorn(const std::vector<std::wstring>& fields)
{
    // name|title|rarity|English|Russian
    if (fields.empty() || fields[0].empty())
        return;

    if (fields.size() < 5 || fields[1].empty())
    {
        m_worn.erase(fields[0]);
        return;
    }

    Worn worn;
    worn.Id = fields[1];
    worn.Color = ToRarity(fields[2]);
    worn.English = fields[3];
    worn.Russian = fields[4];
    m_worn[fields[0]] = std::move(worn);
}

void Store::TakeListLine(std::wstring_view tag, const std::vector<std::wstring>& fields)
{
    if (tag == L"0")
    {
        // worn|auto: a new list starts.
        m_incoming.clear();
        m_incomingWearing = fields.empty() ? std::wstring() : fields[0];
        m_incomingAutomatic = fields.size() > 1 && fields[1] == L"1";
        return;
    }

    if (tag == L".")
    {
        m_list = std::move(m_incoming);
        m_incoming.clear();
        m_wearing = m_incomingWearing;
        m_automatic = m_incomingAutomatic;
        m_hasList = true;
        m_listTime = GetTickCount64();
        ++m_listVersion;
        return;
    }

    // title|rarity|term|owned|seconds left|name|hint|bonus
    if (!tag.empty() || fields.size() < 6 || fields[0].empty())
        return;

    Entry entry;
    entry.Id = fields[0];
    entry.Color = ToRarity(fields[1]);
    entry.Lasts = static_cast<Term>(std::clamp<long long>(ToNumber(fields[2]), 0, 3));
    entry.Owned = fields[3] == L"1";
    entry.SecondsLeft = ToNumber(fields[4]);
    entry.Name = fields[5];
    if (fields.size() > 6)
        entry.Hint = fields[6];
    if (fields.size() > 7)
    {
        // "HP;taken;damage".
        const auto parts = Split(fields[7], L';');
        float* const values[] = {&entry.BonusHealth, &entry.BonusTaken, &entry.BonusDamage};
        for (size_t i = 0; i < std::size(values) && i < parts.size(); ++i)
            *values[i] = ToPercent(parts[i]);
    }
    m_incoming.push_back(std::move(entry));
}

void Store::Request()
{
    SendCommand(L"/titlesask");
}

void Store::RequestList()
{
    SendCommand(Russian() ? L"/titleslist ru" : L"/titleslist en");
}

void Store::Wear(std::wstring_view id)
{
    SendCommand(id.empty() ? std::wstring(L"/title off") : L"/title " + std::wstring(id));
    RequestList();
}

void Store::Reset()
{
    m_worn.clear();
    m_list.clear();
    m_incoming.clear();
    m_wearing.clear();
    m_hasList = false;
    ++m_listVersion;
}

const Worn* Store::Of(std::wstring_view name) const
{
    if (m_worn.empty())
        return nullptr;

    const auto it = m_worn.find(name);
    return it == m_worn.end() ? nullptr : &it->second;
}
} // namespace GameLogic::Titles
