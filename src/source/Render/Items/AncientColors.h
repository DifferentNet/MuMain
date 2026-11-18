#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

class BMD;
class CHARACTER;

// Ancient armour in its set's colour (MU Skynet, ghinzuro 2026-11-07; colours and rules in AncientColorRules.h).
// RenderCharacter names the set of each body part it draws (ScopedPartSet); RenderPartObject then swaps the model's
// textures for recoloured copies while it draws (ScopedSetTextures), and the ancient pulse takes the set's colour
// (PulseColor). The copies are made from the texture files when a set is first drawn, the same on the PC and in the
// browser, and kept for the session. A part whose set isn't known yet keeps the model's own look.
namespace Render::Items::AncientColors
{
// The ancient set of a character's body part (BODYPART_HELM to BODYPART_BOOTS): the own character's from its
// equipment, the others' from the server's line (GameLogic/Items/AncientLook.h). 0 when none or not known yet.
int SetOfBodyPart(const CHARACTER* c, int bodyPart);

// The sets of all body parts of a character at once (index BODYPART_HEAD to BODYPART_BOOTS, 0 for the head and for
// parts without one): the same numbers as SetOfBodyPart, with the armour's set worked out once for the plain pieces
// that borrow it.
constexpr int BodyPartCount = 6;
using BodySets = std::array<int, BodyPartCount>;
BodySets SetsOfBody(const CHARACTER* c);

// The ancient set of an item drawn on its own (a window's preview, an item on the ground), from its model slot and
// its ancient byte. 0 when none.
int SetOfItem(int modelSlot, int ancientDiscriminator);

// While alive, the body part RenderCharacter draws belongs to this set (0: none).
class ScopedPartSet
{
public:
    explicit ScopedPartSet(int set);
    ~ScopedPartSet();
    ScopedPartSet(const ScopedPartSet&) = delete;
    ScopedPartSet& operator=(const ScopedPartSet&) = delete;

private:
    int m_previous;
};

// While alive, the model draws its armour textures in the colour of the set of ScopedPartSet (nothing changes
// outside one, or for a set without a colour).
class ScopedSetTextures
{
public:
    explicit ScopedSetTextures(BMD* model);
    ~ScopedSetTextures();
    ScopedSetTextures(const ScopedSetTextures&) = delete;
    ScopedSetTextures& operator=(const ScopedSetTextures&) = delete;

private:
    using Swap = std::pair<int, std::uint32_t>;
    // Swaps kept without a heap allocation; an armour part has fewer meshes, the rest goes to m_moreSwapped.
    static constexpr int InlineSwaps = 16;

    void Remember(int mesh, std::uint32_t texture);

    BMD* m_model = nullptr;
    // The meshes whose texture was swapped, with their own texture.
    std::array<Swap, InlineSwaps> m_swapped{};
    int m_swappedCount = 0;
    std::vector<Swap> m_moreSwapped;
    const void* m_previousColor = nullptr;
    int m_partSet = 0;
};

// The colour of the ancient pulse while a ScopedSetTextures recolours a model; false otherwise.
bool PulseColor(float color[3]);
} // namespace Render::Items::AncientColors
