#pragma once

// Web build only. What the page (index.html, checkGpu) found out about the graphics card before it
// started the game.
namespace mu::web
{
// Whether the card has WebGPU's clip-distances feature. Without it (ARM Mali: the Pixel 9 and many
// other Android phones) the renderer takes the soft clip shaders (Render/Renderer/SoftClipShaders.h).
// index.html?softclip says it is missing, to try them on a PC.
bool HasClipDistances();
} // namespace mu::web
