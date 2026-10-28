#include "GameLogic/Commands/ChatCommandCatalog.h"

#include "Data/Translation/MultiLanguage.h"

#include <cstdint>

namespace GameLogic::Commands
{
namespace
{
// Offsets of the AvailableChatCommand message (C2 header with sub code).
constexpr int32_t IndexOffset = 5;
constexpr int32_t CountOffset = 6;
constexpr int32_t StatusOffset = 7;
constexpr int32_t ParameterCountOffset = 8;
constexpr int32_t CommandOffset = 9;
constexpr int32_t CommandLength = 32;
constexpr int32_t NameOffset = 41;
constexpr int32_t NameLength = 48;
constexpr int32_t DescriptionOffset = 89;
constexpr int32_t DescriptionLength = 256;
constexpr int32_t ParametersOffset = 345;

// Offsets within one parameter entry.
constexpr int32_t ParameterSize = 102;
constexpr int32_t ParameterRequiredOffset = 0;
constexpr int32_t ParameterTypeOffset = 1;
constexpr int32_t ParameterNameOffset = 2;
constexpr int32_t ParameterNameLength = 32;
constexpr int32_t ParameterShortNameOffset = 34;
constexpr int32_t ParameterShortNameLength = 20;
constexpr int32_t ParameterValidValuesOffset = 54;
constexpr int32_t ParameterValidValuesLength = 48;

// The strings are UTF-8 and padded with zeros.
std::wstring ReadString(const BYTE* data, int32_t offset, int32_t length)
{
    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1, L'\0');
    CMultiLanguage::ConvertFromUtf8(buffer.data(), reinterpret_cast<const char*>(data + offset), length);
    buffer[length] = L'\0';
    return std::wstring(buffer.data());
}

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

// The commands the client handles itself (CheckCommand in Engine/Object/ZzzInterface.cpp). The server only
// lists its own, so these come after them (MU Moldova, 2026-10-21). Most act on the player you selected.
void AppendClientCommands(std::vector<ChatCommand>& commands)
{
    ChatCommandParameter words;
    words.Name = L"words";
    ChatCommandParameter text;
    text.Name = L"text";

    commands.push_back(ClientCommand(L"/trade", L"Trade with the player you selected, or the one who faces you. They have to stand next to you."));
    commands.push_back(ClientCommand(L"/party", L"Invite the player you selected to your party (also /pt)."));
    commands.push_back(ClientCommand(L"/guild", L"Ask the guild master you selected to take you into the guild."));
    commands.push_back(ClientCommand(L"/union", L"Guild masters: ask the guild master you selected for an alliance."));
    commands.push_back(ClientCommand(L"/rival", L"Guild masters: declare the guild of the guild master you selected hostile."));
    commands.push_back(ClientCommand(L"/rivaloff", L"Guild masters: end the hostility with the guild of the guild master you selected."));
    commands.push_back(ClientCommand(L"/duelstart", L"Challenge the player you selected to a duel."));
    commands.push_back(ClientCommand(L"/duelend", L"End your duel."));
    commands.push_back(ClientCommand(L"/filter", L"Show only the chat lines with these words (up to five). Without words, everything shows again.", {words}));
    commands.push_back(ClientCommand(L"/1", L"Save a chat line as macro 1; Alt+1 says it. /2 to /9 and /0 do the same for Alt+2 to Alt+0.", {text}));
}
} // namespace

bool ChatCommand::CanExecuteDirectly() const
{
    for (const auto& parameter : this->Parameters)
    {
        if (parameter.IsRequired)
        {
            return false;
        }
    }

    return true;
}

std::wstring ChatCommand::GetUsage() const
{
    std::wstring usage = this->Command;
    for (const auto& parameter : this->Parameters)
    {
        usage += parameter.IsRequired ? L" {" : L" [";
        usage += parameter.Name;
        usage += parameter.IsRequired ? L"}" : L"]";
    }

    return usage;
}

ChatCommandCatalog& ChatCommandCatalog::Instance()
{
    static ChatCommandCatalog instance;
    return instance;
}

void ChatCommandCatalog::Reset()
{
    this->m_commands.clear();
    this->m_expectedCount = 0;
    this->m_isComplete = false;
    this->m_wasRequested = false;
}

bool ChatCommandCatalog::AddFromPacket(const BYTE* data, int32_t size)
{
    if (data == nullptr || size < ParametersOffset)
    {
        return false;
    }

    const auto parameterCount = data[ParameterCountOffset];
    const auto requiredSize = ParametersOffset + parameterCount * ParameterSize;
    if (size < requiredSize)
    {
        return false;
    }

    const auto index = data[IndexOffset];
    const auto count = data[CountOffset];
    if (count == 0 || index >= count)
    {
        return false;
    }

    // The first message of a list starts a new one, so that a second request
    // doesn't append to the commands we already know.
    if (index == 0)
    {
        this->m_commands.clear();
        this->m_expectedCount = count;
        this->m_isComplete = false;
    }
    else if (this->m_isComplete || count != this->m_expectedCount || index != this->m_commands.size())
    {
        return false;
    }

    ChatCommand command;
    command.MinimumCharacterStatus = data[StatusOffset];
    command.Command = ReadString(data, CommandOffset, CommandLength);
    command.Name = ReadString(data, NameOffset, NameLength);
    command.Description = ReadString(data, DescriptionOffset, DescriptionLength);

    for (int32_t i = 0; i < parameterCount; ++i)
    {
        const auto* entry = data + ParametersOffset + i * ParameterSize;
        const auto parameterType = entry[ParameterTypeOffset];
        if (parameterType > static_cast<BYTE>(ChatCommandParameterType::Boolean))
        {
            return false;
        }

        ChatCommandParameter parameter;
        parameter.IsRequired = entry[ParameterRequiredOffset] != 0;
        parameter.Type = static_cast<ChatCommandParameterType>(parameterType);
        parameter.Name = ReadString(entry, ParameterNameOffset, ParameterNameLength);
        parameter.ShortName = ReadString(entry, ParameterShortNameOffset, ParameterShortNameLength);
        parameter.ValidValues = ReadString(entry, ParameterValidValuesOffset, ParameterValidValuesLength);
        command.Parameters.push_back(std::move(parameter));
    }

    if (command.Command.empty())
    {
        return false;
    }

    this->m_commands.push_back(std::move(command));
    this->m_isComplete = this->m_commands.size() >= count;
    if (this->m_isComplete)
    {
        AppendClientCommands(this->m_commands);
    }

    return true;
}

std::wstring ChatCommandCatalog::BuildCommandLine(const ChatCommand& command, const std::vector<std::wstring>& values)
{
    std::wstring line = command.Command;

    // The server switches to the "shortName=value" notation as soon as the line
    // contains an equals sign, which is what makes optional parameters skippable.
    const bool useShortNames = !command.Parameters.empty() && !command.Parameters.front().ShortName.empty();

    for (size_t i = 0; i < command.Parameters.size() && i < values.size(); ++i)
    {
        if (values[i].empty())
        {
            continue;
        }

        line += L' ';
        if (useShortNames && !command.Parameters[i].ShortName.empty())
        {
            line += command.Parameters[i].ShortName;
            line += L'=';
        }

        line += values[i];
    }

    return line;
}

} // namespace GameLogic::Commands
