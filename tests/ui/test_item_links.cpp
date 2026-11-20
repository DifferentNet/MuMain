#include <doctest.h>

#include "UI/Chat/ItemLinkAttach.h"
#include "UI/Chat/ItemLinkDraft.h"
#include "UI/Chat/ItemLinkPending.h"
#include "UI/Chat/ItemLinkTokens.h"
#include "UI/Chat/ItemLinkWire.h"

#include <string>
#include <vector>

using namespace UI::Chat;
using ItemLinkAttach::Span;
using ItemLinkTokens::Token;

namespace
{
ItemLinkWire::WireLink Link(int offset, int length)
{
    return {offset, length, {0x01, 0x02, 0x03, 0x04, 0x05}};
}

ItemLinkWire::LinksLine Line(std::wstring sender, wchar_t channel)
{
    return {std::move(sender), channel, {Link(0, 4)}};
}

bool SameSpan(const std::optional<Span>& span, std::size_t start, std::size_t length)
{
    return span && span->start == start && span->length == length;
}

ItemLinkDraft::Item DraftItem(std::wstring token, std::wstring ref)
{
    return {std::move(token), std::move(ref), nullptr, LinkRarity::Excellent};
}
} // namespace

TEST_CASE("The hello line gives the server's limits")
{
    const auto hello = ItemLinkWire::ParseHello(L"#ILH|1|3|5");
    REQUIRE(hello);
    CHECK(hello->maxItems == 3);
    CHECK(hello->cooldownSeconds == 5);
}

TEST_CASE("A hello of another version or a broken one is not taken")
{
    CHECK_FALSE(ItemLinkWire::ParseHello(L"#ILH|2|3|5"));
    CHECK_FALSE(ItemLinkWire::ParseHello(L"#ILH|1|0|5"));
    CHECK_FALSE(ItemLinkWire::ParseHello(L"#ILH|1|x|5"));
    CHECK_FALSE(ItemLinkWire::ParseHello(L"#ILH|1|3"));
    CHECK_FALSE(ItemLinkWire::ParseHello(L"#ILK|1|3|5"));
}

TEST_CASE("A links line gives the sender, the channel and each link")
{
    const auto line = ItemLinkWire::ParseLinks(L"#ILK|1|Hero|p|6:12:0A1B2C3D4E;20:5:ff00aa11bb");
    REQUIRE(line);
    CHECK(line->sender == L"Hero");
    CHECK(line->channel == L'p');
    REQUIRE(line->links.size() == 2);
    CHECK(line->links[0].offset == 6);
    CHECK(line->links[0].length == 12);
    CHECK(line->links[0].item == std::vector<std::uint8_t>{0x0A, 0x1B, 0x2C, 0x3D, 0x4E});
    CHECK(line->links[1].item == std::vector<std::uint8_t>{0xFF, 0x00, 0xAA, 0x11, 0xBB});
}

TEST_CASE("The shout's sender comes with its brackets")
{
    const auto line = ItemLinkWire::ParseLinks(L"#ILK|1|[POST]|s|8:10:0102030405");
    REQUIRE(line);
    CHECK(line->sender == L"[POST]");
    CHECK(line->channel == L's');
}

TEST_CASE("A links line with any broken part is not taken")
{
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|2|Hero|n|0:4:0102030405"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1||n|0:4:0102030405"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|x|0:4:0102030405"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:010203040"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:01020304zz"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:1:0102030405"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:" + std::wstring(32, L'A')));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:0102;5:4:0102;10:4:0102;15:4:0102"));
    CHECK_FALSE(ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:0102|extra"));
}

TEST_CASE("Thirty hex digits make the longest item")
{
    const auto line = ItemLinkWire::ParseLinks(L"#ILK|1|Hero|n|0:4:" + std::wstring(30, L'F'));
    REQUIRE(line);
    CHECK(line->links[0].item.size() == 15);
}

