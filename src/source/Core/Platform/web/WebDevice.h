#pragma once

// Web build only. What the page (index.html, detectDevice) took the device for, before the game
// started: phones and tablets are played with the fingers, so the game sets up its touch controls
// from the first frame. index.html?phone, ?tablet or ?desktop decides it on any device.
namespace mu::web
{
enum class Device
{
    Desktop,
    Tablet,
    Phone,
};

Device GetDevice();

// Phones and tablets.
bool IsTouchDevice();

// "phone", "tablet" or "desktop", for the log.
const char* DeviceName(Device device);
} // namespace mu::web
