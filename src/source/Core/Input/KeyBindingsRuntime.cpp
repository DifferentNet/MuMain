#include "stdafx.h"
#include "Core/Input/KeyBindingsRuntime.h"

#include "Core/Utilities/StringUtils.h"
#include "Data/GameConfig/GameConfig.h"

static_assert(Core::Input::KeyBindings::KeyHome == VK_HOME, "KeyBindings::KeyHome must be VK_HOME");

namespace Core::Input::KeyBindings::Runtime
{
namespace
{
Table g_table;
unsigned g_revision = 0;

std::wstring Wide(std::string_view text)
{
    return std::wstring(text.begin(), text.end());
}

std::string Narrow(const std::wstring& text)
{
    return StringUtils::WideToNarrow(text.c_str());
}

// Writes one action's line: its key when off the default, no line when on it.
void SaveAction(Action action)
{
    if (action == Action::None)
        return;
    const ActionInfo& info = InfoOf(action);
    const int key = g_table.KeyOf(action);
    const std::wstring keyName = key == info.defaultKey ? std::wstring() : Wide(KeyName(key));
    GameConfig::GetInstance().SetKeyBinding(Wide(info.configName).c_str(), keyName);
}

void SaveAll()
{
    for (const ActionInfo& info : Actions())
        SaveAction(info.action);
}
} // namespace

const Table& Current()
{
    return g_table;
}

int KeyOf(Action action)
{
    return g_table.KeyOf(action);
}

int RemapDefaultKey(int key)
{
    const Action action = DefaultActionOf(key);
    return action == Action::None ? key : g_table.KeyOf(action);
}

void Load()
{
    GameConfig& config = GameConfig::GetInstance();
    Table::Entries saved;
    for (const ActionInfo& info : Actions())
    {
        const std::wstring keyName = config.GetKeyBinding(Wide(info.configName).c_str());
        if (!keyName.empty())
            saved.emplace_back(std::string(info.configName), Narrow(keyName));
    }

    g_table = Table::FromEntries(saved);
    if (g_table.ToEntries() != saved)
        SaveAll();
    ++g_revision;
}

Table::Change Assign(Action action, int key)
{
    const Table::Change change = g_table.Assign(action, key);
    if (change.first == Action::None)
        return change;

    SaveAction(change.first);
    SaveAction(change.swapped);
    ++g_revision;
    return change;
}

void ResetToDefaults()
{
    g_table.Reset();
    SaveAll();
    ++g_revision;
}

unsigned Revision()
{
    return g_revision;
}
} // namespace Core::Input::KeyBindings::Runtime
