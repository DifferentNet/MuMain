#include "stdafx.h"
#include "GameLogic/Commands/GmPanel.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
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

long long ToLong(const std::wstring& field)
{
    return std::wcstoll(field.c_str(), nullptr, 10);
}

// "0A1F..." -> its bytes; empty when it isn't hex.
std::vector<std::uint8_t> FromHex(const std::wstring& text)
{
    auto digit = [](wchar_t c) -> int
    {
        if (c >= L'0' && c <= L'9')
            return c - L'0';
        if (c >= L'A' && c <= L'F')
            return c - L'A' + 10;
        if (c >= L'a' && c <= L'f')
            return c - L'a' + 10;
        return -1;
    };

    std::vector<std::uint8_t> bytes;
    if (text.size() % 2 != 0)
        return bytes;
    bytes.reserve(text.size() / 2);
    for (size_t i = 0; i + 1 < text.size(); i += 2)
    {
        const int high = digit(text[i]);
        const int low = digit(text[i + 1]);
        if (high < 0 || low < 0)
            return {};
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return bytes;
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
                         // The server takes the bars out of the detail, so the number and the undo flag follow it.
                         e.Detail = At(f, 4);
                         e.Id = ToLong(At(f, 5));
                         e.Undo = At(f, 6);
                         return e;
                     });
        return true;
    case L'E':
        TakeListLine(m_events, suffix, fields, [](const auto& f)
                     {
                         EventInfo e;
                         e.Key = At(f, 0);
                         e.Name = At(f, 1);
                         e.State = At(f, 2);
                         e.Left = At(f, 3).empty() ? -1 : ToInt(At(f, 3));
                         e.Next = At(f, 4);
                         e.Ops = At(f, 5);
                         e.Detail = From(f, 6);
                         return e;
                     });
        return true;
    case L'M':
        TakeListLine(m_monsters, suffix, fields, [](const auto& f)
                     {
                         MonsterInfo m;
                         m.Number = ToInt(At(f, 0));
                         m.Name = At(f, 1);
                         m.Level = ToInt(At(f, 2));
                         return m;
                     });
        return true;
    case L'I':
        TakeItemLine(suffix, fields);
        return true;
    case L'C':
        TakeCharacterLine(suffix, fields);
        return true;
    case L'V':
        TakeInventoryLine(suffix, fields);
        return true;
    case L'T':
        TakeListLine(m_tiers, suffix, fields, [](const auto& f)
                     {
                         TierRun r;
                         r.Leader = At(f, 0);
                         r.Tier = ToInt(At(f, 1));
                         r.Day = ToInt(At(f, 2));
                         r.Map = ToInt(At(f, 3));
                         r.Zone = ToInt(At(f, 4));
                         r.Zones = ToInt(At(f, 5));
                         r.Monsters = ToInt(At(f, 6));
                         r.Alive = ToInt(At(f, 7));
                         r.Roster = At(f, 8);
                         r.Left = ToInt(At(f, 9));
                         r.State = From(f, 10);
                         return r;
                     });
        return true;
    default:
        return false;
    }
}

void Store::TakeItemLine(wchar_t suffix, const std::vector<std::wstring>& fields)
{
    if (suffix == L'0')
    {
        m_itemIncoming = ItemInfo{};
        m_itemIncoming.Group = ToInt(At(fields, 0));
        m_itemIncoming.Number = ToInt(At(fields, 1));
        m_itemIncoming.Name = At(fields, 2);
        m_itemIncoming.MaxLevel = ToInt(At(fields, 3));
        m_itemIncoming.Skill = At(fields, 4) == L"1";
        m_itemIncoming.Luck = At(fields, 5) == L"1";
        m_itemIncoming.Stack = At(fields, 6) == L"1";
        m_itemIncoming.MaxDurability = ToInt(At(fields, 7));
        m_itemReceiving = true;
        return;
    }

    if (!m_itemReceiving)
        return;

    if (suffix == L'.')
    {
        m_item = std::move(m_itemIncoming);
        m_itemIncoming = ItemInfo{};
        m_itemReceiving = false;
        ++m_itemVersion;
        return;
    }

    const auto& kind = At(fields, 0);
    if (kind == L"o")
    {
        m_itemIncoming.Options.push_back({ToInt(At(fields, 1)), ToInt(At(fields, 2)), From(fields, 3)});
    }
    else if (kind == L"x")
    {
        ItemExtraInfo extra;
        extra.Index = ToInt(At(fields, 1));
        extra.Kind = At(fields, 2).empty() ? L'x' : At(fields, 2)[0];
        extra.Bit = ToInt(At(fields, 3));
        extra.Name = From(fields, 4);
        m_itemIncoming.Extras.push_back(std::move(extra));
    }
    else if (kind == L"a")
    {
        ItemSetInfo set;
        set.Index = ToInt(At(fields, 1));
        set.Discriminator = ToInt(At(fields, 2));
        // "1,2"
        const std::wstring& levels = At(fields, 3);
        for (size_t start = 0; start < levels.size();)
        {
            const size_t comma = levels.find(L',', start);
            const std::wstring level = levels.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
            if (!level.empty())
                set.BonusLevels.push_back(ToInt(level));
            if (comma == std::wstring::npos)
                break;
            start = comma + 1;
        }
        set.Name = From(fields, 4);
        m_itemIncoming.Sets.push_back(std::move(set));
    }
}

