#include "stdafx.h"
#include "GameLogic/Items/ExcellentDust.h"

#include "GameLogic/Events/EndlessTowerLines.h"
#include "I18N/All.h"
#include "Network/Server/HiddenChatLines.h"

#include <chrono>
#include <cstdarg>

namespace GameLogic::ExcellentDust
{
namespace
{
using Clock = std::chrono::steady_clock;

constexpr std::wstring_view InfoLine = L"#EXD|";
constexpr std::wstring_view MissingLine = L"#EXD!|";
constexpr std::wstring_view InfoCommand = L"/dustinfo";
constexpr size_t InfoFields = 6;
// Field 6, the dust added at the reset, came with the carry-over (server 2026-11-06); older servers send 6 fields.
constexpr size_t CarriedField = 6;
// Repeated requests within this time are dropped (the menu and the machine both ask).
constexpr auto RequestInterval = std::chrono::seconds(1);
constexpr long long SecondsPerMinute = 60;
constexpr long long SecondsPerHour = 60 * SecondsPerMinute;
constexpr long long SecondsPerDay = 24 * SecondsPerHour;

struct State
{
    bool known = false;
    bool available = false;
    long long dust = 0;
    long long earned = 0;
    long long limit = 0;
    long long cost = 0;
    long long carried = 0;
    Clock::time_point resetAt{};
    Clock::time_point lastRequest{};
    bool dustMode = false;
};

State& Current()
{
    static State state;
    return state;
}

std::wstring Thousands(long long value)
{
    std::wstring digits = std::to_wstring(value < 0 ? -value : value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L",");
    return value < 0 ? L"-" + digits : digits;
}

std::wstring Printf(const wchar_t* format, ...)
{
    wchar_t text[256] = {};
    va_list args;
    va_start(args, format);
    vswprintf(text, std::size(text), format, args);
    va_end(args);
    return text;
}

// "2d 5h" while a day or more is left, else "5h 12m".
std::wstring TimeLeft(long long seconds)
{
    if (seconds < 0)
        seconds = 0;
    const int days = static_cast<int>(seconds / SecondsPerDay);
    const int hours = static_cast<int>((seconds % SecondsPerDay) / SecondsPerHour);
    if (days > 0)
        return Printf(I18N::Game::ExcellentDustDaysHours, days, hours);
    const int minutes = static_cast<int>((seconds % SecondsPerHour) / SecondsPerMinute);
    return Printf(I18N::Game::ExcellentDustHoursMinutes, hours, minutes);
}

bool TakeMessage(const std::wstring& text)
{
    auto& state = Current();
    if (text.starts_with(MissingLine))
    {
        state.known = true;
        state.available = false;
        return true;
    }
    if (!text.starts_with(InfoLine))
        return false;

    const auto fields = EndlessTower::Split(text);
    if (fields.size() < InfoFields)
        return true; // ours, but cut short: kept out of the chat, the counter stays as it was
    state.known = true;
    state.available = true;
    state.dust = EndlessTower::ToLong(fields[1]);
    state.earned = EndlessTower::ToLong(fields[2]);
    state.limit = EndlessTower::ToLong(fields[3]);
    state.resetAt = Clock::now() + std::chrono::seconds(EndlessTower::ToLong(fields[4]));
    state.cost = EndlessTower::ToLong(fields[5]);
    state.carried = fields.size() > CarriedField ? EndlessTower::ToLong(fields[CarriedField]) : 0;
    return true;
}

void EnteredWorld()
{
    // Another character may be on another account: its dust comes with the next request.
    Current() = {};
}

// The dust counter's lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"ExcellentDust",
    .prefixes = {InfoLine, MissingLine},
    .take = TakeMessage,
    .enteredWorld = EnteredWorld,
});
} // namespace

void Request(bool force)
{
    auto& state = Current();
    const auto now = Clock::now();
    if (!force && state.lastRequest != Clock::time_point{} && now - state.lastRequest < RequestInterval)
        return;
    if (!EndlessTower::CanSendCommand())
        return;
    state.lastRequest = now;
    EndlessTower::SendCommand(std::wstring(InfoCommand));
}

bool IsDustMode() { return Current().dustMode; }

void SetDustMode(bool on) { Current().dustMode = on; }

std::wstring CounterLine()
{
    const auto& state = Current();
    if (!state.known)
        return I18N::Game::ExcellentDustWaiting;
    if (!state.available)
        return I18N::Game::ExcellentDustUnavailable;
    return Printf(I18N::Game::ExcellentDustCounter, Thousands(state.dust).c_str(), Thousands(state.cost).c_str());
}

std::wstring WeekLine()
{
    const auto& state = Current();
    if (!state.known || !state.available)
        return {};
    const auto left = std::chrono::duration_cast<std::chrono::seconds>(state.resetAt - Clock::now()).count();
    if (state.carried > 0)
        return Printf(I18N::Game::ExcellentDustWeekCarried, Thousands(state.earned).c_str(), Thousands(state.limit).c_str(),
                      Thousands(state.carried).c_str(), TimeLeft(left).c_str());
    return Printf(I18N::Game::ExcellentDustWeek, Thousands(state.earned).c_str(), Thousands(state.limit).c_str(),
                  TimeLeft(left).c_str());
}
} // namespace GameLogic::ExcellentDust
