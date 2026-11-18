#pragma once

// ClientCommandList.h - the chat commands the client handles itself.

#include "GameLogic/Commands/ChatCommandCatalog.h"

#include <vector>

namespace GameLogic::Commands
{
// Appends the commands of CheckCommand (Engine/Object/ZzzInterface.cpp) with
// descriptions in the player's language. The server lists only its own
// commands, so these come after them in the J window.
void AppendClientCommands(std::vector<ChatCommand>& commands);
} // namespace GameLogic::Commands
