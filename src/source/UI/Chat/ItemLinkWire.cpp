#include "UI/Chat/ItemLinkWire.h"

#include <array>
#include <cwctype>

namespace UI::Chat::ItemLinkWire
{
namespace
{
// Offsets, lengths and counts in the lines are short decimal numbers; a longer one is a broken line.
constexpr std::size_t MaxNumberDigits = 5;
constexpr std::size_t HexDigitsPerByte = 2;
constexpr std::wstring_view WhisperRoutePrefix = L"w:";
constexpr std::array<std::wstring_view, 2> ShoutCommands = {L"/shout ", L"/post "};

constexpr std::uint32_t FirstTwoByteCodePoint = 0x80;
constexpr std::uint32_t FirstThreeByteCodePoint = 0x800;
constexpr std::uint32_t FirstFourByteCodePoint = 0x10000;
constexpr std::uint32_t FirstHighSurrogate = 0xD800;
constexpr std::uint32_t FirstLowSurrogate = 0xDC00;
constexpr std::uint32_t PastLowSurrogates = 0xE000;

std::vector<std::wstring_view> Split(std::wstring_view text, wchar_t separator)
{
    std::vector<std::wstring_view> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t at = text.find(separator, start);
        fields.push_back(text.substr(start, at == std::wstring_view::npos ? std::wstring_view::npos : at - start));
        if (at == std::wstring_view::npos)
            return fields;
        start = at + 1;
    }
}

std::optional<int> ParseNumber(std::wstring_view text)
{
    if (text.empty() || text.size() > MaxNumberDigits)
        return std::nullopt;
    int value = 0;
    for (const wchar_t c : text)
    {
        if (c < L'0' || c > L'9')
            return std::nullopt;
        value = value * 10 + (c - L'0');
    }
    return value;
}

int HexDigit(wchar_t c)
{
    if (c >= L'0' && c <= L'9')
        return c - L'0';
    if (c >= L'A' && c <= L'F')
        return c - L'A' + 10;
    if (c >= L'a' && c <= L'f')
        return c - L'a' + 10;
    return -1;
}

std::optional<std::vector<std::uint8_t>> FromHex(std::wstring_view text)
{
    if (text.empty() || text.size() > MaxItemHexChars || text.size() % HexDigitsPerByte != 0)
        return std::nullopt;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / HexDigitsPerByte);
    for (std::size_t i = 0; i < text.size(); i += HexDigitsPerByte)
    {
        const int high = HexDigit(text[i]);
        const int low = HexDigit(text[i + 1]);
        if (high < 0 || low < 0)
            return std::nullopt;
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return bytes;
}

bool IsVersion(std::wstring_view field)
{
    const auto version = ParseNumber(field);
    return version && *version == Version;
}

bool IsLinkChannel(wchar_t c)
{
    return c == NormalChannel || c == PartyChannel || c == GuildChannel || c == WhisperChannel || c == ShoutChannel;
}

// "<off>:<len>:<hex>"; a link is at least "[]".
std::optional<WireLink> ParseLink(std::wstring_view entry)
{
    const auto parts = Split(entry, L':');
    if (parts.size() != 3)
        return std::nullopt;
    const auto offset = ParseNumber(parts[0]);
    const auto length = ParseNumber(parts[1]);
    auto item = FromHex(parts[2]);
    if (!offset || !length || *length < 2 || !item)
        return std::nullopt;
    return WireLink{*offset, *length, std::move(*item)};
}

wchar_t AsciiLower(wchar_t c)
{
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

bool StartsWithIgnoringAsciiCase(std::wstring_view text, std::wstring_view prefix)
{
    if (text.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        if (AsciiLower(text[i]) != AsciiLower(prefix[i]))
            return false;
    }
    return true;
}

void AppendWithoutBraces(std::wstring& out, std::wstring_view text)
{
    for (const wchar_t c : text)
        out.push_back(c == L'{' ? L'(' : c == L'}' ? L')' : c);
}

std::size_t Utf8BytesOf(std::uint32_t codePoint)
{
    if (codePoint < FirstTwoByteCodePoint)
        return 1;
    if (codePoint < FirstThreeByteCodePoint)
        return 2;
    if (codePoint < FirstFourByteCodePoint)
        return 3;
    return 4;
}

bool IsHighSurrogate(wchar_t c)
{
    const auto unit = static_cast<std::uint32_t>(c);
    return unit >= FirstHighSurrogate && unit < FirstLowSurrogate;
}

std::wstring_view TrimStart(std::wstring_view text)
{
    while (!text.empty() && std::iswspace(text.front()))
        text.remove_prefix(1);
    return text;
}

std::wstring_view TrimEnd(std::wstring_view text)
{
    while (!text.empty() && std::iswspace(text.back()))
        text.remove_suffix(1);
    return text;
}

// The level at the end of a name, " +1" .. " +15" (a space, a plus and one or two digits); empty when there is none.
std::wstring_view LevelSuffix(std::wstring_view name)
{
    constexpr std::wstring_view LevelMark = L" +";
    constexpr std::size_t MaxLevelDigits = 2;
    const std::size_t plus = name.rfind(LevelMark);
    if (plus == std::wstring_view::npos || plus == 0)
        return {};
    const std::wstring_view digits = name.substr(plus + LevelMark.size());
    if (digits.empty() || digits.size() > MaxLevelDigits)
        return {};
    for (const wchar_t c : digits)
    {
        if (c < L'0' || c > L'9')
            return {};
    }
    return name.substr(plus);
}
} // namespace

std::optional<Hello> ParseHello(std::wstring_view line)
{
    if (!line.starts_with(HelloTag))
        return std::nullopt;
    const auto fields = Split(line.substr(HelloTag.size()), L'|');
    if (fields.size() < 3 || !IsVersion(fields[0]))
        return std::nullopt;
    const auto maxItems = ParseNumber(fields[1]);
    const auto cooldown = ParseNumber(fields[2]);
    if (!maxItems || *maxItems < 1 || !cooldown)
        return std::nullopt;
    return Hello{*maxItems, *cooldown};
}

std::optional<LinksLine> ParseLinks(std::wstring_view line)
{
    if (!line.starts_with(LinksTag))
        return std::nullopt;
    const auto fields = Split(line.substr(LinksTag.size()), L'|');
    if (fields.size() != 4 || !IsVersion(fields[0]) || fields[1].empty() || fields[2].size() != 1
        || !IsLinkChannel(fields[2][0]))
    {
        return std::nullopt;
    }

    LinksLine parsed{std::wstring(fields[1]), fields[2][0], {}};
    for (const std::wstring_view entry : Split(fields[3], L';'))
    {
        auto link = ParseLink(entry);
        if (!link)
            return std::nullopt;
        parsed.links.push_back(std::move(*link));
    }
    if (parsed.links.empty() || parsed.links.size() > static_cast<std::size_t>(MaxLinks))
        return std::nullopt;
    return parsed;
}

std::optional<wchar_t> ChannelOfLine(std::wstring_view sender, mu::ui::window::MESSAGE_TYPE type)
{
    switch (type)
    {
    case mu::ui::window::TYPE_CHAT_MESSAGE:
        return sender == ShoutSender ? ShoutChannel : NormalChannel;
    case mu::ui::window::TYPE_GM_MESSAGE:
        return NormalChannel;
    case mu::ui::window::TYPE_PARTY_MESSAGE:
        return PartyChannel;
    case mu::ui::window::TYPE_GUILD_MESSAGE:
        return GuildChannel;
    case mu::ui::window::TYPE_WHISPER_MESSAGE:
        return WhisperChannel;
    default:
        return std::nullopt;
    }
}

bool IsOwnLine(std::wstring_view sender, std::wstring_view text, wchar_t channel, std::wstring_view ownName)
{
    if (ownName.empty())
        return false;
    if (channel != ShoutChannel)
        return sender == ownName;
    return text.starts_with(ownName) && text.substr(ownName.size()).starts_with(L": ");
}

std::wstring ItemRef(wchar_t storage, int slot, int type, int level)
{
    return std::wstring(1, storage) + std::to_wstring(slot) + L':' + std::to_wstring(type) + L':'
           + std::to_wstring(level);
}

std::optional<Route> RouteOf(std::wstring_view text, InputChannel channel, std::wstring_view whisperTarget)
{
    for (const std::wstring_view command : ShoutCommands)
    {
        if (StartsWithIgnoringAsciiCase(text, command))
            return Route{std::wstring(1, ShoutChannel), command.size()};
    }
    if (text.starts_with(L'/'))
        return std::nullopt;
    if (!whisperTarget.empty())
        return Route{std::wstring(WhisperRoutePrefix) + std::wstring(whisperTarget), 0};

    switch (channel)
    {
    case InputChannel::Normal:
        return Route{std::wstring(1, NormalChannel), 0};
    case InputChannel::Party:
        return Route{std::wstring(1, PartyChannel), 0};
    case InputChannel::Guild:
        return Route{std::wstring(1, GuildChannel), 0};
    default:
        return std::nullopt;
    }
}

std::size_t MaxLineLength(std::wstring_view channel)
{
    constexpr std::size_t PrefixLength = 1;
    const bool prefixed = channel.size() == 1 && (channel.front() == PartyChannel || channel.front() == GuildChannel);
    return prefixed ? MaxChatLength - PrefixLength : MaxChatLength;
}

void Cooldown::Sent(std::uint64_t nowMs, wchar_t channel)
{
    const bool comesBack = channel != WhisperChannel;
    m_fromMs = nowMs;
    m_marginMs = comesBack ? 0 : WhisperMarginMs;
    m_awaitingEcho = comesBack;
    m_channel = channel;
}

void Cooldown::OwnLineBack(std::uint64_t nowMs, wchar_t channel)
{
    if (!m_awaitingEcho || channel != m_channel)
        return;
    m_fromMs = nowMs;
    m_marginMs = 0;
    m_awaitingEcho = false;
}

int Cooldown::Wait(std::uint64_t nowMs, int cooldownSeconds) const
{
    constexpr std::uint64_t MillisecondsPerSecond = 1000;
    if (!m_fromMs || cooldownSeconds <= 0)
        return 0;
    const std::uint64_t passed = nowMs > *m_fromMs ? nowMs - *m_fromMs : 0;
    if (m_awaitingEcho && passed >= EchoWaitMs)
        return 0;
    const std::uint64_t lastsMs = static_cast<std::uint64_t>(cooldownSeconds) * MillisecondsPerSecond + m_marginMs;
    if (passed >= lastsMs)
        return 0;
    return static_cast<int>((lastsMs - passed + MillisecondsPerSecond - 1) / MillisecondsPerSecond);
}

bool Cooldown::Refused(std::uint64_t nowMs) const
{
    if (!m_fromMs || !m_awaitingEcho)
        return false;
    const std::uint64_t passed = nowMs > *m_fromMs ? nowMs - *m_fromMs : 0;
    return passed >= EchoWaitMs && passed < RefusedKeepMs;
}

std::optional<std::wstring> BuildCommand(std::wstring_view channel, std::wstring_view text, std::size_t bodyStart,
                                         const std::vector<PlacedRef>& placed)
{
    if (placed.empty() || bodyStart > text.size())
        return std::nullopt;

    std::wstring refs;
    std::wstring body;
    std::size_t at = bodyStart;
    int number = 0;
    for (const PlacedRef& token : placed)
    {
        if (token.start < at || token.start + token.length > text.size())
            return std::nullopt;
        AppendWithoutBraces(body, text.substr(at, token.start - at));
        body += L'{' + std::to_wstring(++number) + L'}';
        if (!refs.empty())
            refs += L',';
        refs += token.ref;
        at = token.start + token.length;
    }
    AppendWithoutBraces(body, text.substr(at));
    return std::wstring(LinkCommand) + L' ' + std::wstring(channel) + L' ' + refs + L' ' + body;
}

std::size_t Utf8Bytes(std::wstring_view text)
{
    std::size_t bytes = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto unit = static_cast<std::uint32_t>(text[i]);
        const bool pair = unit >= FirstHighSurrogate && unit < FirstLowSurrogate && i + 1 < text.size()
                          && static_cast<std::uint32_t>(text[i + 1]) >= FirstLowSurrogate
                          && static_cast<std::uint32_t>(text[i + 1]) < PastLowSurrogates;
        bytes += pair ? 4 : Utf8BytesOf(unit);
        if (pair)
            ++i;
    }
    return bytes;
}

std::wstring Clip(std::wstring_view name, std::wstring_view suffix)
{
    if (name.size() + suffix.size() <= MaxNameLength)
        return std::wstring(name) + std::wstring(suffix);

    std::size_t keep = MaxNameLength - suffix.size() - 1;
    if (keep > 0 && IsHighSurrogate(name[keep - 1]))
        --keep;
    std::wstring clipped(TrimEnd(name.substr(0, keep)));
    clipped.push_back(Ellipsis);
    clipped += suffix;
    return clipped;
}

std::wstring ClipName(std::wstring_view name)
{
    name = TrimEnd(TrimStart(name));
    const std::wstring_view level = LevelSuffix(name);
    return Clip(TrimEnd(name.substr(0, name.size() - level.size())), level);
}
} // namespace UI::Chat::ItemLinkWire
