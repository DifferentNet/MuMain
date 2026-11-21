#include "doctest.h"

#include "UI/Tooltip/ItemTooltipLayout.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using UI::RmlBridge::Tooltip::Line;
using UI::RmlBridge::Tooltip::LineColor;
using UI::RmlBridge::Tooltip::LineRole;
using UI::Tooltip::ArrangeRich;
using UI::Tooltip::FitScale;
using UI::Tooltip::RichInput;

namespace
{
Line Text(const std::string& text, LineRole role = LineRole::None, LineColor color = LineColor::White)
{
    Line line;
    line.text = text;
    line.role = role;
    line.color = color;
    return line;
}

Line Half()
{
    Line line;
    line.kind = Line::Kind::HalfSpacer;
    return line;
}

Line Full()
{
    Line line;
    line.kind = Line::Kind::FullSpacer;
    return line;
}

bool IsSpacer(const Line& line)
{
    return line.kind != Line::Kind::Text;
}

// The lines as one string: text lines by their text, spacers as "|".
std::string Shape(const std::vector<Line>& lines)
{
    std::string shape;
    for (const Line& line : lines)
    {
        if (!shape.empty())
            shape += ',';
        shape += IsSpacer(line) ? "|" : line.text;
    }
    return shape;
}

// The texts are views: a literal outlives every arrange.
RichInput Input(std::string_view kind = "", bool detailed = true)
{
    RichInput input;
    input.kind = kind;
    input.detailed = detailed;
    input.hint = "hint";
    return input;
}

// How the item tooltip starts and ends today: a half spacer, the name, a half spacer, stats, blank lines.
std::vector<Line> PlainItem()
{
    return {Half(), Text("Name", LineRole::Name), Half(), Text("a"), Text("b"), Half(), Full(), Text("c"), Half(),
            Half()};
}

std::size_t Count(const std::vector<Line>& lines, LineRole role)
{
    std::size_t count = 0;
    for (const Line& line : lines)
        count += !IsSpacer(line) && line.role == role ? 1 : 0;
    return count;
}
} // namespace

TEST_CASE("blank lines collapse to one half spacer, none at either end [item_tooltip_layout]")
{
    std::vector<Line> lines = PlainItem();
    REQUIRE(ArrangeRich(lines, Input()));
    CHECK(Shape(lines) == "Name,|,a,b,|,c");
    for (const Line& line : lines)
    {
        if (IsSpacer(line))
            CHECK(line.kind == Line::Kind::HalfSpacer);
    }
}

TEST_CASE("the header is the name, the kind and the Combat Power [item_tooltip_layout]")
{
    std::vector<Line> lines = {Half(),    Text("Name", LineRole::Name),      Half(),
                               Text("a"), Text("CP", LineRole::CombatPower), Text("set CP"),
                               Half()};
    REQUIRE(ArrangeRich(lines, Input("Ancient · Armour")));
    CHECK(Shape(lines) == "Name,Ancient · Armour,CP,|,a,set CP");
    CHECK(lines[1].role == LineRole::Kind);

    std::vector<Line> noKind = {Text("Name", LineRole::Name), Text("a"), Text("CP", LineRole::CombatPower)};
    REQUIRE(ArrangeRich(noKind, Input()));
    CHECK(Shape(noKind) == "Name,CP,|,a");

    std::vector<Line> nameOnly = {Text("Name", LineRole::Name), Text("a")};
    REQUIRE(ArrangeRich(nameOnly, Input("Weapons")));
    CHECK(Shape(nameOnly) == "Name,Weapons,|,a");
}

