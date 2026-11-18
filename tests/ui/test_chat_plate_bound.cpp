#include "doctest.h"

#include "UI/Chat/ChatPlateBound.h"

#include <array>

using UI::Chat::ChatPlateBound;

namespace
{
struct Plate
{
    float life = 0.f;
};

bool IsIdle(const Plate& plate)
{
    return plate.life <= 0.f;
}

// Every plate still counting down must sit below End(), or the per-frame walks would skip it.
template <std::size_t N>
bool EveryLivePlateBelowEnd(const std::array<Plate, N>& plates, const ChatPlateBound& bound)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        if (!IsIdle(plates[i]) && static_cast<int>(i) >= bound.End())
            return false;
    }
    return true;
}
} // namespace

TEST_CASE("chat plate bound rises on use and trims idle plates at the top")
{
    std::array<Plate, 8> plates{};
    ChatPlateBound bound;
    CHECK(bound.End() == 0);

    plates[2].life = 5.f;
    bound.OnUsed(2);
    plates[5].life = 3.f;
    bound.OnUsed(5);
    CHECK(bound.End() == 6);
    CHECK(EveryLivePlateBelowEnd(plates, bound));

    plates[5].life = 0.f;
    bound.TrimEnd(plates.data(), IsIdle);
    CHECK(bound.End() == 3);
    CHECK(EveryLivePlateBelowEnd(plates, bound));

    // A use below the end leaves it where it is.
    plates[0].life = 1.f;
    bound.OnUsed(0);
    CHECK(bound.End() == 3);

    plates[2].life = 0.f;
    bound.TrimEnd(plates.data(), IsIdle);
    CHECK(bound.End() == 1);

    plates[0].life = -2.f;
    bound.TrimEnd(plates.data(), IsIdle);
    CHECK(bound.End() == 0);
}

TEST_CASE("chat plate bound keeps an idle plate below a live one")
{
    std::array<Plate, 4> plates{};
    ChatPlateBound bound;
    plates[3].life = 1.f;
    bound.OnUsed(3);
    bound.TrimEnd(plates.data(), IsIdle);
    CHECK(bound.End() == 4);
}
