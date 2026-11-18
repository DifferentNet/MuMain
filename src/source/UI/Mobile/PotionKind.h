#pragma once

// The kind of potion bound to a touch potion button (Q, W): the button is tinted by it in the phone
// view (touch_controls.rcss), so the button says what it holds without its key letter.

namespace UI::Mobile
{
// The values are the model fields potion_q_kind / potion_w_kind in touch_controls.rml.
enum class PotionKind : int
{
    None = 0,
    Heal = 1,
    Mana = 2,
    Shield = 3,
    Complex = 4,
    // Something else bound to the button (an antidote, an elixir...): no tint.
    Other = 5,
};

// The kind from what the item type was found to be (GameLogic::Items::Is...PotionType); `bound` is
// false for an empty button. A complex potion heals and restores shield at once, so it wins over the
// single kinds; shield wins over healing and mana.
constexpr PotionKind ClassifyPotion(bool bound, bool heal, bool mana, bool shield, bool complex)
{
    if (!bound)
        return PotionKind::None;
    if (complex)
        return PotionKind::Complex;
    if (shield)
        return PotionKind::Shield;
    if (heal)
        return PotionKind::Heal;
    if (mana)
        return PotionKind::Mana;
    return PotionKind::Other;
}
} // namespace UI::Mobile