TEST_CASE("the header lines are flagged and the last of them too [item_tooltip_layout]")
{
    std::vector<Line> lines = {Text("Name", LineRole::Name), Text("a"), Text("CP", LineRole::CombatPower)};
    REQUIRE(ArrangeRich(lines, Input("Kind")));
    REQUIRE(lines.size() == 5);
    CHECK(lines[0].head);
    CHECK(lines[1].head);
    CHECK(lines[2].head);
    CHECK_FALSE(lines[0].headLast);
    CHECK_FALSE(lines[1].headLast);
    CHECK(lines[2].headLast);
    CHECK_FALSE(lines[3].head);
    CHECK_FALSE(lines[4].head);
    CHECK_FALSE(lines[4].headLast);

    std::vector<Line> nameOnly = {Text("Name", LineRole::Name)};
    REQUIRE(ArrangeRich(nameOnly, Input()));
    REQUIRE(nameOnly.size() == 1);
    CHECK(nameOnly[0].head);
    CHECK(nameOnly[0].headLast);
}

TEST_CASE("an NPC shop's price lines come first under the header [item_tooltip_layout]")
{
    std::vector<Line> lines = {Half(), Text("Price", LineRole::None, LineColor::Red), Text("Name", LineRole::Name),
                               Half(), Text("a")};
    REQUIRE(ArrangeRich(lines, Input("Kind")));
    CHECK(Shape(lines) == "Name,Kind,|,Price,|,a");
    CHECK(lines[3].color == LineColor::Red);
}

TEST_CASE("one spacer before each section and none right after it [item_tooltip_layout]")
{
    std::vector<Line> lines = {
        Text("Name", LineRole::Name),     Half(),           Text("a"), Half(),
        Text("Set", LineRole::Section),   Half(),           Half(),    Text("opt"),
        Text("PoA", LineRole::Section),   Text("poa line"), Half(),    Text("Pieces", LineRole::Section),
        Text("p1", LineRole::PieceOwned), Half(),           Half()};
    REQUIRE(ArrangeRich(lines, Input()));
    CHECK(Shape(lines) == "Name,|,a,|,Set,opt,|,PoA,poa line,|,Pieces,✓ p1");

    std::vector<Line> sectionFirst = {Text("Name", LineRole::Name), Text("Set", LineRole::Section), Half(),
                                      Text("opt")};
    REQUIRE(ArrangeRich(sectionFirst, Input()));
    CHECK(Shape(sectionFirst) == "Name,|,Set,opt");
}

TEST_CASE("the short view hides 3 or more inactive lines and ends with the hint [item_tooltip_layout]")
{
    const std::vector<Line> source = {Text("Name", LineRole::Name),
                                      Half(),
                                      Text("PoA", LineRole::Section),
                                      Text("active", LineRole::None, LineColor::Green),
                                      Text("d1", LineRole::Detail, LineColor::Gray),
                                      Text("d2", LineRole::Detail, LineColor::Gray),
                                      Half(),
                                      Text("Bonus", LineRole::Section),
                                      Text("d3", LineRole::Detail, LineColor::Gray),
                                      Half(),
                                      Text("Pieces", LineRole::Section),
                                      Text("p1", LineRole::PieceMissing, LineColor::Gray)};

    std::vector<Line> shortView = source;
    REQUIRE(ArrangeRich(shortView, Input("", false)));
    // Bonus had only its inactive line: its header goes with it.
    CHECK(Shape(shortView) == "Name,|,PoA,active,|,Pieces,– p1,hint");
    CHECK(Count(shortView, LineRole::Hint) == 1);
    CHECK(shortView.back().role == LineRole::Hint);
    CHECK(shortView.back().color == LineColor::Gray);

    std::vector<Line> detailed = source;
    REQUIRE(ArrangeRich(detailed, Input("", true)));
    CHECK(Count(detailed, LineRole::Detail) == 3);
    CHECK(Count(detailed, LineRole::Hint) == 0);
    CHECK(Shape(detailed) == "Name,|,PoA,active,d1,d2,|,Bonus,d3,|,Pieces,– p1");
}

