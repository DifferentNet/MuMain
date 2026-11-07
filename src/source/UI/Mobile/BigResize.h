#pragma once

// Whether a window resize is big enough to drop the held fingers (TouchInput Reset).
// A phone browser's address bar sliding in or out changes the height by a few percent, and a
// player holding the stick should keep walking; a turn of the phone or a much bigger change moves
// every on-screen control, so a held finger no longer sits on what it pressed.
namespace UI::Mobile
{
bool IsBigResize(int oldWidth, int oldHeight, int newWidth, int newHeight);
} // namespace UI::Mobile
