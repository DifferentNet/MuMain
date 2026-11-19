#pragma once

// The phone's HUD only (UI::Mobile::PhoneHudLayout): keeps the notices (notices.rml, UI::Notices) under
// the touch controls' open menu and skill picker. It was written when the notices stood over every
// window, the open menu too, and lowered them while the menu was open. Since round 24 their own depth
// lies under every window (RmlStackingOrder.cpp), the open menu included: Sync() then leaves them at
// it and never raises them to the menu's depth, and Release() gives back that same depth.
namespace UI::Mobile::NoticesUnderPanel
{
// Each frame the phone's HUD is laid out; panelOpen = the menu or the picker is open
// (UI::HUD::TouchControls::IsMenuOpen()). Writes only when that changed or the notices were rebuilt.
void Sync(bool panelOpen);
// The notices back at their own depth (the phone's HUD let go, or the controls hidden).
void Release();
} // namespace UI::Mobile::NoticesUnderPanel
