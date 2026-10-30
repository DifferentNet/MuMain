// A tap on one of the touch controls' action buttons (attack, a skill, pick up) owes its action
// once, until it is done.
//
// The buttons used to act only while a finger or the pad held them, so a tap could do nothing: one
// shorter than a frame on a slow phone, one that came while the hero was finishing a swing or
// between two paced actions, and a tap on a monster out of reach took two steps towards it and
// stopped. The tapped action is now carried on until it went off or cannot (no target, refused),
// for a few seconds at most; a newer tap on another button takes its place.
#pragma once

namespace UI::Mobile
{
class FollowThrough
{
public:
    static constexpr int None = -1;
    // How long an action that keeps waiting (a skill on cooldown, a monster out of reach) is owed.
    static constexpr double MaxSeconds = 4.0;

    // A press on the control at that time: its action is owed from now on.
    void Press(int control, double seconds);
    // The control whose action is owed at that time, or None.
    [[nodiscard]] int Owed(double seconds) const;
    // The control's action went off, or never will: it is no longer owed.
    void Settle(int control);
    // Nothing is owed any more (the world was left, a panel opened over the buttons).
    void Drop();

private:
    int m_control = None;
    double m_since = 0.0;
};
} // namespace UI::Mobile