TEST_CASE("Each chat line type belongs to its links channel")
{
    using namespace mu::ui::window;
    CHECK(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_CHAT_MESSAGE) == L'n');
    CHECK(ItemLinkWire::ChannelOfLine(L"[POST]", TYPE_CHAT_MESSAGE) == L's');
    CHECK(ItemLinkWire::ChannelOfLine(L"Gm", TYPE_GM_MESSAGE) == L'n');
    CHECK(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_PARTY_MESSAGE) == L'p');
    CHECK(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_GUILD_MESSAGE) == L'g');
    CHECK(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_WHISPER_MESSAGE) == L'w');
    CHECK_FALSE(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_GENS_MESSAGE));
    CHECK_FALSE(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_SYSTEM_MESSAGE));
    CHECK_FALSE(ItemLinkWire::ChannelOfLine(L"Hero", TYPE_ERROR_MESSAGE));
}

TEST_CASE("A link attaches where its brackets stand")
{
    const std::wstring_view text = L"look [Exc Helm +13] ok";
    const auto spans = ItemLinkAttach::Attach(text, {Link(5, 14)});
    REQUIRE(spans.size() == 1);
    CHECK(SameSpan(spans[0], 5, 14));
}

TEST_CASE("A link whose text moved is found by its length after the link before it")
{
    // A chat phrase before the link came out longer in the reader's language.
    const std::wstring_view text = L"Hello there! [Helm +2] and [Sword]";
    const auto spans = ItemLinkAttach::Attach(text, {Link(6, 9), Link(20, 7)});
    REQUIRE(spans.size() == 2);
    CHECK(SameSpan(spans[0], 13, 9));
    CHECK(SameSpan(spans[1], 27, 7));
}

TEST_CASE("A link with no brackets of its length stays plain text")
{
    const auto spans = ItemLinkAttach::Attach(std::wstring_view(L"no links [here]"), {Link(0, 9)});
    REQUIRE(spans.size() == 1);
    CHECK_FALSE(spans[0]);
}

TEST_CASE("Links listed out of order attach by their offsets")
{
    const std::wstring_view text = L"[B] then [AA]";
    const auto spans = ItemLinkAttach::Attach(text, {Link(9, 4), Link(0, 3)});
    CHECK(SameSpan(spans[0], 9, 4));
    CHECK(SameSpan(spans[1], 0, 3));
}

TEST_CASE("On a 32-bit text a character past U+FFFF counts two UTF-16 units")
{
    // /play's wchar_t: the emoji takes one character but two of the server's units.
    const std::u32string text = U"\U0001F600 [Ring]";
    const auto spans = ItemLinkAttach::Attach(std::u32string_view(text), {Link(3, 6)});
    REQUIRE(spans.size() == 1);
    CHECK(SameSpan(spans[0], 2, 6));
}

TEST_CASE("On a 16-bit text the same offset is the index")
{
    const std::u16string text = u"\U0001F600 [Ring]";
    const auto spans = ItemLinkAttach::Attach(std::u16string_view(text), {Link(3, 6)});
    REQUIRE(spans.size() == 1);
    CHECK(SameSpan(spans[0], 3, 6));
}

TEST_CASE("The reader's names replace the server's and the links move with them")
{
    const std::wstring_view text = L"a [Helm +2] b [Sword] c";
    const auto renamed =
        ItemLinkAttach::Rename(text, {Span{2, 9}, Span{14, 7}}, {L"[Шлем +2]", L"[Меч длинный]"});
    CHECK(renamed.text == L"a [Шлем +2] b [Меч длинный] c");
    CHECK(SameSpan(renamed.spans[0], 2, 9));
    CHECK(SameSpan(renamed.spans[1], 14, 13));
}

TEST_CASE("An unattached link keeps its text when renamed")
{
    const auto renamed = ItemLinkAttach::Rename(L"x [A] y", {std::nullopt, Span{2, 3}}, {L"[Q]", L""});
    CHECK(renamed.text == L"x [A] y");
    CHECK_FALSE(renamed.spans[0]);
    CHECK(SameSpan(renamed.spans[1], 2, 3));
}

