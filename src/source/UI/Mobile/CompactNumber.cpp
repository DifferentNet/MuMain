#include "UI/Mobile/CompactNumber.h"

namespace UI::Mobile::CompactNumber
{
namespace
{
constexpr std::uint32_t Thousand = 1'000;
constexpr std::uint32_t Million = 1'000'000;
constexpr char ThousandsSuffix = 'k';
constexpr char MillionsSuffix = 'M';
} // namespace

std::string Format(std::uint32_t value)
{
    if (value < ExactBelow)
        return std::to_string(value);
    if (value < Million)
        return std::to_string(value / Thousand) + ThousandsSuffix;
    return std::to_string(value / Million) + MillionsSuffix;
}
} // namespace UI::Mobile::CompactNumber
