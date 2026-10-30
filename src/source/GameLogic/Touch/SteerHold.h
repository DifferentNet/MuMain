// The joystick's hold across a gate or a map change (touch and pad alike).
//
// A stick held through a gate kept walking the same way on the other side, and the arrival point of
// a gate usually faces the gate back: the hero walked straight back out (Lorencia and the Dungeon
// passed it to each other for as long as the stick was held). A hold that sees the hero land
// somewhere else, on another map or far across this one, waits until the stick is let go or centred.
#pragma once

namespace GameLogic::Touch
{
// Where the hero stands at a moment: the map, the tile, and the time in seconds.
struct HeroPlace
{
    int world = -1;
    int tileX = 0;
    int tileY = 0;
    double seconds = 0.0;
};

// A step farther than any walk covers in that time: a gate, a teleport, a respawn.
[[nodiscard]] bool IsJump(const HeroPlace& from, const HeroPlace& to);

class SteerHold
{
public:
    // A frame the stick asks to walk, with the hero where it stands now: whether it may.
    [[nodiscard]] bool MayWalk(const HeroPlace& place);
    // The controls went away under a held stick (a map change, a death): it waits.
    void Pause();
    // The stick was let go or centred: the next push walks again.
    void Release();
    [[nodiscard]] bool IsWaiting() const { return m_waiting; }
    // Pushed since it was last let go, waiting or not.
    [[nodiscard]] bool IsHeld() const { return m_held; }

private:
    bool m_held = false;
    bool m_waiting = false;
    HeroPlace m_last;
};
} // namespace GameLogic::Touch
