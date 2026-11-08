#pragma once

class CHARACTER;

// The Remaster enchant look (HD textures on; numbers in Render/Items/EnchantFxTuning.h):
//   +7..+10   the outline light (Render/Items/RimGlow.h) over the Classic chrome shine (RenderPartObjectEffect),
//   +11..+13  and a slowly pulsing aura in the glow colour around each such piece and around the character,
//   +14..+15  and sparks rising from each such piece,
// and a column of light over a character whose item the server says was just enchanted to +13, +14 or +15
// ("#UPF|<key>|<level>", GameLogic/Items/EnchantFlashLine.h). The line stays out of the chat in Classic too;
// Classic draws none of this.
namespace Render::Items::EnchantFx
{
// True when the Remaster look applies: HD textures on.
[[nodiscard]] bool Enabled();

// How strong the Classic chrome passes are drawn under the outline light for an item level.
[[nodiscard]] float ChromeShare(int level);

// The piece's own light factor under the glow passes.
[[nodiscard]] float BodyLight();

// The aura, the sparks and a running flash of a character; called once a frame from RenderCharacter.
void RenderCharacter(CHARACTER* c);
} // namespace Render::Items::EnchantFx
