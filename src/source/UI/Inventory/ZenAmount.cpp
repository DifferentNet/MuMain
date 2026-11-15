#include "UI/Inventory/ZenAmount.h"

#include <algorithm>

namespace
{
// Any amount above this is above every limit, so Parse() stops growing there.
constexpr std::uint64_t kParseCeiling = 99'999'999'999ull;

std::uint32_t AtMostCarried(std::uint64_t amount)
{
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(amount, UI::Inventory::ZenAmount::kMaxCarriedZen));
}
} // namespace

namespace UI::Inventory::ZenAmount
{
std::uint64_t Parse(std::wstring_view text)
{
    std::uint64_t amount = 0;
    for (const wchar_t character : text)
    {
        if (character < L'0' || character > L'9')
            continue;
        amount = std::min(amount * 10 + static_cast<std::uint64_t>(character - L'0'), kParseCeiling);
    }
    return amount;
}

std::uint32_t DepositLimit(std::uint64_t carried)
{
    return AtMostCarried(carried);
}

std::uint32_t WithdrawLimit(std::uint64_t inVault, std::uint64_t carried)
{
    const std::uint32_t room = kMaxCarriedZen - AtMostCarried(carried);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(inVault, room));
}

std::uint32_t TradeLimit(std::uint64_t carried)
{
    return AtMostCarried(carried);
}

std::uint32_t Clamp(std::uint64_t typed, std::uint32_t limit)
{
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(typed, limit));
}
} // namespace UI::Inventory::ZenAmount
