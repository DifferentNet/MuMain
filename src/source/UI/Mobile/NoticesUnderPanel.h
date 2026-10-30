#pragma once

// The phone's HUD only (UI::Mobile::PhoneHudLayout): the notices (notices.rml, UI::Notices) go under
// the touch controls' open menu and skill picker, and back over every window once they shut. The
// notices stand over every window, the open menu too, so on a phone's small screen their lines lay
// over the menu's buttons and the picker's skills and read mixed with them. Under the menu they stay
// in view, dimmed by its backdrop like the world, and still over every other window.
namespace UI::Mobile::NoticesUnderPanel
{
// Each frame the phone's HUD is laid out; panelOpen = the menu or the picker is open
// (UI::HUD::TouchControls::IsMenuOpen()). Writes only when that changed or the notices were rebuilt.
void Sync(bool panelOpen);
// The notices back at their own depth (the phone's HUD let go, or the controls hidden).
void Release();
} // namespace UI::Mobile::NoticesUnderPanel
