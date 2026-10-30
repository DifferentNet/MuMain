#include "stdafx.h"
#include "GameLogic/Events/EndlessTowerLines.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

#include <cwchar>

namespace GameLogic::EndlessTower
{
std::vector<std::wstring> Split(std::wstring_view text)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true)
    {
        const size_t bar = text.find(L'|', start);
        fields.emplace_back(text.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start));
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }
    return fields;
}

int ToInt(const std::wstring& field)
{
    return static_cast<int>(std::wcstol(field.c_str(), nullptr, 10));
}

long long ToLong(const std::wstring& field)
{
    return std::wcstoll(field.c_str(), nullptr, 10);
}

std::wstring Rest(const std::vector<std::wstring>& fields, size_t from)
{
    std::wstring text;
    for (size_t i = from; i < fields.size(); ++i)
        text += (i > from ? L"|" : L"") + fields[i];
    return text;
}

bool CanSendCommand()
{
    return SocketClient != nullptr && Hero != nullptr;
}

void SendCommand(const std::wstring& command)
{
    if (!CanSendCommand())
        return;

    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}
} // namespace GameLogic::EndlessTower
