#pragma once

// When the echo (the battle pass mark «Echo», Render/Effects/SeasonEcho.h) takes a new afterimage of a character and
// how long each one stays: a copy every so many units of the path walked or ridden, each fading from 0.6 to nothing,
// a few at a time. Standing still takes none; a jump (a teleport, a warp, a gate) clears the trail. Pure logic: the
// renderer keeps one Trail per character and the poses of its copies by slot.
namespace Render::Effects::EchoTrail
{
// The most copies a trail keeps (the PC's three).
inline constexpr int MaxCopies = 3;
// A new copy's alpha, on the PC and the phones; it fades evenly to 0 over the copy's life.
inline constexpr float StartAlpha = 0.6f;

struct Settings
{
    // Path (world units, 100 a tile) between two copies.
    float spacing = 70.f;
    // How long a copy stays, in milliseconds.
    double lifetimeMs = 450.0;
    // At most this many copies at a time (1 to MaxCopies).
    int maxAlive = MaxCopies;
    // A new copy's alpha; it fades evenly to 0 over its life.
    float startAlpha = StartAlpha;
    // A step longer than this between two frames clears the trail.
    float jump = 300.f;
};

// The PC's trail: a copy every 70 units, 450 ms, three at a time.
constexpr Settings PcSettings()
{
    return {70.f, 450.0, 3, StartAlpha, 300.f};
}

// The phones' (phone view or touch controls): a copy every 110 units, 350 ms, two at a time.
constexpr Settings PhoneSettings()
{
    return {110.f, 350.0, 2, StartAlpha, 300.f};
}

// A copy is taken where its character stands, in the same pose, so at first it lies right on the character. Until
// the character has moved this far (world units) from it, the copy shows only in part, so it doesn't tint the
// character violet or flicker against it.
inline constexpr float Clearance = 30.f;

// The share (0 to 1) of a copy's alpha it shows at `distance` from its character now: none on top of it, growing
// evenly to all from `clearance` on. A clearance of 0 or less always shows all.
[[nodiscard]] float ClearanceFactor(float distance, float clearance = Clearance);

class Trail
{
public:
    // The character is at (x, y) at nowMs (once a frame). Returns the slot (0 to maxAlive - 1) whose copy is taken
    // now, a dead one or else the oldest, or -1 when no copy is due: the first position, too little path since the
    // last copy, or a jump, which also clears the trail.
    int Advance(float x, float y, double nowMs, const Settings& settings);

    // Whether a slot's copy is still there at nowMs.
    [[nodiscard]] bool IsAlive(int slot, double nowMs, const Settings& settings) const;

    // A slot's copy's alpha at nowMs, 0 when it is gone.
    [[nodiscard]] float AlphaOf(int slot, double nowMs, const Settings& settings) const;

    // How many copies are there at nowMs.
    [[nodiscard]] int AliveCount(double nowMs, const Settings& settings) const;

    // Forgets the copies and the last position (dead, cloaked, Chaos Castle, the character gone).
    void Clear();

private:
    struct Copy
    {
        double bornMs = 0.0;
        bool used = false;
    };

    Copy m_copies[MaxCopies]{};
    float m_lastX = 0.f;
    float m_lastY = 0.f;
    bool m_hasLast = false;
    // Path since the last copy.
    float m_path = 0.f;
};
} // namespace Render::Effects::EchoTrail