TEST_CASE("the short view drops the section headers left with nothing under them [item_tooltip_layout]")
{
    // An ancient piece with nothing of its Power of the Ancients or its full set bonus on.
    const std::vector<Line> source = {Text("Name", LineRole::Name),
                                      Half(),
                                      Text("a"),
                                      Half(),
                                      Text("PoA", LineRole::Section),
                                      Text("d1", LineRole::Detail, LineColor::Gray),
                                      Text("d2", LineRole::Detail, LineColor::Gray),
                                      Text("d3", LineRole::Detail, LineColor::Gray),
                                      Half(),
                                      Full(),
                                      Text("Bonus", LineRole::Section),
                                      Text("d4", LineRole::Detail, LineColor::Gray),
                                      Half(),
                                      Text("Pieces", LineRole::Section),
                                      Text("p1", LineRole::PieceOwned),
                                      Half()};

    std::vector<Line> shortView = source;
    REQUIRE(ArrangeRich(shortView, Input("", false)));
    CHECK(Shape(shortView) == "Name,|,a,|,Pieces,\xE2\x9C\x93 p1,hint");
    CHECK(Count(shortView, LineRole::Section) == 1);
    CHECK(shortView.back().role == LineRole::Hint);

    const std::string once = Shape(shortView);
    REQUIRE(ArrangeRich(shortView, Input("", false)));
    CHECK(Shape(shortView) == once);

    std::vector<Line> detailed = source;
    REQUIRE(ArrangeRich(detailed, Input("", true)));
    CHECK(Shape(detailed) == "Name,|,a,|,PoA,d1,d2,d3,|,Bonus,d4,|,Pieces,\xE2\x9C\x93 p1");

    // The last section emptied: nothing after the lines before it but the hint.
    std::vector<Line> lastEmpty = {Text("Name", LineRole::Name),
                                   Text("a"),
                                   Half(),
                                   Text("PoA", LineRole::Section),
                                   Text("d1", LineRole::Detail),
                                   Text("d2", LineRole::Detail),
                                   Text("d3", LineRole::Detail),
                                   Half()};
    REQUIRE(ArrangeRich(lastEmpty, Input("", false)));
    CHECK(Shape(lastEmpty) == "Name,|,a,hint");
}

TEST_CASE("one or two inactive lines stay, with no hint [item_tooltip_layout]")
{
    std::vector<Line> lines = {Text("Name", LineRole::Name), Text("d1", LineRole::Detail),
                               Text("d2", LineRole::Detail)};
    REQUIRE(ArrangeRich(lines, Input("", false)));
    CHECK(Shape(lines) == "Name,|,d1,d2");
    CHECK(Count(lines, LineRole::Hint) == 0);
}

TEST_CASE("set pieces get a tick or a dash once [item_tooltip_layout]")
{
    std::vector<Line> lines = {Text("Name", LineRole::Name), Text("Helm", LineRole::PieceOwned),
                               Text("Armour", LineRole::PieceMissing)};
    REQUIRE(ArrangeRich(lines, Input()));
    REQUIRE(lines.size() == 4);
    CHECK(lines[2].text == "\xE2\x9C\x93 Helm");
    CHECK(lines[3].text == "\xE2\x80\x93 Armour");

    REQUIRE(ArrangeRich(lines, Input()));
    CHECK(lines[2].text == "\xE2\x9C\x93 Helm");
    CHECK(lines[3].text == "\xE2\x80\x93 Armour");
}

TEST_CASE("without a name line the lines stay as they are [item_tooltip_layout]")
{
    std::vector<Line> lines = {Half(), Text("Repair", LineRole::None),   Half(),
                               Full(), Text("p1", LineRole::PieceOwned), Text("CP", LineRole::CombatPower)};
    const std::string before = Shape(lines);
    CHECK_FALSE(ArrangeRich(lines, Input("Kind", false)));
    CHECK(Shape(lines) == before);
    CHECK(lines.size() == 6);
    CHECK(lines[3].kind == Line::Kind::FullSpacer);
    CHECK_FALSE(lines[5].head);
}

