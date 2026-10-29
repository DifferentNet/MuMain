// Needs nothing of stdafx.h, so the unit test (tests/network/test_hidden_chat_lines.cpp) compiles it alone.
#include "Network/Server/HiddenChatLines.h"

#include "UI/Core/WindowAccess.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

namespace Network::HiddenChatLines
{
namespace
{
struct Entry
{
    std::wstring_view prefix;
    std::size_t handler = 0; // index in Handlers()
};

// Function-local statics: features register during static initialisation, in no fixed order across files.
std::vector<Handler>& Handlers()
{
    static std::vector<Handler> handlers;
    return handlers;
}

// Every registered prefix, longest first, then alphabetical, then by feature name, so the order never depends
// on the link order. Sorted in Register (start-up only), so Take() never sorts.
std::vector<Entry>& Entries()
{
    static std::vector<Entry> entries;
    return entries;
}

bool IsAskedBefore(const Entry& left, const Entry& right)
{
    if (left.prefix.size() != right.prefix.size())
        return left.prefix.size() > right.prefix.size();
    if (left.prefix != right.prefix)
        return left.prefix < right.prefix;
    return Handlers()[left.handler].name < Handlers()[right.handler].name;
}

bool IsRegistered(std::wstring_view prefix)
{
    const auto& entries = Entries();
    return std::any_of(entries.begin(), entries.end(), [prefix](const Entry& entry) { return entry.prefix == prefix; });
}

void OpenRequestedWindow(const Handler& handler)
{
    if (handler.takeOpenRequest == nullptr || !handler.takeOpenRequest())
        return;

    if (!UI::Windows::IsVisible(handler.window))
        UI::Windows::Show(handler.window);
}

bool IsEnteredWorldBefore(const Handler* left, const Handler* right)
{
    if (left->prefixes.front() != right->prefixes.front())
        return left->prefixes.front() < right->prefixes.front();
    return left->name < right->name;
}
} // namespace

bool Register(Handler handler)
{
    assert(handler.take != nullptr && !handler.prefixes.empty());

    auto& entries = Entries();
    const std::size_t index = Handlers().size();
    for (const std::wstring_view prefix : handler.prefixes)
    {
        assert(!prefix.empty() && !IsRegistered(prefix)); // two features read the same tag
        entries.push_back({prefix, index});
    }

    Handlers().push_back(std::move(handler));
    std::sort(entries.begin(), entries.end(), IsAskedBefore);
    return true;
}

bool Take(const std::wstring& text)
{
    const std::wstring_view view(text);
    for (const Entry& entry : Entries())
    {
        if (!view.starts_with(entry.prefix))
            continue;

        const Handler& handler = Handlers()[entry.handler];
        if (!handler.take(text))
            continue;

        OpenRequestedWindow(handler);
        return true;
    }

    return false;
}

void EnteredWorld()
{
    std::vector<const Handler*> ordered;
    for (const Handler& handler : Handlers())
    {
        if (handler.enteredWorld != nullptr)
            ordered.push_back(&handler);
    }

    std::sort(ordered.begin(), ordered.end(), IsEnteredWorldBefore);
    for (const Handler* handler : ordered)
        handler->enteredWorld();
}
} // namespace Network::HiddenChatLines
