#include "UI/HUD/LiveHudTest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

namespace UI::HUD::LiveHudTest
{
namespace
{
constexpr double MillisecondsPerSecond = 1000.0;

// The value after a gauge's or the cooldown's name: a finite number, nothing after it but spaces.
std::optional<float> ParseNumber(std::wstring_view text)
{
    const std::wstring copy(text);
    const wchar_t* begin = copy.c_str();
    wchar_t* end = nullptr;
    const float value = std::wcstof(begin, &end);
    if (end == begin || !std::isfinite(value))
        return std::nullopt;
    while (*end == L' ')
        ++end;
    if (*end != L'\0')
        return std::nullopt;
    return value;
}

std::wstring_view Trimmed(std::wstring_view text)
{
    const std::size_t first = text.find_first_not_of(L' ');
    if (first == std::wstring_view::npos)
        return {};
    const std::size_t last = text.find_last_not_of(L' ');
    return text.substr(first, last - first + 1);
}

std::optional<float>* GaugeByName(Overrides& overrides, std::wstring_view name)
{
    if (name == L"hp")
        return &overrides.life;
    if (name == L"mp")
        return &overrides.mana;
    if (name == L"sd")
        return &overrides.shield;
    if (name == L"ag")
        return &overrides.skillMana;
    if (name == L"expfill")
        return &overrides.expFill;
    return nullptr;
}
} // namespace

Overrides& Current()
{
    static Overrides overrides;
    return overrides;
}

bool Apply(Overrides& overrides, std::wstring_view arguments)
{
    const std::wstring_view text = Trimmed(arguments);
    const std::size_t space = text.find(L' ');
    const std::wstring_view name = text.substr(0, space);
    const std::wstring_view value =
        space == std::wstring_view::npos ? std::wstring_view() : Trimmed(text.substr(space));

    if (value.empty())
    {
        if (name == L"off")
            overrides = {};
        else if (name == L"exp")
            ++overrides.expGains;
        else if (name == L"lvl")
            ++overrides.levelUps;
        else if (name == L"stats")
            ++overrides.statsRequests;
        else
            return false;
        return true;
    }

    if (name == L"poison" || name == L"stats")
    {
        if (value != L"on" && value != L"off")
            return false;
        if (name == L"poison")
            overrides.poisoned = value == L"on";
        else
            overrides.statsLog = value == L"on";
        return true;
    }

    const std::optional<float> number = ParseNumber(value);
    if (!number)
        return false;
    if (name == L"cd")
    {
        if (*number < 0.f)
            return false;
        overrides.cooldownSeconds = *number;
        ++overrides.cooldowns;
        return true;
    }
    std::optional<float>* gauge = GaugeByName(overrides, name);
    if (gauge == nullptr || *number < 0.f || *number > 1.f)
        return false;
    *gauge = *number;
    return true;
}

std::optional<CooldownLeft> CooldownPlayback::At(const Overrides& overrides, double now)
{
    if (overrides.cooldowns != m_seen)
    {
        m_seen = overrides.cooldowns;
        m_start = now;
    }
    if (!overrides.cooldownSeconds || *overrides.cooldownSeconds <= 0.f)
        return std::nullopt;
    const double length = *overrides.cooldownSeconds;
    const double left = length - (now - m_start);
    if (left <= 0.0)
        return std::nullopt;
    return CooldownLeft{static_cast<float>(left / length), static_cast<int>(std::ceil(left * MillisecondsPerSecond))};
}

void StatsWindow::Record(double now, int micros, bool ticked, int rebuilds)
{
    if (m_start < 0.0)
    {
        m_start = now;
        return;
    }
    ++m_running.syncs;
    m_running.micros += micros;
    m_running.maxMicros = std::max(m_running.maxMicros, micros);
    if (ticked)
    {
        ++m_running.ticks;
        m_running.rebuilds += rebuilds;
        m_running.maxRebuilds = std::max(m_running.maxRebuilds, rebuilds);
    }
    m_running.seconds = now - m_start;
    if (m_running.seconds < WindowSeconds)
        return;
    m_last = m_running;
    m_hasLast = true;
    m_running = {};
    m_start = now;
    ++m_filled;
}

void StatsWindow::Reset()
{
    m_running = {};
    m_last = {};
    m_hasLast = false;
    m_start = -1.0;
}

std::string FormatStats(const SyncStats& stats)
{
    if (stats.syncs == 0 || stats.seconds <= 0.0)
        return "HUD stats: no syncs yet";
    const double ticks = std::max(stats.ticks, 1);
    char line[192] = {};
    std::snprintf(
        line, sizeof(line),
        "HUD stats over %.1f s: %.1f syncs/s, %.1f ticks/s; mesh rebuilds per tick avg %.1f, max %d; sync avg "
        "%lld us, max %d us",
        stats.seconds, stats.syncs / stats.seconds, stats.ticks / stats.seconds, stats.rebuilds / ticks,
        stats.maxRebuilds, stats.micros / stats.syncs, stats.maxMicros);
    return line;
}
} // namespace UI::HUD::LiveHudTest