TEST_CASE("arranging an arranged list changes nothing [item_tooltip_layout]")
{
    std::vector<Line> lines = {Half(),
                               Text("Price"),
                               Text("Name", LineRole::Name),
                               Half(),
                               Text("CP", LineRole::CombatPower),
                               Half(),
                               Text("Set", LineRole::Section),
                               Half(),
                               Text("d1", LineRole::Detail),
                               Text("d2", LineRole::Detail),
                               Text("d3", LineRole::Detail),
                               Text("p1", LineRole::PieceOwned),
                               Half()};
    const RichInput input = Input("Kind", false);
    REQUIRE(ArrangeRich(lines, input));
    const std::string once = Shape(lines);
    const std::size_t sizeOnce = lines.size();

    REQUIRE(ArrangeRich(lines, input));
    CHECK(Shape(lines) == once);
    CHECK(lines.size() == sizeOnce);
    CHECK(Count(lines, LineRole::Kind) == 1);
    CHECK(Count(lines, LineRole::Hint) == 1);
    CHECK(lines[2].headLast);
    CHECK_FALSE(lines[3].head);
}

TEST_CASE("a full 80-line tooltip grows by 2 lines at most [item_tooltip_layout]")
{
    // An ancient piece with every block: what RenderItemInfo writes, filled with stat lines to 80.
    std::vector<Line> lines = {
        Half(),         Text("Name", LineRole::Name), Half(), Text("Defense"), Text("CP", LineRole::CombatPower),
        Text("set CP"), Text("If equipped"),          Half()};
    while (lines.size() < 52)
        lines.push_back(Text("stat"));
    const std::vector<Line> tail = {Half(),
                                    Text("Set", LineRole::Section),
                                    Half(),
                                    Half(),
                                    Text("opt1"),
                                    Text("opt2"),
                                    Text("PoA", LineRole::Section),
                                    Text("d1", LineRole::Detail),
                                    Text("d2", LineRole::Detail),
                                    Text("d3", LineRole::Detail),
                                    Half(),
                                    Text("Bonus", LineRole::Section),
                                    Text("b1", LineRole::Detail),
                                    Text("b2", LineRole::Detail),
                                    Text("b3", LineRole::Detail),
                                    Text("b4", LineRole::Detail),
                                    Text("b5", LineRole::Detail),
                                    Half(),
                                    Text("Pieces", LineRole::Section),
                                    Text("p1", LineRole::PieceOwned),
                                    Text("p2", LineRole::PieceMissing),
                                    Text("p3", LineRole::PieceMissing),
                                    Text("p4", LineRole::PieceMissing),
                                    Text("p5", LineRole::PieceMissing),
                                    Text("p6", LineRole::PieceMissing),
                                    Half(),
                                    Half(),
                                    Half()};
    lines.insert(lines.end(), tail.begin(), tail.end());
    REQUIRE(lines.size() == 80);

    for (const bool detailed : {true, false})
    {
        CAPTURE(detailed);
        std::vector<Line> arranged = lines;
        REQUIRE(ArrangeRich(arranged, Input("Ancient · Armour", detailed)));
        CHECK(arranged.size() <= 82);
    }
}

TEST_CASE("a rich tooltip taller than the window shrinks its text to fit, down to the minimum [item_tooltip_layout]")
{
    constexpr float minScale = 0.75f;
    // Fits as it is.
    CHECK(FitScale(500.0f, 66.0f, 757.0f, minScale) == 1.0f);
    CHECK(FitScale(691.0f, 66.0f, 757.0f, minScale) == 1.0f);
    // The part that shrinks takes all the room the fixed part leaves.
    CHECK(FitScale(800.0f, 66.0f, 757.0f, minScale) == doctest::Approx((757.0f - 66.0f) / 800.0f));
    // Never below the minimum: the tooltip then stays too tall.
    CHECK(FitScale(1200.0f, 66.0f, 757.0f, minScale) == minScale);
    CHECK(FitScale(100.0f, 800.0f, 757.0f, minScale) == minScale);
    // Nothing measured, or no window: left as it is.
    CHECK(FitScale(0.0f, 66.0f, 757.0f, minScale) == 1.0f);
    CHECK(FitScale(800.0f, 66.0f, 0.0f, minScale) == 1.0f);
}
