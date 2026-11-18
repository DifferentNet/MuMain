#include "App/stdafx.h"

#include <doctest.h>

#include "GameLogic/Commands/ClientCommandList.h"

#include <vector>

TEST_CASE("every client chat command has a localized description [ui][round13]")
{
    std::vector<GameLogic::Commands::ChatCommand> commands;
    GameLogic::Commands::AppendClientCommands(commands);

    REQUIRE(commands.size() == 11);
    for (const auto& command : commands)
    {
        CAPTURE(command.Command);
        CHECK(command.IsLocal);
        CHECK_FALSE(command.Description.empty());
        for (const auto& parameter : command.Parameters)
        {
            CHECK_FALSE(parameter.Name.empty());
        }
    }
}
