#include "UI/HUD/LiveGaugeMath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace UI::HUD::LiveGauge
{
namespace
{
constexpr double TwoPi = 6.283185307179586;

constexpr float EaseTimeConstant = 0.12f;
constexpr float MaxEaseStep = 0.1f;
constexpr float SnapDistance = 0.001f;

constexpr float GhostDropThreshold = 0.02f;
constexpr double GhostHold = 0.35;
constexpr float GhostDrainRate = 1.2f;

constexpr float WaveFadeBand = 0.04f;
constexpr double FirstWaveLength = 0.55;
constexpr double SecondWaveLength = 0.31;
constexpr double FirstWaveWeight = 0.65;
constexpr double SecondWaveWeight = 0.35;
constexpr double SecondWavePeriodShare = 0.7;

constexpr float SloshPerLevel = 12.f;
constexpr float SloshMaxExtra = 1.5f;
constexpr double SloshFade = 0.6;

constexpr float HaloBase = 0.10f;
constexpr float HaloPerLevel = 0.25f;
constexpr float HaloPulseLow = 0.25f;
constexpr float HaloPulseHigh = 0.60f;

constexpr int MillisecondsPerSecond = 1000;
// A cooldown's last whole second shows no number.
constexpr int FirstHiddenSecond = 1;

constexpr float BreathLow = 0.65f;
constexpr float BreathHigh = 1.f;

constexpr float PercentScale = 100.f;
constexpr std::size_t MinOutlinePoints = 3;

constexpr std::size_t RgbDigits = 6;
constexpr std::size_t RgbaDigits = 8;
constexpr std::uint32_t OpaqueAlpha = 0xFFu;

// The fractional part of `cycles`, so a long uptime keeps the sine's precision.
double Fraction(double cycles)
{
    return cycles - std::floor(cycles);
}

bool ParseFloat(const std::string& text, float& value)
{
    const char* begin = text.c_str();
    char* end = nullptr;
    const float parsed = std::strtof(begin, &end);
    if (end == begin || !std::isfinite(parsed))
        return false;
    while (*end == ' ' || *end == '\t')
        ++end;
    if (*end != '\0')
        return false;
    value = parsed;
    return true;
}

bool ParsePoint(const std::string& token, OutlinePoint& point)
{
    const std::size_t comma = token.find(',');
    if (comma == std::string::npos)
        return false;
    float x = 0.f;
    float y = 0.f;
    if (!ParseFloat(token.substr(0, comma), x) || !ParseFloat(token.substr(comma + 1), y))
        return false;
    if (x < 0.f || x > PercentScale || y < 0.f || y > PercentScale)
        return false;
    point = {x / PercentScale, y / PercentScale};
    return true;
}

// Every turn the same way, and some area: each vertical line crosses the outline at most twice.
bool IsConvex(const std::vector<OutlinePoint>& points)
{
    bool turnsLeft = false;
    bool turnsRight = false;
    const std::size_t count = points.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const OutlinePoint& a = points[i];
        const OutlinePoint& b = points[(i + 1) % count];
        const OutlinePoint& c = points[(i + 2) % count];
        const float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
        turnsLeft = turnsLeft || cross > 0.f;
        turnsRight = turnsRight || cross < 0.f;
    }
    return turnsLeft != turnsRight;
}

const std::string* Find(const std::map<std::string, std::string>& section, const char* key)
{
    const auto found = section.find(key);
    return found == section.end() ? nullptr : &found->second;
}

// The key's value when it parses and lies in [low, high]; `value` kept otherwise.
void ReadFloat(const std::map<std::string, std::string>& section, const char* key, float low, float high, float& value)
{
    const std::string* text = Find(section, key);
    float parsed = 0.f;
    if (text != nullptr && ParseFloat(*text, parsed) && parsed >= low && parsed <= high)
        value = parsed;
}

void ReadColour(const std::map<std::string, std::string>& section, const char* key, std::uint32_t& value)
{
    if (const std::string* text = Find(section, key))
        value = ParseColour(*text, value);
}

int HexDigit(char digit)
{
    if (digit >= '0' && digit <= '9')
        return digit - '0';
    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    return -1;
}
} // namespace

