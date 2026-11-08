#pragma once

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
    BMD* m_model = nullptr;
    // The meshes whose texture was swapped, with their own texture.
    std::vector<std::pair<int, std::uint32_t>> m_swapped;
    const void* m_previousColor = nullptr;
    int m_partSet = 0;
};

// The colour of the ancient pulse while a ScopedSetTextures recolours a model; false otherwise.
bool PulseColor(float color[3]);
} // namespace Render::Items::AncientColors
