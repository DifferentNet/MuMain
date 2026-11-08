#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The look preset ([Render] Look).
enum class LookPreset : std::uint8_t
{
    Classic,
    Enhanced,
};

// The graphics preset ([Render] GraphicsPreset). Auto only means "not chosen yet": start-up replaces it
// with the preset that suits the GPU. Custom is any mix of values that matches no preset.
enum class GraphicsPreset : std::uint8_t
{
    Auto,
    Low,
    Medium,
    High,
    Ultra,
    Custom,
};

namespace GameConfigValidation
{
int ValidateServerPort(int value, int defaultPort);
std::wstring ValidateServerIP(const std::wstring& value, const std::wstring& defaultIP);
// At least 1 (no anisotropy). The upper bound is what the GPU supports and is
// applied by the renderer, which logs when it clamps.
int ValidateAnisotropy(int value);
// 0 (off), 2, 4 or 8: other values round down to the nearest of those, 1 and
// below are off, above 8 is 8. What the GPU supports is applied by the
// renderer, which logs the count it uses.
int ValidateAntialiasing(int value);
// "Classic" or "Enhanced" in any case, surrounding blanks ignored; nothing otherwise.
std::optional<LookPreset> ParseLook(std::wstring_view text);
// The text ParseLook reads back as `preset`.
const wchar_t* FormatLook(LookPreset preset);
// A [Render] Look value, or the default when it is missing or invalid.
LookPreset ValidateLook(std::wstring_view text);

// "Auto", "Low", "Medium", "High", "Ultra" or "Custom" in any case, surrounding blanks ignored; nothing otherwise.
std::optional<GraphicsPreset> ParseGraphicsPreset(std::wstring_view text);
// The text ParseGraphicsPreset reads back as `preset`.
const wchar_t* FormatGraphicsPreset(GraphicsPreset preset);
// A [Render] GraphicsPreset value, or Auto when it is missing or invalid.
GraphicsPreset ValidateGraphicsPreset(std::wstring_view text);
// [Render] Sharpen in whole percent: 0 (off) to 100.
int ValidateSharpen(int value);
// [Render] SunShadows: 0 (off), 1 (medium) or 2 (high).
int ValidateSunShadows(int value);

// The window size for a new installation ([Window] Width and Height 0): the largest of `modes` (width,
// height) that fits the usable area of the screen, or a share of the usable area when none fits.
std::pair<int, int> ChooseWindowSize(const std::vector<std::pair<int, int>>& modes, int usableWidth, int usableHeight);

// Trims whitespace, lowercases, and expands the "d3d12" alias to "direct3d12". Does NOT reject
// unrecognized values -- an unknown string is passed through as-is and left for
// SDL_CreateGPUDevice's own fail-and-fallback path to handle safely.
std::wstring ValidateRenderBackend(const std::wstring& value, const std::wstring& defaultValue);
}
