#include "GameLogic/Events/GrandShop.h"

#include "doctest.h"

#include <string>

namespace
{
using GameLogic::GrandShop::Kind;
using GameLogic::GrandShop::Store;

// The server's answer to /grshop: a header, a classic piece, a high-tier piece and the jewel.
void SendShop(Store& store)
{
    CHECK(store.TakeMessage(L"#GSO|120|2|1"));
    CHECK(store.TakeMessage(L"#GS|0.3.1|0|0|3|1|10|Hyon"));
    CHECK(store.TakeMessage(L"#GS|7.12.2|1|7|12|2|20|Anubis Of Light"));
    CHECK(store.TakeMessage(L"#GS|jewel|2|14|160|0|15|Jewel of Ancient"));
    CHECK(store.TakeMessage(L"#GS.|"));
}
} // namespace

TEST_CASE("the shop lines are taken and kept once the footer comes")
{
    Store store;
    SendShop(store);

    REQUIRE(store.HasInfo());
    CHECK(store.GetStatus().Marks == 120);
    CHECK(store.GetStatus().GrandResets == 2);
    CHECK(store.GetStatus().SafeZone);
    CHECK(store.FailText().empty());
    REQUIRE(store.Lines().size() == 3);

    const auto& classic = store.Lines()[0];
    CHECK(classic.Key == L"0.3.1");
    CHECK(classic.LineKind == Kind::Classic);
    CHECK(classic.Group == 0);
    CHECK(classic.Number == 3);
    CHECK(classic.SetType == 1);
    CHECK(classic.Price == 10);
    CHECK(classic.SetName == L"Hyon");

    CHECK(store.Lines()[1].LineKind == Kind::HighTier);
    CHECK(store.Lines()[1].SetType == 2);
    CHECK(store.Lines()[2].LineKind == Kind::Jewel);
    CHECK(store.Lines()[2].Key == L"jewel");
    CHECK(store.Lines()[2].Price == 15);
}

TEST_CASE("the open request comes once")
{
    Store store;
    CHECK_FALSE(store.TakeOpenRequest());
    CHECK(store.TakeMessage(L"#GSO|5|0|0"));
    CHECK(store.TakeOpenRequest());
    CHECK_FALSE(store.TakeOpenRequest());
}

TEST_CASE("nothing is shown until the footer, and a new header starts the list over")
{
    Store store;
    CHECK(store.TakeMessage(L"#GSO|5|0|0"));
    CHECK(store.TakeMessage(L"#GS|0.3.1|0|0|3|1|10|Hyon"));
    CHECK_FALSE(store.HasInfo());

    CHECK(store.TakeMessage(L"#GS.|"));
    REQUIRE(store.HasInfo());
    CHECK(store.Lines().size() == 1);

    // A second shop replaces the first; lines before a header are dropped.
    CHECK(store.TakeMessage(L"#GSO|9|1|1"));
    CHECK(store.TakeMessage(L"#GS|0.4.2|0|0|4|2|10|Hyon"));
    CHECK(store.TakeMessage(L"#GS.|"));
    REQUIRE(store.Lines().size() == 1);
    CHECK(store.Lines()[0].Key == L"0.4.2");
    CHECK(store.GetStatus().Marks == 9);
}

TEST_CASE("a malformed line is taken out of the chat but kept out of the list")
{
    Store store;
    CHECK(store.TakeMessage(L"#GSO|5|0|0"));
    CHECK(store.TakeMessage(L"#GS|short|0|0"));
    CHECK(store.TakeMessage(L"#GS|0.3.1|9|0|3|1|10|Unknown kind"));
    CHECK(store.TakeMessage(L"#GS|0.5.1|0|0|5|1|10|Set|With A Bar"));
    CHECK(store.TakeMessage(L"#GS.|"));

    REQUIRE(store.Lines().size() == 1);
    // The set's name comes last, so a bar in it stays.
    CHECK(store.Lines()[0].SetName == L"Set|With A Bar");
}

TEST_CASE("a refusal keeps the window without data and carries the text")
{
    Store store;
    SendShop(store);
    CHECK(store.TakeMessage(L"#GS!|The shop is closed during events."));

    CHECK_FALSE(store.HasInfo());
    CHECK(store.Lines().empty());
    CHECK(store.FailText() == L"The shop is closed during events.");
    CHECK(store.TakeOpenRequest());

    // The next shop clears it.
    SendShop(store);
    CHECK(store.FailText().empty());
    CHECK(store.HasInfo());
}

TEST_CASE("a purchase answer updates the marks and carries its text")
{
    Store store;
    SendShop(store);
    const int before = store.Answers();

    CHECK(store.TakeMessage(L"#GSK|1|110|Bought: Hyon Helm | bound"));
    CHECK(store.LastAnswerSucceeded());
    CHECK(store.GetStatus().Marks == 110);
    CHECK(store.LastAnswerText() == L"Bought: Hyon Helm | bound");
    CHECK(store.Answers() == before + 1);

    CHECK(store.TakeMessage(L"#GSK|0|110|Not enough Grand Marks."));
    CHECK_FALSE(store.LastAnswerSucceeded());
    CHECK(store.LastAnswerText() == L"Not enough Grand Marks.");
    CHECK(store.Answers() == before + 2);
    CHECK_FALSE(store.IsBusy());
}

TEST_CASE("other lines are not the shop's")
{
    Store store;
    CHECK_FALSE(store.TakeMessage(L"#GSX|1|2"));
    CHECK_FALSE(store.TakeMessage(L"#RNK|1|text"));
    CHECK_FALSE(store.TakeMessage(L"Grand reset done"));
    CHECK_FALSE(store.HasInfo());
}

TEST_CASE("reset forgets the shop")
{
    Store store;
    SendShop(store);
    store.Reset();

    CHECK_FALSE(store.HasInfo());
    CHECK(store.Lines().empty());
    CHECK(store.GetStatus().Marks == 0);
    CHECK_FALSE(store.TakeOpenRequest());
}