TEST_CASE("A row with links comes in pieces")
{
    const auto segments = ItemLinkAttach::Segments(L"Hero : ", L"see [Helm] and [Ring]!", {Span{4, 6}, Span{15, 6}});
    REQUIRE(segments.size() == 5);
    CHECK(segments[0].text == L"Hero : see ");
    CHECK(segments[0].link == -1);
    CHECK(segments[1].text == L"[Helm]");
    CHECK(segments[1].link == 0);
    CHECK(segments[2].text == L" and ");
    CHECK(segments[3].text == L"[Ring]");
    CHECK(segments[3].link == 1);
    CHECK(segments[4].text == L"!");
}

TEST_CASE("A row that starts with a link and has no lead has no empty piece")
{
    const auto segments = ItemLinkAttach::Segments(L"", L"[Helm]", {Span{0, 6}});
    REQUIRE(segments.size() == 1);
    CHECK(segments[0].link == 0);
}

TEST_CASE("A waiting links line goes to the next line of its sender and channel")
{
    ItemLinkPending pending;
    pending.Put(Line(L"Hero", L'n'), 1'000);
    CHECK_FALSE(pending.Take(L"Hero", L'p', 1'100));
    CHECK_FALSE(pending.Take(L"Other", L'n', 1'100));
    const auto taken = pending.Take(L"Hero", L'n', 1'200);
    REQUIRE(taken);
    CHECK(taken->sender == L"Hero");
    CHECK_FALSE(pending.Take(L"Hero", L'n', 1'300));
}

TEST_CASE("A links line nothing took goes after its time")
{
    ItemLinkPending pending;
    pending.Put(Line(L"Hero", L'w'), 1'000);
    CHECK_FALSE(pending.Take(L"Hero", L'w', 1'000 + ItemLinkPending::KeptMs + 1));
    CHECK(pending.Waiting() == 0);
}

TEST_CASE("A newer links line of the same sender and channel replaces the waiting one")
{
    ItemLinkPending pending;
    auto first = Line(L"Hero", L'g');
    first.links[0].offset = 1;
    auto second = Line(L"Hero", L'g');
    second.links[0].offset = 2;
    pending.Put(first, 1'000);
    pending.Put(second, 1'100);
    CHECK(pending.Waiting() == 1);
    CHECK(pending.Take(L"Hero", L'g', 1'200)->links[0].offset == 2);
}

TEST_CASE("The waiting list never grows past its cap")
{
    ItemLinkPending pending;
    for (int i = 0; i < 40; ++i)
        pending.Put(Line(L"P" + std::to_wstring(i), L'n'), 1'000);
    CHECK(pending.Waiting() == ItemLinkPending::MaxWaiting);
    CHECK_FALSE(pending.Take(L"P0", L'n', 1'000));
    CHECK(pending.Take(L"P39", L'n', 1'000));
}

TEST_CASE("A typed shout goes to the shout channel, its message after the command")
{
    using ItemLinkWire::InputChannel;
    const auto shout = ItemLinkWire::RouteOf(L"/Shout hi [Helm]", InputChannel::Party, L"Friend");
    REQUIRE(shout);
    CHECK(shout->channel == L"s");
    CHECK(shout->bodyStart == 7);
    const auto post = ItemLinkWire::RouteOf(L"/post [Helm]", InputChannel::Normal, L"");
    REQUIRE(post);
    CHECK(post->channel == L"s");
    CHECK(post->bodyStart == 6);
}

TEST_CASE("Other commands and the gens channel send items as plain text")
{
    using ItemLinkWire::InputChannel;
    CHECK_FALSE(ItemLinkWire::RouteOf(L"/trade [Helm]", InputChannel::Normal, L""));
    CHECK_FALSE(ItemLinkWire::RouteOf(L"/shoutx [Helm]", InputChannel::Normal, L""));
    CHECK_FALSE(ItemLinkWire::RouteOf(L"[Helm]", InputChannel::Gens, L""));
}

TEST_CASE("The whisper target goes before the channel, the channel otherwise")
{
    using ItemLinkWire::InputChannel;
    CHECK(ItemLinkWire::RouteOf(L"[Helm]", InputChannel::Guild, L"Friend")->channel == L"w:Friend");
    CHECK(ItemLinkWire::RouteOf(L"[Helm]", InputChannel::Normal, L"")->channel == L"n");
    CHECK(ItemLinkWire::RouteOf(L"[Helm]", InputChannel::Party, L"")->channel == L"p");
    CHECK(ItemLinkWire::RouteOf(L"[Helm]", InputChannel::Guild, L"")->channel == L"g");
}

TEST_CASE("The command numbers the tokens in order and turns the player's braces into parentheses")
{
    const std::wstring text = L"{x} [Helm +13] or [Ring]}";
    const auto command = ItemLinkWire::BuildCommand(
        L"n", text, 0, {{4, 10, L"i37:6153:13"}, {18, 6, L"v121:6700:0"}});
    REQUIRE(command);
    CHECK(*command == L"/il n i37:6153:13,v121:6700:0 (x) {1} or {2})");
}

TEST_CASE("A shout's command starts its message after the typed command")
{
    const std::wstring text = L"/shout sell [Helm]";
    const auto command = ItemLinkWire::BuildCommand(L"s", text, 7, {{12, 6, L"i12:1:0"}});
    REQUIRE(command);
    CHECK(*command == L"/il s i12:1:0 sell {1}");
}

TEST_CASE("A line that is only the item has the placeholder as its message")
{
    const auto command = ItemLinkWire::BuildCommand(L"w:Friend", L"[Helm]", 0, {{0, 6, L"i0:3:9"}});
    REQUIRE(command);
    CHECK(*command == L"/il w:Friend i0:3:9 {1}");
}

TEST_CASE("Tokens out of order, before the message or past the text give no command")
{
    const std::wstring text = L"/shout [A] [B]";
    CHECK_FALSE(ItemLinkWire::BuildCommand(L"s", text, 7, {{11, 3, L"i1:1:0"}, {7, 3, L"i2:1:0"}}));
    CHECK_FALSE(ItemLinkWire::BuildCommand(L"s", text, 8, {{7, 3, L"i2:1:0"}}));
    CHECK_FALSE(ItemLinkWire::BuildCommand(L"n", text, 0, {{12, 5, L"i2:1:0"}}));
    CHECK_FALSE(ItemLinkWire::BuildCommand(L"n", text, 0, {}));
}

TEST_CASE("A ref names the storage, slot, type and level")
{
    CHECK(ItemLinkWire::ItemRef(ItemLinkWire::InventoryStorage, 37, 6153, 13) == L"i37:6153:13");
    CHECK(ItemLinkWire::ItemRef(ItemLinkWire::VaultStorage, 120, 0, 0) == L"v120:0:0");
}

TEST_CASE("A long name is cut with an ellipsis")
{
    CHECK(ItemLinkWire::ClipName(L"Short Name") == L"Short Name");
    const std::wstring exact(ItemLinkWire::MaxNameLength, L'a');
    CHECK(ItemLinkWire::ClipName(exact) == exact);
    CHECK(ItemLinkWire::Clip(exact, L"") == exact);
    // No level: only the ellipsis.
    const std::wstring cut = ItemLinkWire::ClipName(L"Excellent Divine Sword of Archangel");
    CHECK(cut == L"Excellent Divine Sword of A…");
    CHECK(cut.size() == ItemLinkWire::MaxNameLength);
    // No space before the ellipsis.
    const std::wstring as(26, L'a');
    CHECK(ItemLinkWire::ClipName(as + L" bbbb") == as + ItemLinkWire::Ellipsis);
}

TEST_CASE("A cut name keeps its level, as the server cuts it")
{
    const std::wstring sword = ItemLinkWire::ClipName(L"Divine Sword of Archangel +15");
    CHECK(sword == L"Divine Sword of Archang… +15");
    CHECK(sword.size() == ItemLinkWire::MaxNameLength);
    CHECK(ItemLinkWire::ClipName(L"Excellent Great Dragon Armor +13") == L"Excellent Great Dragon… +13");
    CHECK(ItemLinkWire::Clip(L"Excellent Great Dragon Armor", L" +13") == L"Excellent Great Dragon… +13");
    // Exactly 28 with the level, a short name, spaces around it.
    CHECK(ItemLinkWire::ClipName(L"Excellent Great Dragon A +13") == L"Excellent Great Dragon A +13");
    CHECK(ItemLinkWire::ClipName(L"Excellent Kris +3") == L"Excellent Kris +3");
    CHECK(ItemLinkWire::ClipName(L" Kris  +3 ") == L"Kris +3");
    // Not a level: no digits, three digits, a letter, or the name's start.
    CHECK(ItemLinkWire::ClipName(L"Excellent Box +x") == L"Excellent Box +x");
    CHECK(ItemLinkWire::ClipName(L"Excellent Divine Sword of Arch +") == L"Excellent Divine Sword of A…");
    CHECK(ItemLinkWire::ClipName(L"Excellent Divine Sword of Arch +100") == L"Excellent Divine Sword of A…");
    CHECK(ItemLinkWire::ClipName(L"+5") == L"+5");
    // In another language the level stays too.
    CHECK(ItemLinkWire::ClipName(L"Отличный Меч Божественного Архангела +15") == L"Отличный Меч Божественн… +15");
}

TEST_CASE("A cut name never splits a surrogate pair")
{
    const std::wstring as(26, L'a');
    if constexpr (sizeof(wchar_t) == 2)
    {
        const std::wstring emoji = ItemLinkWire::Clip(as + L"\U0001F600bb", L"");
        CHECK(emoji == as + ItemLinkWire::Ellipsis);
        const std::wstring withLevel = ItemLinkWire::Clip(std::wstring(22, L'a') + L"\U0001F600bb", L" +15");
        CHECK(withLevel == std::wstring(22, L'a') + ItemLinkWire::Ellipsis + L" +15");
    }
    else
    {
        // One wchar_t is one code point: the emoji is one character, as a whole one it may stay.
        CHECK(ItemLinkWire::Clip(as + L"\U0001F600bb", L"") == as + L"\U0001F600" + ItemLinkWire::Ellipsis);
    }
}

TEST_CASE("The command's size is counted as the packet carries it")
{
    CHECK(ItemLinkWire::Utf8Bytes(L"abc") == 3);
    CHECK(ItemLinkWire::Utf8Bytes(L"Шлем") == 8);
    CHECK(ItemLinkWire::Utf8Bytes(L"…") == 3);
    CHECK(ItemLinkWire::Utf8Bytes(L"\U0001F600") == 4);
}

TEST_CASE("Typing before a token moves it, typing after it leaves it")
{
    std::vector<Token> tokens = {{2, 6, 7}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"a [Helm] b", L"xa [Helm] b", 1, tokens));
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].start == 3);
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"xa [Helm] b", L"xa [Helm]! b", 10, tokens));
    CHECK(tokens[0].start == 3);
}

TEST_CASE("A bracket typed right before a token does not break it")
{
    std::vector<Token> tokens = {{1, 6, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"x[Helm]", L"x[[Helm]", 2, tokens));
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].start == 2);
}

TEST_CASE("Backspace at a token's end takes the whole token out")
{
    std::vector<Token> tokens = {{2, 6, 0}, {9, 3, 1}};
    const auto fix = ItemLinkTokens::ApplyEdit(L"a [Helm] [R]", L"a [Helm [R]", 7, tokens);
    REQUIRE(fix);
    CHECK(fix->text == L"a  [R]");
    CHECK(fix->caret == 2);
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].start == 3);
    CHECK(tokens[0].key == 1);
}

TEST_CASE("Delete at a token's start takes the whole token out")
{
    std::vector<Token> tokens = {{1, 6, 0}};
    const auto fix = ItemLinkTokens::ApplyEdit(L"a[Helm]b", L"aHelm]b", 1, tokens);
    REQUIRE(fix);
    CHECK(fix->text == L"ab");
    CHECK(fix->caret == 1);
    CHECK(tokens.empty());
}

TEST_CASE("An edit inside a token turns it into plain text")
{
    std::vector<Token> tokens = {{0, 6, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Helm]", L"[Hexlm]", 4, tokens));
    CHECK(tokens.empty());
    tokens = {{0, 6, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Helm]", L"[Hem]", 3, tokens));
    CHECK(tokens.empty());
}

TEST_CASE("Replacing a selection over a token removes it")
{
    std::vector<Token> tokens = {{2, 6, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"a [Helm] b", L"x", 1, tokens));
    CHECK(tokens.empty());
}

TEST_CASE("Without a caret that fits, the narrowest edit is followed")
{
    std::vector<Token> tokens = {{2, 6, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"a [Helm] b", L"a [Helm] bc", 0, tokens));
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].start == 2);
}

TEST_CASE("Edits on both sides of a token in one frame keep it at its new start")
{
    // Home, "p", End, "q" before the field is followed: one edit across the whole token.
    std::vector<Token> tokens = {{0, 16, 4}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Bone Blade +15]", L"p[Bone Blade +15]q", 18, tokens));
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].start == 1);
    CHECK(tokens[0].length == 16);
    CHECK(tokens[0].key == 4);
}

TEST_CASE("Typing inside a token still drops it, even with the same token after it")
{
    std::vector<Token> tokens = {{0, 16, 0}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Bone Blade +15]", L"[Bone Blxade +15]", 9, tokens));
    CHECK(tokens.empty());
    tokens = {{0, 6, 0}, {7, 6, 1}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Helm] [Helm]", L"[Hexlm] [Helm]", 4, tokens));
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].key == 1);
    CHECK(tokens[0].start == 8);
}

TEST_CASE("Edits around two tokens in one frame keep both, in order")
{
    std::vector<Token> tokens = {{0, 6, 0}, {11, 6, 1}};
    CHECK_FALSE(ItemLinkTokens::ApplyEdit(L"[Helm] and [Ring]", L"p[Helm] and [Ring]q", 19, tokens));
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].key == 0);
    CHECK(tokens[0].start == 1);
    CHECK(tokens[1].key == 1);
    CHECK(tokens[1].start == 12);
}

TEST_CASE("A token goes in at the caret, never inside another token")
{
    std::vector<Token> tokens = {{2, 6, 0}};
    CHECK(ItemLinkTokens::InsertPoint(tokens, 4) == 8);
    CHECK(ItemLinkTokens::InsertPoint(tokens, 2) == 2);
    CHECK(ItemLinkTokens::InsertPoint(tokens, 8) == 8);
    ItemLinkTokens::AddToken(tokens, 2, 3, 1);
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].key == 1);
    CHECK(tokens[0].start == 2);
    CHECK(tokens[1].start == 5);
}

TEST_CASE("The field's caret in characters maps to the wide text")
{
    const std::wstring text = L"a\U0001F600b";
    const std::size_t bIndex = text.size() - 1;
    CHECK(ItemLinkTokens::IndexOfCharacter(text, 2) == bIndex);
    CHECK(ItemLinkTokens::CharacterOfIndex(text, bIndex) == 2);
    CHECK(ItemLinkTokens::IndexOfCharacter(text, 10) == text.size());
}

TEST_CASE("The draft refuses a fourth item and an item that does not fit")
{
    ItemLinkDraft draft;
    std::wstring text = L"hi ";
    const auto helm = DraftItem(L"[Helm]", L"i12:1:0");
    REQUIRE(draft.CanInsert(text, helm, 3, 20) == ItemLinkDraft::Refusal::None);
    draft.Insert(text, text.size(), helm);
    draft.Insert(text, text.size(), helm);
    CHECK(draft.CanInsert(text, helm, 2, 20) == ItemLinkDraft::Refusal::TooManyItems);
    CHECK(draft.CanInsert(text, DraftItem(L"[Very long name]", L"i1:1:0"), 3, 20) == ItemLinkDraft::Refusal::TooLong);
}

TEST_CASE("A party or guild line has one character less for its items")
{
    CHECK(ItemLinkWire::MaxLineLength(L"n") == ItemLinkWire::MaxChatLength);
    CHECK(ItemLinkWire::MaxLineLength(L"s") == ItemLinkWire::MaxChatLength);
    CHECK(ItemLinkWire::MaxLineLength(L"w:Friend") == ItemLinkWire::MaxChatLength);
    CHECK(ItemLinkWire::MaxLineLength(L"p") == ItemLinkWire::MaxChatLength - 1);
    CHECK(ItemLinkWire::MaxLineLength(L"g") == ItemLinkWire::MaxChatLength - 1);

    // 84 characters and a 6-character token: 90 fits the normal chat, not the party's 89.
    ItemLinkDraft draft;
    const std::wstring text(84, L'a');
    const auto helm = DraftItem(L"[Helm]", L"i12:1:0");
    CHECK(draft.CanInsert(text, helm, 3, ItemLinkWire::MaxLineLength(L"n")) == ItemLinkDraft::Refusal::None);
    CHECK(draft.CanInsert(text, helm, 3, ItemLinkWire::MaxLineLength(L"p")) == ItemLinkDraft::Refusal::TooLong);
    CHECK(draft.CanInsert(text, helm, 3, ItemLinkWire::MaxLineLength(L"g")) == ItemLinkDraft::Refusal::TooLong);
}

TEST_CASE("The cooldown counts whole seconds up, as the server does")
{
    CHECK(ItemLinkWire::CooldownWait(std::nullopt, 1000, 5) == 0);
    CHECK(ItemLinkWire::CooldownWait(1000, 1000, 5) == 5);
    CHECK(ItemLinkWire::CooldownWait(1000, 1001, 5) == 5);
    CHECK(ItemLinkWire::CooldownWait(1000, 2000, 5) == 4);
    CHECK(ItemLinkWire::CooldownWait(1000, 5999, 5) == 1);
    CHECK(ItemLinkWire::CooldownWait(1000, 6000, 5) == 0);
    CHECK(ItemLinkWire::CooldownWait(1000, 60000, 5) == 0);
    // No cooldown from the server, a clock that went back.
    CHECK(ItemLinkWire::CooldownWait(1000, 1500, 0) == 0);
    CHECK(ItemLinkWire::CooldownWait(5000, 1000, 5) == 5);
}

TEST_CASE("The draft puts items in at the caret and sends them in order")
{
    ItemLinkDraft draft;
    std::wstring text = L"a b";
    CHECK(draft.Insert(text, 2, DraftItem(L"[Ring]", L"i13:1:0")) == 8);
    CHECK(draft.Insert(text, 0, DraftItem(L"[Helm]", L"v4:2:7")) == 6);
    CHECK(text == L"[Helm]a [Ring]b");
    const auto placed = draft.Placed();
    REQUIRE(placed.size() == 2);
    CHECK(placed[0].ref == L"v4:2:7");
    CHECK(placed[0].start == 0);
    CHECK(placed[1].ref == L"i13:1:0");
    CHECK(placed[1].start == 8);
    CHECK(*ItemLinkWire::BuildCommand(L"n", text, 0, placed) == L"/il n v4:2:7,i13:1:0 {1}a {2}b");
}

TEST_CASE("The draft follows Backspace on a token and forgets its item")
{
    ItemLinkDraft draft;
    std::wstring text = L"x ";
    draft.Insert(text, 2, DraftItem(L"[Ring]", L"i13:1:0"));
    const auto fix = draft.Follow(L"x [Ring", 7);
    REQUIRE(fix);
    CHECK(fix->text == L"x ");
    CHECK(draft.Empty());
    CHECK(draft.Placed().empty());
}

TEST_CASE("The whisper echo finds each token in the shown line")
{
    ItemLinkDraft draft;
    std::wstring text = L"see ";
    draft.Insert(text, text.size(), DraftItem(L"[Ring]", L"i13:1:0"));
    const auto links = draft.LinksIn(L"[to Friend] see [Ring]");
    REQUIRE(links.size() == 1);
    CHECK(links[0].start == 16);
    CHECK(links[0].length == 6);
    CHECK(links[0].rarity == LinkRarity::Excellent);
}
