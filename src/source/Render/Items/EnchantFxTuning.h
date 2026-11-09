#pragma once

// Every number of the Remaster enchant look (HD textures on) in one place, to tune after screenshots:
//   +7..+10   the outline light (RimGlow) over the Classic chrome shine,
//   +11..+13  and a slowly pulsing aura in the glow colour around each such piece and the character,
//   +14..+15  and sparks rising from each such piece,
//   and the flash over a character whose item was just enchanted to +13, +14 or +15 ("#UPF" line).
// Classic (HD textures off) uses none of these.
namespace Render::Items::EnchantFx::Tuning
{
// ---- The piece itself (RenderPartObjectEffect in Engine/Object/ZzzObject.cpp) ----

// The piece's own light under the glow passes (Classic: 0.8 at +7/+8, 0.9 from +9).
inline constexpr float BodyLight = 0.95f;

// How strong the Classic chrome passes are drawn under the outline light, by tier (Classic = 1).
inline constexpr float ChromeShareLow = 0.75f;  // +7..+10
inline constexpr float ChromeShareMid = 0.85f;  // +11..+13
inline constexpr float ChromeShareHigh = 0.95f; // +14..+15

// The outline light's brightness by level (Bright of the RENDER_RIM pass; above 1 saturates the glow colour).
inline constexpr float RimStrength[9] = {
    1.40f, // +7
    1.55f, // +8
    1.70f, // +9
    1.85f, // +10
    2.00f, // +11
    2.15f, // +12
    2.30f, // +13
    2.50f, // +14
    2.80f, // +15
};

// The RimGlow texture (Render/Items/RimGlow.cpp): where the outline starts (0 = facing the camera, 1 = outline),
// how sharp it is, and the sweeping band: how many bands per sweep period (the period is set by the shader, about
// 2.8 s), their half-width and brightness.
inline constexpr float OutlineStart = 0.35f;
inline constexpr float OutlinePower = 1.1f;
inline constexpr int BandsPerSweep = 2;
inline constexpr float BandWidth = 0.08f;
inline constexpr float BandBrightness = 1.3f;

// ---- The aura (+11 and up) ----

inline constexpr int AuraMinLevel = 11;
// A light at each such piece (BITMAP_LIGHT sprite scale), by tier: +11..+13, +14..+15.
inline constexpr float AuraPieceScale[2] = {2.2f, 2.8f};
// A big halo around the whole character: scale with one such piece, and with all seven (two weapons, five armour).
inline constexpr float AuraBodyScaleMin = 4.0f;
inline constexpr float AuraBodyScaleMax = 6.5f;
// Brightness of the aura's lights, low and high point of the pulse.
inline constexpr float AuraPulseLow = 0.55f;
inline constexpr float AuraPulseHigh = 1.0f;
inline constexpr float AuraBodyBrightness = 0.8f;
// Pulse speed (radians per millisecond): 0.0025 = one pulse in about 2.5 s.
inline constexpr float AuraPulseSpeed = 0.0025f;
// The light the aura throws on the ground under the character (terrain light, range in tiles).
inline constexpr float AuraGroundLight = 0.7f;
inline constexpr int AuraGroundRange = 2;
// At most this many aura sprites a frame for all characters together (MAX_SPRITES = 1000 for every effect).
inline constexpr int MaxAuraSpritesPerFrame = 260;

// ---- The sparks (+14 and up) ----

inline constexpr int SparkMinLevel = 14;
// Sparks a second from each bone of such a piece, at +14 and +15 (armour pieces have one or two bones, weapons one).
inline constexpr float SparksPerSecond[2] = {4.0f, 8.0f};
// Spark size (BITMAP_FLARE particle, subtype 10: rises 1..5 units a reference frame for 2.4 s, 40 units off the
// bone) and how much white is mixed into the glow colour.
inline constexpr float SparkScale = 0.16f;
inline constexpr float SparkWhite = 0.35f;
inline constexpr float SparkBrightness = 1.0f;
// All characters together emit at most this many sparks a second; past it everyone emits less, evenly. With their
// 2.4 s life that keeps some 530 of the 3,000 particles every effect shares (MAX_PARTICLES).
inline constexpr float MaxSparksPerSecond = 220.0f;

// ---- The flash after an enchant to +13, +14, +15 ----

// Index 0 = +13, 1 = +14, 2 = +15.
inline constexpr float FlashSeconds[3] = {2.0f, 2.5f, 3.2f};
// Fade in and fade out (seconds) inside that time.
inline constexpr float FlashFadeIn = 0.15f;
inline constexpr float FlashFadeOut = 0.8f;
// The column of light: height, spacing of its lights, the core's and the glow's sprite scale and brightness.
inline constexpr float PillarHeight[3] = {480.0f, 600.0f, 760.0f};
inline constexpr float PillarStep = 40.0f;
inline constexpr float PillarCoreScale[3] = {1.5f, 1.8f, 2.2f};
inline constexpr float PillarGlowScale[3] = {3.4f, 4.0f, 4.8f};
inline constexpr float PillarCoreBrightness[3] = {1.2f, 1.3f, 1.4f};
inline constexpr float PillarGlowBrightness[3] = {1.0f, 1.1f, 1.2f};
// The burst at the start: flares spiralling up (level-up joints), a magic circle, and at +15 the master level-up
// swirl too.
inline constexpr int BurstFlares[3] = {14, 18, 24};
inline constexpr int BurstMasterFlares[3] = {0, 10, 19};
// The rune circle on the ground under the column (terrain tiles across) and its brightness.
inline constexpr float GroundCircle[3] = {2.2f, 2.6f, 3.0f};
inline constexpr float GroundCircleBrightness[3] = {1.0f, 1.0f, 1.0f};
// Light thrown on the ground (range in tiles).
inline constexpr float FlashGroundLight[3] = {1.1f, 1.3f, 1.5f};
inline constexpr int FlashGroundRange[3] = {3, 4, 5};
// The colour of the flash when the character wears nothing above +12 any more: gold.
inline constexpr float FlashDefaultColor[3] = {1.0f, 0.8f, 0.35f};
// How much white is mixed into the column's core.
inline constexpr float PillarCoreWhite = 0.6f;
// Play a sound with the burst.
inline constexpr bool FlashSound = true;
} // namespace Render::Items::EnchantFx::Tuning