std::vector<OutlinePoint> DefaultOutline()
{
    return {{0.25f, 0.03f}, {0.75f, 0.03f}, {0.96f, 0.32f}, {0.96f, 0.70f},
            {0.68f, 0.97f}, {0.32f, 0.97f}, {0.05f, 0.70f}, {0.05f, 0.32f}};
}

std::vector<OutlinePoint> ParseOutline(std::string_view text)
{
    std::istringstream stream{std::string(text)};
    std::vector<OutlinePoint> points;
    std::string token;
    while (stream >> token)
    {
        OutlinePoint point;
        if (!ParsePoint(token, point))
            return DefaultOutline();
        points.push_back(point);
    }
    if (points.size() < MinOutlinePoints || !IsConvex(points))
        return DefaultOutline();
    return points;
}

std::uint32_t ParseColour(std::string_view text, std::uint32_t fallback)
{
    if (text.empty() || text.front() != '#')
        return fallback;
    const std::size_t digits = text.size() - 1;
    if (digits != RgbDigits && digits != RgbaDigits)
        return fallback;
    std::uint32_t channels[4] = {0u, 0u, 0u, OpaqueAlpha};
    for (std::size_t i = 0; i < digits / 2; ++i)
    {
        const int high = HexDigit(text[1 + i * 2]);
        const int low = HexDigit(text[2 + i * 2]);
        if (high < 0 || low < 0)
            return fallback;
        channels[i] = static_cast<std::uint32_t>(high * 16 + low);
    }
    return channels[0] | (channels[1] << 8) | (channels[2] << 16) | (channels[3] << 24);
}

LiveHudConfig ReadLiveHudConfig(const std::map<std::string, std::string>& section)
{
    constexpr float NoLimit = 1.0e6f;
    constexpr float FullTurn = 360.f;
    LiveHudConfig config;
    if (const std::string* outline = Find(section, "gemoutline"))
        config.gemOutline = ParseOutline(*outline);
    // Below 1 the art's edges would show past the silhouette.
    ReadFloat(section, "gemzoom", 1.f, NoLimit, config.gemZoom);
    ReadFloat(section, "emptyshade", 0.f, 1.f, config.emptyShade);
    ReadFloat(section, "waveamplitude", 0.f, NoLimit, config.waveAmplitude);
    ReadFloat(section, "waveperiodlife", 0.01f, NoLimit, config.wavePeriodLife);
    ReadFloat(section, "waveperiodmana", 0.01f, NoLimit, config.wavePeriodMana);
    ReadFloat(section, "arcstart", -FullTurn, FullTurn, config.arcStart);
    ReadFloat(section, "arcsweep", 0.f, FullTurn, config.arcSweep);
    ReadFloat(section, "arcwidth", 0.f, NoLimit, config.arcWidth);
    ReadFloat(section, "lowlife", 0.f, 1.f, config.lowLife);
    ReadFloat(section, "pulseperiod", 0.01f, NoLimit, config.pulsePeriod);
    ReadFloat(section, "pulsestrength", 0.f, 1.f, config.pulseStrength);
    ReadColour(section, "halolife", config.haloLife);
    ReadColour(section, "halomana", config.haloMana);
    ReadColour(section, "halopoison", config.haloPoison);
    ReadColour(section, "sweepshade", config.sweepShade);
    ReadColour(section, "rimouter", config.rimOuter);
    ReadColour(section, "riminner", config.rimInner);
    ReadFloat(section, "rimwidth", 0.f, NoLimit, config.rimWidth);
    ReadFloat(section, "cresttint", 0.f, 1.f, config.crestTint);
    return config;
}

float EaseToward(float shown, float target, float dt)
{
    const float step = std::clamp(dt, 0.f, MaxEaseStep);
    const float next = shown + (target - shown) * (1.f - std::exp(-step / EaseTimeConstant));
    return std::fabs(target - next) < SnapDistance ? target : next;
}

void GhostTracker::Update(float target, float shown, double now)
{
    if (!m_started)
    {
        m_started = true;
        m_lastTarget = target;
        m_shown = shown;
        m_level = shown;
        m_lastTime = now;
        return;
    }

    if (m_lastTarget - target >= GhostDropThreshold)
    {
        // The level shown before this drop, or the ghost already up if a second hit lands while it shows.
        m_level = m_active ? std::max(m_level, m_shown) : m_shown;
        m_active = true;
        m_holdUntil = now + GhostHold;
    }
    else if (m_active && now > m_holdUntil)
    {
        const double drainFrom = std::max(m_lastTime, m_holdUntil);
        m_level -= GhostDrainRate * static_cast<float>(now - drainFrom);
    }

    m_lastTarget = target;
    m_shown = shown;
    m_lastTime = now;
    // Only once the shown level passes it: on the hit's own frame the gem may not have moved yet.
    if (m_active && m_level < shown)
        m_active = false;
    if (!m_active)
        m_level = shown;
}

