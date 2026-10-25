#include "stdafx.h"
#include "GameLogic/Commands/GmPanel.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>
#include <string_view>

namespace GameLogic::GmPanel
{
namespace
{
constexpr std::wstring_view Prefix = L"#GM";
constexpr const wchar_t* Command = L"/gm ";
// Repeated hellos within this time are dropped (a held Y key, a window that opens and closes).
constexpr auto HelloInterval = std::chrono::seconds(3);

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

// The field at `index`, empty past the end (a newer server may add fields, an older one leave some out).
const std::wstring& At(const std::vector<std::wstring>& fields, size_t index)
{
    static const std::wstring empty;
    return index < fields.size() ? fields[index] : empty;
}

// The rest of the line from `index` on, joined back: the last field is free text and may hold a bar.
std::wstring From(const std::vector<std::wstring>& fields, size_t index)
{
    std::wstring text = At(fields, index);
    for (size_t i = index + 1; i < fields.size(); ++i)
        text += L"|" + fields[i];
    return text;
}

// A list's header ('0'), row (no suffix) or footer ('.').
template <typename Row, typename Parse>
void TakeListLine(List<Row>& list, wchar_t suffix, const std::vector<std::wstring>& fields, Parse parse)
{
    if (suffix == L'0')
    {
        list.Incoming.clear();
        list.Receiving = true;
    }
    else if (suffix == L'.')
    {
        if (list.Receiving)
        {
            list.Rows = std::move(list.Incoming);
            list.Incoming.clear();
            list.Receiving = false;
            list.Received = true;
            ++list.Version;
        }
    }
    else if (list.Receiving)
    {
        list.Incoming.push_back(parse(fields));
    }
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
    if (!view.starts_with(Prefix))
        return false;

    // "#GM" + tag + "|": the tag is "0" (hello), "R" (an action's answer), or a list's letter with "0" (header),
    // nothing (row) or "." (footer).
    const size_t bar = view.find(L'|');
    if (bar == std::wstring_view::npos || bar < Prefix.size() + 1 || bar > Prefix.size() + 2)
        return false;

    const std::wstring_view tag = view.substr(Prefix.size(), bar - Prefix.size());
    const auto fields = Split(view.substr(bar + 1));

    if (tag == L"0")
    {
        const auto& role = At(fields, 0);
        m_role = role == L"admin" ? Role::Admin : role == L"mod" ? Role::Moderator : Role::None;
        m_hidden = At(fields, 2) == L"1";
        m_gmMode = At(fields, 3) == L"1";
        ++m_helloVersion;
        m_openRequested = m_openWhenGm && IsGm();
        m_openWhenGm = false;
        return true;
    }

    if (tag == L"R")
    {
        m_result.Ok = At(fields, 0) == L"ok";
        m_result.Op = At(fields, 1);
        m_result.Text = From(fields, 2);
        ++m_resultVersion;
        return true;
    }

    const wchar_t kind = tag[0];
    const wchar_t suffix = tag.size() > 1 ? tag[1] : L'\0';
    if (suffix != L'\0' && suffix != L'0' && suffix != L'.')
        return false;

    switch (kind)
    {
    case L'P':
        TakeListLine(m_players, suffix, fields, [](const auto& f)
                     {
                         OnlinePlayer p;
                         p.Name = At(f, 0);
                         p.Account = At(f, 1);
                         p.ClassName = At(f, 2);
                         p.Level = ToInt(At(f, 3));
                         p.Resets = ToInt(At(f, 4));
                         p.Map = ToInt(At(f, 5));
                         p.X = ToInt(At(f, 6));
                         p.Y = ToInt(At(f, 7));
                         p.CombatPower = ToInt(At(f, 8));
                         p.Flags = At(f, 9);
                         return p;
                     });
        return true;
    case L'F':
        TakeListLine(m_found, suffix, fields, [](const auto& f)
                     {
                         FoundCharacter c;
                         c.Name = At(f, 0);
                         c.Account = At(f, 1);
                         c.ClassName = At(f, 2);
                         c.Online = At(f, 3) == L"1";
                         c.State = At(f, 4);
                         c.MutedUntil = At(f, 5);
                         c.BannedUntil = At(f, 6);
                         return c;
                     });
        return true;
    case L'B':
        TakeListLine(m_penalties, suffix, fields, [](const auto& f)
                     {
                         Penalty p;
                         p.Name = At(f, 0);
                         p.Account = At(f, 1);
                         p.Kind = At(f, 2);
                         p.Until = At(f, 3);
                         p.GmName = At(f, 4);
                         p.Reason = From(f, 5);
                         return p;
                     });
        return true;
    case L'W':
        TakeListLine(m_warps, suffix, fields, [](const auto& f)
                     {
                         Warp w;
                         w.Index = ToInt(At(f, 0));
                         w.Name = At(f, 1);
                         w.Map = ToInt(At(f, 2));
                         w.X = ToInt(At(f, 3));
                         w.Y = ToInt(At(f, 4));
                         return w;
                     });
        return true;
    case L'L':
        TakeListLine(m_log, suffix, fields, [](const auto& f)
                     {
                         LogEntry e;
                         e.Time = At(f, 0);
                         e.GmName = At(f, 1);
                         e.Op = At(f, 2);
                         e.Target = At(f, 3);
                         e.Detail = From(f, 4);
                         return e;
                     });
        return true;
    default:
        return false;
    }
}

void Store::Hello(bool openWhenGm)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    m_openWhenGm = m_openWhenGm || openWhenGm;
    const auto now = std::chrono::steady_clock::now();
    if (m_lastHello != std::chrono::steady_clock::time_point{} && now - m_lastHello < HelloInterval)
        return;

    m_lastHello = now;
    Send(L"hello");
}

bool Store::TakeOpenRequest()
{
    const bool requested = m_openRequested;
    m_openRequested = false;
    return requested;
}

void Store::Send(const std::wstring& request)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands and
    // shows them to nobody.
    const std::wstring line = Command + request;
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(line.c_str()));
}

void Store::Reset()
{
    // The counts keep going up, so a window that saw the old character's lists still notices the new ones.
    Store fresh;
    fresh.m_helloVersion = m_helloVersion + 1;
    fresh.m_resultVersion = m_resultVersion;
    fresh.m_players.Version = m_players.Version + 1;
    fresh.m_found.Version = m_found.Version + 1;
    fresh.m_penalties.Version = m_penalties.Version + 1;
    fresh.m_warps.Version = m_warps.Version + 1;
    fresh.m_log.Version = m_log.Version + 1;
    *this = std::move(fresh);
}
} // namespace GameLogic::GmPanel
