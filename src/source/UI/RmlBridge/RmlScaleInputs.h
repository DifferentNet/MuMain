#pragma once

namespace Rml
{
class Context;
}

// The scales a theme composes its layout from, set once per context on its root element so every
// document inherits them as custom properties:
//   --ui-scale       the UI scale: RmlUi's dp ratio, the panels' and native text's scale
//   --hud-scale      the bottom HUD's scale
//   --dock-scale     the docked windows' scale (a larger cap than the panels')
//   --overlay-scale-x / --overlay-scale-y   the stretched 640x480 screen (world-space overlays)
//   --layout-pixel   window pixels per pixel of a fixed-size scene window (LayoutPixelRatio()):
//                    1 on the desktop; the page's pixel density in the browser, so a window laid
//                    out as calc(118px * var(--layout-pixel, 1)) keeps its size and gets sharper
// base.rcss's .stage and .hud-board place a 640x480 reference screen with them.
namespace UI::RmlBridge
{
void ApplyScaleInputs(Rml::Context* context);
}
