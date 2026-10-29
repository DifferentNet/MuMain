#pragma once

// Web build only. What the page (index.html, checkGpu) found out about the graphics card before it
// started the game.
namespace mu::web
{
// Whether the card has WebGPU's clip-distances feature. Without it (ARM Mali: the Pixel 9 and many
// other Android phones) the renderer takes the soft clip shaders (Render/Renderer/SoftClipShaders.h).
// index.html?softclip says it is missing, to try them on a PC.
bool HasClipDistances();

// Whether the card has WebGPU's depth-clip-control feature, which lets a pipeline clamp depth (what the
// renderer does on every other platform). Without it (ARM Mali Bifrost: a Galaxy Note 10 Lite) the
// pipelines clip depth, as the original OpenGL game did. index.html?nodepthclamp says it is missing.
bool HasDepthClamp();
} // namespace mu::web
