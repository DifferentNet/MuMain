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

// The name cut to MaxNameLength, ending with an ellipsis when cut (as the server cuts its own).
std::wstring ClipName(std::wstring name);
} // namespace UI::Chat::ItemLinkWire
