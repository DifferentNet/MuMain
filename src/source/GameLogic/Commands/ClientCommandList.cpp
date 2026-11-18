// ClientCommandList.cpp - the chat commands the client handles itself.

#include "stdafx.h"

#include "GameLogic/Commands/ClientCommandList.h"

#include "I18N/All.h"

namespace GameLogic::Commands
{
namespace
{
ChatCommand ClientCommand(const wchar_t* command, const wchar_t* description,
                          std::vector<ChatCommandParameter> parameters = {})
{
    ChatCommand result;
    result.Command = command;
    result.Name = command;
    result.Description = description;
    result.Parameters = std::move(parameters);
    result.IsLocal = true;
    return result;
}
} // namespace

// Most of them act on the player you selected (MU Moldova, 2026-10-21).
void AppendClientCommands(std::vector<ChatCommand>& commands)
{
    ChatCommandParameter words;
    words.Name = I18N::Game::ChatCmdParamWords;
    ChatCommandParameter text;
    text.Name = I18N::Game::ChatCmdParamText;

    commands.push_back(ClientCommand(L"/trade", I18N::Game::ChatCmdHelpTrade));
    commands.push_back(ClientCommand(L"/party", I18N::Game::ChatCmdHelpParty));
    commands.push_back(ClientCommand(L"/guild", I18N::Game::ChatCmdHelpGuild));
    commands.push_back(ClientCommand(L"/union", I18N::Game::ChatCmdHelpUnion));
    commands.push_back(ClientCommand(L"/rival", I18N::Game::ChatCmdHelpRival));
    commands.push_back(ClientCommand(L"/rivaloff", I18N::Game::ChatCmdHelpRivalOff));
    commands.push_back(ClientCommand(L"/duelstart", I18N::Game::ChatCmdHelpDuelStart));
    commands.push_back(ClientCommand(L"/duelend", I18N::Game::ChatCmdHelpDuelEnd));
    commands.push_back(ClientCommand(L"/r", I18N::Game::ChatCmdHelpReply, {text}));
    commands.push_back(ClientCommand(L"/filter", I18N::Game::ChatCmdHelpFilter, {words}));
    commands.push_back(ClientCommand(L"/1", I18N::Game::ChatCmdHelpMacro, {text}));
}
} // namespace GameLogic::Commands
