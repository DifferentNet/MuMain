#pragma once

#include "UI/Chat/MessageType.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// MU Moldova, items shown in the chat (ghinzuro 2026-11-20, «Вариант 1»): the lines client and server exchange, as
// contract v1 fixes them (side-session note item-link-spec.md). The player puts an item into the chat line; the
// client sends "/il <ch> <refs> <text>", the server checks the items, writes the line itself and sends every
// receiver that said "/ilinkhello" a hidden "#ILK|…" right before that chat line (UI/Chat/ItemLinks.h).
namespace UI::Chat::ItemLinkWire
{
// Sent on entering the world; the server answers HelloTag.
inline constexpr std::wstring_view HelloCommand = L"/ilinkhello";
inline constexpr std::wstring_view HelloTag = L"#ILH|";
inline constexpr std::wstring_view LinksTag = L"#ILK|";
inline constexpr std::wstring_view LinkCommand = L"/il";
inline constexpr int Version = 1;
inline constexpr int MaxLinks = 3;
// An item is at most 15 bytes, sent as hex.
inline constexpr std::size_t MaxItemHexChars = 30;
// A shout comes as a normal chat line from this sender, its text "Name: line".
inline constexpr std::wstring_view ShoutSender = L"[POST]";
// The longest name in brackets the server writes; the client shows its own name as long at most.
inline constexpr std::size_t MaxNameLength = 28;
inline constexpr wchar_t Ellipsis = L'…';
// The command goes in a chat packet (C1, at most 255 bytes): a 3-byte header and the 10-byte name before it,
// a zero after it.
inline constexpr std::size_t MaxCommandBytes = 255 - 3 - 10 - 1;
// The longest line the server writes for a channel, the party's "~" and the guild's "@" in front counted
// (ItemLinkRules.MaxChatLength, the client's MAX_CHAT_SIZE).
inline constexpr std::size_t MaxChatLength = 90;

// The storages a ref names.
inline constexpr wchar_t InventoryStorage = L'i';
inline constexpr wchar_t VaultStorage = L'v';

// The channels of "/il" and "#ILK" (the whisper of "/il" is "w:<name>").
inline constexpr wchar_t NormalChannel = L'n';
inline constexpr wchar_t PartyChannel = L'p';
inline constexpr wchar_t GuildChannel = L'g';
inline constexpr wchar_t WhisperChannel = L'w';
inline constexpr wchar_t ShoutChannel = L's';

struct Hello
{
    int maxItems = 0;
    int cooldownSeconds = 0;
};

// "#ILH|1|<maxItems>|<cooldownSeconds>"; empty for any other line or version.
std::optional<Hello> ParseHello(std::wstring_view line);

struct WireLink
{
    // UTF-16 units of "[name]" in the shown text (after the channel prefix is dropped).
    int offset = 0;
    int length = 0;
    // The item as the server's serializer writes it.
    std::vector<std::uint8_t> item;
};

struct LinksLine
{
    std::wstring sender;
    wchar_t channel = NormalChannel;
    std::vector<WireLink> links;
};

// "#ILK|1|<sender>|<ch>|<off>:<len>:<hex>;…" with 1 to MaxLinks links; empty when any part is wrong.
std::optional<LinksLine> ParseLinks(std::wstring_view line);

// The "#ILK" channel a chat line of this sender and type belongs to; empty for lines that never carry links.
std::optional<wchar_t> ChannelOfLine(std::wstring_view sender, mu::ui::window::MESSAGE_TYPE type);

// Whether a chat line of this channel is the player `ownName`'s own: a shout comes from ShoutSender, its text
// "Name: line".
bool IsOwnLine(std::wstring_view sender, std::wstring_view text, wchar_t channel, std::wstring_view ownName);

// One item of "/il": "<storage><slot>:<type>:<level>", e.g. "i37:6153:13".
std::wstring ItemRef(wchar_t storage, int slot, int type, int level);

// The chat input's channel buttons.
enum class InputChannel
{
    Normal,
    Party,
    Guild,
    Gens,
};

// Where a line with items goes: the "/il" channel, and where the message starts in the typed text.
struct Route
{
    std::wstring channel;
    std::size_t bodyStart = 0;
};

// A typed "/shout " or "/post " goes as a shout; any other command, and the gens channel, cannot carry items
// (empty: the line goes as typed). Otherwise the whisper target when there is one, else the channel chosen.
std::optional<Route> RouteOf(std::wstring_view text, InputChannel channel, std::wstring_view whisperTarget);

// How many characters the line of a "/il" channel may have: MaxChatLength less the prefix the server puts in front
// of it (party and guild: 89).
std::size_t MaxLineLength(std::wstring_view channel);

// The client's guard against the server's cooldown (one message with items per account every cooldownSeconds of
// "#ILH"), so a line the server would refuse stays in the chat bar instead of going and being lost. The server
// counts from when it took the line, which only its answer tells: the guard counts from the sender's own line
// coming back on its channel (OwnLineBack()), with its items or without them (the server delivers the line alone when
// it cannot write an item or its notice). Until then it counts from the sending, and a line that has not come back
// after EchoWaitMs was refused, so the server's cooldown never began: the guard lets go of it. A whisper comes back
// to no one; it counts from the sending, WhisperMarginMs longer for its way to the server. Times are milliseconds
// of one monotonic clock.
class Cooldown
{
public:
    static constexpr std::uint64_t EchoWaitMs = 2'000;
    static constexpr std::uint64_t WhisperMarginMs = 1'000;
    // How long a refused line is still offered back to the chat bar (Refused()).
    static constexpr std::uint64_t RefusedKeepMs = 60'000;

