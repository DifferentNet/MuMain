#pragma once

// Shared helpers of the Endless Tower's chat line protocols (EndlessTower.h, EndlessTowerShop.h): the server's blue
// notices are bar-separated fields, and the client's requests go out as typed chat commands.

#include <string>
#include <string_view>
#include <vector>

namespace GameLogic::EndlessTower
{
    // Splits "a|b|c" into its fields.
    std::vector<std::wstring> Split(std::wstring_view text);

    int ToInt(const std::wstring& field);

    long long ToLong(const std::wstring& field);

    // The rest of the fields from the given one, joined again: a text field comes last, so it may hold a bar.
    std::wstring Rest(const std::vector<std::wstring>& fields, size_t from);

    // True while a command can reach the game server (connected, a character in the world).
    bool CanSendCommand();

    // Sends the command as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    void SendCommand(const std::wstring& command);
}