void Store::TakeCharacterLine(wchar_t suffix, const std::vector<std::wstring>& fields)
{
    if (suffix == L'0')
    {
        CharacterInfo& c = m_characterIncoming;
        c = CharacterInfo{};
        c.Name = At(fields, 0);
        c.Account = At(fields, 1);
        c.Where = At(fields, 2).empty() ? L'o' : At(fields, 2)[0];
        c.Class = ToInt(At(fields, 3));
        c.ClassName = At(fields, 4);
        c.Level = ToInt(At(fields, 5));
        c.MaxLevel = ToInt(At(fields, 6));
        c.Resets = ToInt(At(fields, 7));
        c.ResetLimit = ToInt(At(fields, 8));
        c.Points = ToInt(At(fields, 9));
        c.Zen = ToLong(At(fields, 10));
        c.MaxZen = ToLong(At(fields, 11));
        c.PkState = ToInt(At(fields, 12));
        c.PkCount = ToInt(At(fields, 13));
        c.Map = ToInt(At(fields, 14));
        c.MapName = At(fields, 15);
        c.X = ToInt(At(fields, 16));
        c.Y = ToInt(At(fields, 17));
        m_characterReceiving = true;
        return;
    }

    if (!m_characterReceiving)
        return;

    if (suffix == L'.')
    {
        m_character = std::move(m_characterIncoming);
        m_characterIncoming = CharacterInfo{};
        m_characterReceiving = false;
        ++m_characterVersion;
        return;
    }

    const auto& kind = At(fields, 0);
    if (kind == L"s")
        m_characterIncoming.Stats.push_back({At(fields, 1), ToInt(At(fields, 2)), ToInt(At(fields, 3)), From(fields, 4)});
    else if (kind == L"k")
        m_characterIncoming.Classes.push_back({ToInt(At(fields, 1)), From(fields, 2)});
}

void Store::TakeInventoryLine(wchar_t suffix, const std::vector<std::wstring>& fields)
{
    if (suffix == L'0')
    {
        CharacterItems& items = m_inventoryIncoming;
        items = CharacterItems{};
        items.Name = At(fields, 0);
        items.Where = At(fields, 1).empty() ? L'o' : At(fields, 1)[0];
        items.Zen = ToLong(At(fields, 2));
        items.VaultZen = ToLong(At(fields, 3));
        m_inventoryReceiving = true;
        return;
    }

    if (!m_inventoryReceiving)
        return;

    if (suffix == L'.')
    {
        m_inventory = std::move(m_inventoryIncoming);
        m_inventoryIncoming = CharacterItems{};
        m_inventoryReceiving = false;
        ++m_inventoryVersion;
        return;
    }

    StoredItem item;
    item.Storage = At(fields, 0) == L"v" ? L'v' : L'i';
    item.Slot = ToInt(At(fields, 1));
    item.Data = FromHex(At(fields, 2));
    // The shortest item is 5 bytes (group and number, level, durability, flags).
    if (item.Data.size() >= 5)
        m_inventoryIncoming.Items.push_back(std::move(item));
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
    fresh.m_events.Version = m_events.Version + 1;
    fresh.m_monsters.Version = m_monsters.Version + 1;
    fresh.m_tiers.Version = m_tiers.Version + 1;
    fresh.m_itemVersion = m_itemVersion + 1;
    fresh.m_characterVersion = m_characterVersion + 1;
    fresh.m_inventoryVersion = m_inventoryVersion + 1;
    *this = std::move(fresh);
}

namespace
{
// The GM panel's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"GmPanel",
    .prefixes = {Prefix},
    .take = [](const std::wstring& text) { return Panel().TakeMessage(text); },
    // Y asked whether this character may use the panel, and the answer says yes.
    .takeOpenRequest = [] { return Panel().TakeOpenRequest(); },
    .window = mu::ui::window::INTERFACE_GM_PANEL,
    .enteredWorld = [] { Panel().Reset(); },
});
} // namespace
} // namespace GameLogic::GmPanel
