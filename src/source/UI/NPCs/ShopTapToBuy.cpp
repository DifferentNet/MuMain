#include "UI/NPCs/ShopTapToBuy.h"

namespace UI::Shop::TapToBuy
{
std::optional<std::uint32_t> Chosen(const Choice& choice, Clock::time_point now)
{
    if (!choice.itemKey || now - choice.time > SecondTapTime)
        return std::nullopt;
    return choice.itemKey;
}

Action Tap(Choice& choice, std::uint32_t itemKey, Clock::time_point now)
{
    if (Chosen(choice, now) == itemKey)
    {
        choice = {};
        return Action::Buy;
    }
    choice = {itemKey, now};
    return Action::Choose;
}
} // namespace UI::Shop::TapToBuy