float GhostTracker::Level() const
{
    return m_active ? std::max(m_level, m_shown) : m_shown;
}

float WaveFade(float level)
{
    const float edge = std::min(level, 1.f - level);
    return std::clamp(edge / WaveFadeBand, 0.f, 1.f);
}

float WaveOffset(float x, double t, float period, float level, float amplitude)
{
    if (period <= 0.f)
        return 0.f;
    const double first = x / FirstWaveLength - Fraction(t / period);
    const double second = x / SecondWaveLength + Fraction(t / (SecondWavePeriodShare * period));
    const double wave = FirstWaveWeight * std::sin(TwoPi * first) + SecondWaveWeight * std::sin(TwoPi * second);
    return amplitude * WaveFade(level) * static_cast<float>(wave);
}

float SloshGain(float delta, double age)
{
    const float extra = std::min(SloshMaxExtra, SloshPerLevel * std::fabs(delta));
    const double fade = std::clamp(1.0 - std::max(age, 0.0) / SloshFade, 0.0, 1.0);
    return 1.f + extra * static_cast<float>(fade);
}

void SloshTracker::Kick(float delta, double now)
{
    if (SloshGain(delta, 0.0) < Gain(now))
        return;
    m_delta = delta;
    m_time = now;
}

float SloshTracker::Gain(double now) const
{
    return SloshGain(m_delta, now - m_time);
}

ArcRange ArcSpan(float level, float startDegrees, float sweepDegrees)
{
    const float share = std::clamp(level, 0.f, 1.f);
    return {startDegrees, startDegrees + share * sweepDegrees};
}

bool IsLowLife(float life, float lowLife)
{
    return life > 0.f && life < lowLife;
}

float PulseLevel(float life, float lowLife, float period, double t)
{
    if (!IsLowLife(life, lowLife) || period <= 0.f)
        return 0.f;
    return static_cast<float>(0.5 + 0.5 * std::sin(TwoPi * Fraction(t / period)));
}

float HaloStrength(float level, bool pulsing, float pulse)
{
    if (pulsing)
        return HaloPulseLow + (HaloPulseHigh - HaloPulseLow) * std::clamp(pulse, 0.f, 1.f);
    return HaloBase + HaloPerLevel * std::clamp(level, 0.f, 1.f);
}

bool ReadyFlash::Update(float cooldownFraction)
{
    const bool ended = m_last > 0.f && cooldownFraction <= 0.f;
    m_last = cooldownFraction;
    return ended;
}

bool LevelUpDetector::Update(std::uint64_t hero, std::int64_t level)
{
    if (!m_started || hero != m_hero)
    {
        m_started = true;
        m_hero = hero;
        m_level = level;
        return false;
    }
    const bool rose = level > m_level;
    m_level = level;
    return rose;
}

int CooldownSeconds(int remainingMs)
{
    if (remainingMs <= 0)
        return 0;
    const int seconds = remainingMs / MillisecondsPerSecond + (remainingMs % MillisecondsPerSecond != 0 ? 1 : 0);
    return seconds > FirstHiddenSecond ? seconds : 0;
}

float SparkBreath(double t, float period)
{
    if (period <= 0.f)
        return BreathHigh;
    const double wave = 0.5 + 0.5 * std::sin(TwoPi * Fraction(t / period));
    return BreathLow + (BreathHigh - BreathLow) * static_cast<float>(wave);
}

float FlareLeft(double age, float duration)
{
    if (age < 0.0 || duration <= 0.f || age >= duration)
        return 0.f;
    const float left = 1.f - static_cast<float>(age / duration);
    return left * left;
}

float RunPhase(double age, float duration)
{
    if (age < 0.0 || duration <= 0.f || age >= duration)
        return -1.f;
    return static_cast<float>(age / duration);
}
} // namespace UI::HUD::LiveGauge
