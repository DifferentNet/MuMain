#pragma once

class CHARACTER;
class OBJECT;

// The battle pass mark «Echo» (MU Skynet, ghinzuro 2026-11-22, variant 3; Season 2 «Winter II», level 50): while a
// character that shows it walks or rides, frozen copies of its pose (body, weapons, wings and mount) stay behind it
// along its path, tinted the season's colour (Data/Seasons.json "echo", GameLogic::SeasonMarks::EchoColorOf), and
// fade. When a copy is due is EchoTrail's rule (Render/Effects/EchoTrail.h): every 70 units of path on the PC, 450 ms
// each, three at a time; on phones every 110 units, 350 ms, two at a time. Who shows one comes from the server's
// "#BPE" lines (GameLogic/Events/SeasonEchoStore.h).
//
// A copy is a snapshot, not a second animation: as a character is drawn the game tells this which models it drew
// with which bones and which meshes it hid or glowed with (BeginCharacter, NoteBodyPart, NoteLinkedItem,
// NoteLinkedItemDrawn, NoteMount), and on the frame a copy is due they are copied. Render() then draws every live copy
// in its own pass after the opaque world and its ground, tested against the world's depth but writing none, with no
// lighting: an opaque mesh as a translucent flat silhouette in the season's colour, a cut-out one (a texture with an
// alpha channel) as its texture tinted so, and the meshes the live model draws in light (its BlendMesh, a texture
// script's bright mesh: most wings) added in the season's colour. On the PC at render level 2 or more an additive
// layer goes over it: the opaque meshes' textures in the season's colour, so the gear's detail shows, and the meshes
// drawn in light once more. No item glow passes, particles, cloth, capes, shadow, sun shadow, bloom, ambient
// occlusion, haze or picking. Only riding mounts are copied (Uniria, Dinorant, Dark Horse, the Fenrirs), never pets.
//
// The own echo always shows while the owner keeps it on (Titles window). Other players' only with Options, Interface:
// "Other players' echoes" (config.ini [UI] OtherEchoes; no key: on on the PC, off on phones) at render level 2 or
// more, the nearest 8 within 12 tiles on the PC, 3 on phones. Never in Chaos Castle, cloaked, dead or transformed,
// and only in the game (MAIN_SCENE).
namespace Render::Effects::SeasonEcho
{
// RenderCharacter, once the character's bones are ready and its ground shadow was drawn: the echo's trail moves on
// and, when a copy is due, the models drawn next are copied into it.
void BeginCharacter(CHARACTER* c, OBJECT* o);

// RenderCharacter's body loop, before each part is drawn: the part's model (as RenderPartObject gets it) with the
// character's own bones.
void NoteBodyPart(const OBJECT* o, int model, int level, bool translate);

// RenderLinkObject, right after a weapon, wing or other linked item was transformed (not for capes): `model` with
// the bones, origin and scale it was transformed with.
void NoteLinkedItem(const OBJECT* o, int model);

// RenderLinkObject, right after that item was drawn: the mesh it hid and the ones it glowed with (`item`'s HiddenMesh
// and BlendMesh, which its item effect may set while it is drawn).
void NoteLinkedItemDrawn(const OBJECT* o, const OBJECT* item);

// RenderMount, right after the mount was transformed (Calc_RenderObject), before it is drawn. Riding mounts only.
void NoteMount(const OBJECT* mount);

// Draws the live copies and picks who shows one next frame; MainScene, once a frame after the opaque world and its
// ground (right after EndOpaqueWorld, on water-terrain maps right after RenderWaterTerrain).
void Render();

// Whether other players' echoes show: config.ini [UI] OtherEchoes, else on on the PC and off on phones.
[[nodiscard]] bool OtherEchoesOn();
// Saves the Options switch (written to config.ini only once the player sets it).
void SetOtherEchoes(bool shown);
} // namespace Render::Effects::SeasonEcho
