#include "stdafx.h"
#include "GameLogic/Commands/ChatCommandCatalog.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Network/Server/WSclient.h"
#include "UI/Core/WindowSystem.h"

#include <algorithm>
#include <vector>

namespace GameLogic::Commands
{
void ChatCommandCatalog::RequestOnce()
{
    if (this->m_wasRequested)
    {
        return;
    }

    this->m_wasRequested = true;
    if (SocketClient != nullptr)
    {
        SocketClient->ToGameServer()->SendChatCommandListRequest();
    }
}

void ChatCommandCatalog::Execute(const std::wstring& commandLine)
{
    if (commandLine.empty() || SocketClient == nullptr)
    {
        return;
    }

    // One of the client's own commands goes where a typed one goes; the server wouldn't know it.
    const std::wstring command = commandLine.substr(0, commandLine.find(L' '));
    const auto& commands = Catalog().GetCommands();
    const bool isLocal = std::any_of(commands.begin(), commands.end(), [&command](const ChatCommand& known)
                                     { return known.IsLocal && _wcsicmp(known.Command.c_str(), command.c_str()) == 0; });
    if (isLocal)
    {
        std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
        buffer.resize(std::max<size_t>(buffer.size() + 1, 256), 0);
        CheckCommand(buffer.data());
    }
    else
    {
        SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(commandLine.c_str()));
    }

    // Put it into the chat history as if it had been typed, so that it can be
    // repeated with the arrow keys without going through the window again.
    if (g_pChatInputBox != nullptr)
    {
        g_pChatInputBox->AddChatHistory(commandLine);
    }
}
} // namespace GameLogic::Commands
