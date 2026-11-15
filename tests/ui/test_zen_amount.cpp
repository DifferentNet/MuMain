#include <doctest.h>
#include "UI/Inventory/ZenAmount.h"

using namespace UI::Inventory::ZenAmount;

TEST_CASE("typed zen reads up to the carry limit and beyond without overflow")
{
    CHECK(Parse(L"") == 0u);
    CHECK(Parse(L"0") == 0u);
    CHECK(Parse(L"12345678") == 12'345'678u);
    CHECK(Parse(L"2000000000") == 2'000'000'000u);
    CHECK(Parse(L"9999999999") == 9'999'999'999ull);
    CHECK(Parse(L"1 000") == 1'000u);
    CHECK(Parse(L"99999999999999999999") > kMaxCarriedZen);
}

TEST_CASE("a deposit is at most the zen carried")
{
    CHECK(DepositLimit(0) == 0u);
    CHECK(DepositLimit(1'500'000'000u) == 1'500'000'000u);
    CHECK(DepositLimit(3'000'000'000ull) == kMaxCarriedZen);
    CHECK(Clamp(Parse(L"2000000000"), DepositLimit(750)) == 750u);
    CHECK(Clamp(Parse(L"500"), DepositLimit(750)) == 500u);
}

TEST_CASE("a withdrawal is at most the vault's zen and the room left to carry")
{
    CHECK(WithdrawLimit(1'000'000'000u, 0) == 1'000'000'000u);
    CHECK(WithdrawLimit(1'000'000'000u, 1'500'000'000u) == 500'000'000u);
    CHECK(WithdrawLimit(1'000'000'000u, kMaxCarriedZen) == 0u);
    CHECK(WithdrawLimit(1'000'000'000u, 2'500'000'000ull) == 0u);
    CHECK(WithdrawLimit(0, 0) == 0u);
    CHECK(WithdrawLimit(3'000'000'000ull, 0) == kMaxCarriedZen);
}

TEST_CASE("a trade offer is at most the zen carried (the server checks it before giving the old offer back)")
{
    CHECK(TradeLimit(100) == 100u);
    CHECK(TradeLimit(0) == 0u);
    CHECK(TradeLimit(2'500'000'000ull) == kMaxCarriedZen);
    CHECK(Clamp(Parse(L"9999999999"), TradeLimit(1'000)) == 1'000u);
}
