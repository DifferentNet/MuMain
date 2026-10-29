#pragma once

#include "Core/Globals/InterfaceList.h"

#include <string>
#include <string_view>
#include <vector>

// Our server's add-ons answer the client's windows with blue server messages that start with a tag, "#EVT|…",
// "#TM0|…" and so on. Each feature registers the tags it reads in its own .cpp, with a file-scope
//
//     [[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({...});
//
// and ReceiveNotice only asks Take(), so adding a feature never touches WSclient.cpp. A line no feature takes
// goes to the chat as before.
namespace Network::HiddenChatLines
{
struct Handler
{
    // The feature (its namespace is fine); orders features that share a prefix, which a Debug build refuses.
    std::wstring_view name;
    // Every line the feature takes starts with one of these (usually its own tag constants). Pick tags no other
    // feature uses (grep 'L"#' in src/source); a longer prefix is asked before a shorter one.
    std::vector<std::wstring_view> prefixes;
    // The feature's TakeMessage: true when the line was its own, which then stays out of the chat.
    bool (*take)(const std::wstring& text) = nullptr;
    // For a window an NPC opens (Tier Maps, the auction, the GM panel): after a taken line, true once the server
    // asked to open it. The window is then shown unless it already is.
    bool (*takeOpenRequest)() = nullptr;
    mu::ui::window::INTERFACE_LIST window = mu::ui::window::INTERFACE_BEGIN;
    // Called when a character enters the world (ReceiveJoinMapServer), to forget the previous one's state and
    // ask for the new one's. Optional.
    void (*enteredWorld)() = nullptr;
};

// Adds a feature. Called during static initialisation, so it only stores the handler (a Debug build asserts
// that no other feature has registered one of its prefixes).
bool Register(Handler handler);

// Offers a server message to the registered features. True when one took it (it then stays out of the chat);
// if that feature has a window to open on request, opens it.
bool Take(const std::wstring& text);

// Every registered feature's enteredWorld, in the order of their first prefixes, so the order doesn't depend on
// the link order.
void EnteredWorld();
} // namespace Network::HiddenChatLines
