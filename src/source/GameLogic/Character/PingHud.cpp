#include "stdafx.h"
#include "GameLogic/Character/PingHud.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <array>
#include <cwchar>

namespace GameLogic::PingHud
{
namespace
{
constexpr std::wstring_view Prefix = L"#PNG|";
constexpr const wchar_t* Command = L"/pinghud";
// The server pings every 10 s and counts a ping lost after 30 s; without a line for this long the label greys out.
constexpr auto StaleAfter = std::chrono::seconds(35);

// A field in whole milliseconds, or "-" (unknown): -1. False for anything else.
bool ReadMs(std::wstring_view field, int& value)
{
    if (field == L"-")
    {
        value = -1;
        return true;
    }
    if (field.empty() || field.size() > 6)
        return false;
    int number = 0;
    for (const wchar_t c : field)
    {
        if (c < L'0' || c > L'9')
            return false;
        number = number * 10 + (c - L'0');
    }
    value = number;
    return true;
}
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::Parse(std::wstring_view text, Reading& reading)
{
    if (!text.starts_with(Prefix))
        return false;

    std::array<std::wstring_view, 4> fields{};
    size_t start = Prefix.size();
    size_t count = 0;
    while (count < fields.size())
    {
        const size_t bar = text.find(L'|', start);
        fields[count++] = text.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start);
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }
    if (count < fields.size())
        return false;

    Reading parsed;
    if (!ReadMs(fields[0], parsed.lastMs) || !ReadMs(fields[1], parsed.medianMs) || !ReadMs(fields[3], parsed.networkMs))
        return false;
    if (fields[2] != L"d" && fields[2] != L"b")
        return false;
    parsed.browser = fields[2] == L"b";
    reading = parsed;
    return true;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (!view.starts_with(Prefix))
        return false;

    // A broken line from a newer server is still ours: it stays out of the chat.
    Reading reading;
    if (Parse(view, reading))
    {
        m_reading = reading;
        m_received = std::chrono::steady_clock::now();
    }
    return true;
}

void Store::Request()
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(Command));
}

void Store::Reset()
{
    m_reading = {};
    m_received = {};
}

bool Store::IsStale() const
{
    return HasReading() && std::chrono::steady_clock::now() - m_received > StaleAfter;
}

namespace
{
// A new character: drop the last one's numbers and ask for this one's lines.
void ForgetAndAsk()
{
    Ping().Reset();
    Ping().Request();
}

// The ping lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"PingHud",
    .prefixes = {Prefix},
    .take = [](const std::wstring& text) { return Ping().TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::PingHud
