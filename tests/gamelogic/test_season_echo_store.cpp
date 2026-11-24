#include "stdafx.h"

#include "doctest.h"

#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/SeasonEchoStore.h"
#include "Network/Server/HiddenChatLines.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

namespace
{
using GameLogic::SeasonEcho::Store;

// Plays a character with this name (the store reads the own name from Hero) and puts the last one back after.
class PlayedHero
{
public:
    explicit PlayedHero(std::wstring_view name) : m_hero(std::make_unique<CHARACTER>()), m_previous(Hero)
    {
        std::fill(std::begin(m_hero->ID), std::end(m_hero->ID), L'\0');
        std::copy(name.begin(), name.end(), m_hero->ID);
        Hero = m_hero.get();
    }

    ~PlayedHero()
    {
        Hero = m_previous;
    }

    PlayedHero(const PlayedHero&) = delete;
    PlayedHero& operator=(const PlayedHero&) = delete;

private:
    std::unique_ptr<CHARACTER> m_hero;
    CHARACTER* m_previous;
};
} // namespace

TEST_CASE("a shown line about the hero shows its echo, but owns none")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.TakeMessage(L"#BPE|Alice|2"));
    CHECK(store.ShownSeasonOf(L"Alice") == 2);
    CHECK(store.OwnShownSeason() == 2);
    CHECK(store.OwnedSeason() == 0);
    CHECK_FALSE(store.OwnHidden());
}

TEST_CASE("the owner line wins over a shown line about the hero")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.TakeMessage(L"#BPE|Alice|2"));
    CHECK(store.TakeMessage(L"#BPE|Alice|2|1"));
    CHECK(store.OwnedSeason() == 2);
    CHECK(store.OwnHidden());
    CHECK(store.OwnShownSeason() == 0);
    // The shown line itself is kept as it came.
    CHECK(store.ShownSeasonOf(L"Alice") == 2);

    CHECK(store.TakeMessage(L"#BPE|Alice|2|0"));
    CHECK(store.OwnedSeason() == 2);
    CHECK_FALSE(store.OwnHidden());
    CHECK(store.OwnShownSeason() == 2);

    // A shown line that came after the owner line doesn't take over either.
    CHECK(store.TakeMessage(L"#BPE|Alice|0"));
    CHECK(store.OwnShownSeason() == 2);
    CHECK(store.TakeMessage(L"#BPE|Alice|2|1"));
    CHECK(store.TakeMessage(L"#BPE|Alice|3"));
    CHECK(store.OwnShownSeason() == 0);
}

TEST_CASE("an owner line with season 0 shows nothing, even with a shown line")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.TakeMessage(L"#BPE|Alice|2"));
    CHECK(store.TakeMessage(L"#BPE|Alice|0|0"));
    CHECK(store.OwnedSeason() == 0);
    CHECK_FALSE(store.OwnHidden());
    CHECK(store.OwnShownSeason() == 0);
}

TEST_CASE("an owner line about another character is not the hero's")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.TakeMessage(L"#BPE|Alice|2"));
    CHECK(store.TakeMessage(L"#BPE|Bob|3|1"));
    CHECK(store.OwnedSeason() == 0);
    CHECK_FALSE(store.OwnHidden());
    CHECK(store.OwnShownSeason() == 2);
}

TEST_CASE("season 0 on a shown line takes the echo away")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.TakeMessage(L"#BPE|Bob|2"));
    CHECK(store.ShownSeasonOf(L"Bob") == 2);
    CHECK(store.TakeMessage(L"#BPE|Bob|0"));
    CHECK(store.ShownSeasonOf(L"Bob") == 0);
    CHECK(store.OthersShownCount() == 0);

    // Taking away one that was never shown is fine too.
    CHECK(store.TakeMessage(L"#BPE|Carol|0"));
    CHECK(store.ShownSeasonOf(L"Carol") == 0);
}

TEST_CASE("the others' count leaves out the hero")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK(store.OthersShownCount() == 0);
    CHECK(store.TakeMessage(L"#BPE|Alice|2"));
    CHECK(store.OthersShownCount() == 0);
    CHECK(store.TakeMessage(L"#BPE|Bob|2"));
    CHECK(store.TakeMessage(L"#BPE|Carol|1"));
    CHECK(store.OthersShownCount() == 2);
    CHECK(store.TakeMessage(L"#BPE|Alice|0"));
    CHECK(store.OthersShownCount() == 2);
}

TEST_CASE("only echo lines are taken, a broken one stays out of the chat but is not kept")
{
    const PlayedHero hero(L"Alice");
    Store store;

    CHECK_FALSE(store.TakeMessage(L"hello"));
    CHECK_FALSE(store.TakeMessage(L"#BPM|Alice|2"));
    const unsigned before = store.Version();
    CHECK(store.TakeMessage(L"#BPE|Alice|x"));
    CHECK(store.TakeMessage(L"#BPE|Alice|2|7"));
    CHECK(store.Version() == before);
    CHECK(store.OwnShownSeason() == 0);
    CHECK(store.OwnedSeason() == 0);
}

TEST_CASE("every change grows the version")
{
    const PlayedHero hero(L"Alice");
    Store store;

    unsigned version = store.Version();
    CHECK(store.TakeMessage(L"#BPE|Bob|2"));
    CHECK(store.Version() > version);
    version = store.Version();
    CHECK(store.TakeMessage(L"#BPE|Alice|2|0"));
    CHECK(store.Version() > version);
    version = store.Version();
    store.Reset();
    CHECK(store.Version() > version);
}

TEST_CASE("the chat lines reach the store, and entering the world forgets them")
{
    const PlayedHero hero(L"Alice");
    auto& echoes = GameLogic::SeasonEcho::Echoes();
    echoes.Reset();

    CHECK(Network::HiddenChatLines::Take(L"#BPE|Bob|2"));
    CHECK(Network::HiddenChatLines::Take(L"#BPE|Alice|2|0"));
    CHECK(echoes.ShownSeasonOf(L"Bob") == 2);
    CHECK(echoes.OwnedSeason() == 2);
    CHECK(echoes.OwnShownSeason() == 2);

    const unsigned before = echoes.Version();
    Network::HiddenChatLines::EnteredWorld();
    CHECK(echoes.Version() > before);
    CHECK(echoes.ShownSeasonOf(L"Bob") == 0);
    CHECK(echoes.OthersShownCount() == 0);
    CHECK(echoes.OwnedSeason() == 0);
    CHECK_FALSE(echoes.OwnHidden());
    CHECK(echoes.OwnShownSeason() == 0);
}
