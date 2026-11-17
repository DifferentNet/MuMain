#pragma once

#include <string>

// The low frame rate hint: watches steady play in the world and, when the frame rate has been really low for two
// 20-second windows in a row, offers the next lower graphics preset in a toast (UI/HUD/GraphicsHint.h). The Remaster
// client on the desktop only; the Classic client and the browser get nothing. The rules are in
// Core/Performance/LowFpsDetector.h and Render/Look/LowFpsHintPolicy.h; docs/enhanced-look.md, "Low frame rate hint".
namespace Scenes::LowFpsHint
{

// Once per rendered frame, right after CalcFPS().
void OnFrame();

// The developer command `$fpshint force|sim <fps>|sim off|fast|reset|status`. True when `command` was one.
bool HandleDeveloperCommand(const std::wstring& command);

} // namespace Scenes::LowFpsHint