    // A message with items went on `channel` (the "/il" channel's letter); its line comes back to the sender on every
    // channel but the whisper.
    void Sent(std::uint64_t nowMs, wchar_t channel);
    // The sender's own line came back on `channel`: the server took the message with items it waits for there just
    // now. Nothing while none waits there (a plain line on another channel, one after the answer).
    void OwnLineBack(std::uint64_t nowMs, wchar_t channel);
    // The whole seconds before the next message with items may go, rounded up as the server counts them
    // (PostRules.WaitSeconds); 0 when it may go now.
    int Wait(std::uint64_t nowMs, int cooldownSeconds) const;
    // The last message with items should have come back and has not after EchoWaitMs: the server refused it (and
    // said why). False once RefusedKeepMs passed since it went, for a whisper, and once it came back.
    bool Refused(std::uint64_t nowMs) const;

private:
    std::optional<std::uint64_t> m_fromMs;
    std::uint64_t m_marginMs = 0;
    bool m_awaitingEcho = false;
    wchar_t m_channel = L'\0';
};

// An item's token in the typed text.
struct PlacedRef
{
    std::size_t start = 0;
    std::size_t length = 0;
    std::wstring ref;
};

// "/il <channel> <refs> <body>": the body is the text from bodyStart with each token (in order, all at or after
// bodyStart, not overlapping) replaced by "{1}", "{2}", … and the player's own braces turned into parentheses.
// Empty when the tokens are not in that order or reach past the text.
std::optional<std::wstring> BuildCommand(std::wstring_view channel, std::wstring_view text, std::size_t bodyStart,
                                         const std::vector<PlacedRef>& placed);

// How many bytes the text takes in UTF-8, as the packet carries it.
std::size_t Utf8Bytes(std::wstring_view text);

// The name in at most MaxNameLength characters, as the server cuts its own (ItemLinkRules.VisibleName): the level at
// its end (" +1" .. " +15") stays whole and the rest is cut, ending with an ellipsis ("Divine Sword of Archang… +15").
std::wstring ClipName(std::wstring_view name);

// The name and then the suffix in at most MaxNameLength characters: when they don't fit, the name is cut (never inside
// a surrogate pair, no space before the ellipsis) and ends with an ellipsis, the suffix stays whole.
std::wstring Clip(std::wstring_view name, std::wstring_view suffix);
} // namespace UI::Chat::ItemLinkWire
