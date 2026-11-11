#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Options/LootFilterWindow.h"

#include "Audio/DSPlaySound.h"
#include "Core/Utilities/StringUtils.h"
#include "GameLogic/Items/LootFilterRuntime.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <iterator>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
namespace LootFilter = GameLogic::Items::LootFilter;
namespace LootFilterRuntime = GameLogic::Items::LootFilterRuntime;

namespace
{
// The zen thresholds the stepper walks through; 0 is "off".
constexpr std::array<std::uint32_t, 9> ZenSteps{
    0, 10'000, 100'000, 500'000, 1'000'000, 5'000'000, 10'000'000, 50'000'000, 100'000'000};

// The item types' names, in LootFilter::Category order; nullptr for a retired category (never shown).
const wchar_t* const* const CategoryLabels[LootFilter::CategoryCount] = {
    &I18N::Game::LootFilterCatZen,        &I18N::Game::LootFilterCatJewel,  &I18N::Game::LootFilterCatWings,
    &I18N::Game::LootFilterCatContainer,  &I18N::Game::LootFilterCatQuest,  nullptr,
    &I18N::Game::LootFilterCatMedal,      &I18N::Game::LootFilterCatConsumable, &I18N::Game::LootFilterCatWeapon,
    &I18N::Game::LootFilterCatShield,     &I18N::Game::LootFilterCatArmour, &I18N::Game::LootFilterCatJewellery,
    &I18N::Game::LootFilterCatOther,
};

// The rarity options' names, in RarityFlag bit order; the level one is a format with the level threshold; nullptr
// for a retired bit (never shown).
const wchar_t* const* const RarityLabels[LootFilter::RarityFlagCount] = {
    &I18N::Game::LootFilterRarExcellent, &I18N::Game::LootFilterRarAncient, nullptr,
    &I18N::Game::LootFilterRarLuck,      &I18N::Game::LootFilterRarSkill,   &I18N::Game::LootFilterRarOption,
    &I18N::Game::LootFilterRarLevel,
};
constexpr int RarityLevelIndex = LootFilter::RarityFlagCount - 1;

Rml::String Narrow(const std::wstring& text)
{
    return StringUtils::WideToNarrow(text.c_str());
}

Rml::String Narrow(const wchar_t* text)
{
    return StringUtils::WideToNarrow(text);
}

// 5000000 -> "5,000,000".
std::wstring Thousands(std::uint32_t value)
{
    std::wstring digits = std::to_wstring(value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L",");
    return digits;
}

std::wstring LevelRarityLabel(int minLevel)
{
    wchar_t text[128] = {};
    std::swprintf(text, std::size(text), I18N::Game::LootFilterRarLevel, minLevel);
    return text;
}

// The step at or below the threshold, so a value set by hand in config.ini still steps sensibly.
int ZenStepIndex(std::uint32_t zenMin)
{
    int index = 0;
    for (int i = 0; i < static_cast<int>(ZenSteps.size()); ++i)
    {
        if (ZenSteps[static_cast<size_t>(i)] <= zenMin)
            index = i;
    }
    return index;
}

bool ValidCategory(int index)
{
    return index >= 0 && index < LootFilter::CategoryCount
        && !LootFilter::IsRetired(static_cast<LootFilter::Category>(index));
}

bool IsRetiredRarity(int bit)
{
    return (LootFilter::RarityRetiredMask & (1u << bit)) != 0;
}

bool ValidVerdict(int verdict)
{
    return verdict >= static_cast<int>(LootFilter::Verdict::Normal)
        && verdict <= static_cast<int>(LootFilter::Verdict::Highlight);
}

LootFilterLabels MakeLabels()
{
    LootFilterLabels labels;
    labels.enabled = Narrow(I18N::Game::LootFilterEnabled);
    labels.presets = Narrow(I18N::Game::LootFilterPresets);
    labels.presetDefault = Narrow(I18N::Game::LootFilterPresetDefault);
    labels.presetStrict = Narrow(I18N::Game::LootFilterPresetStrict);
    labels.presetOff = Narrow(I18N::Game::LootFilterPresetOff);
    labels.hide = Narrow(I18N::Game::LootFilterHide);
    labels.normal = Narrow(I18N::Game::LootFilterNormal);
    labels.highlight = Narrow(I18N::Game::LootFilterHighlight);
    labels.categories = Narrow(I18N::Game::LootFilterCategories);
    labels.rarity = Narrow(I18N::Game::LootFilterRarity);
    labels.level = Narrow(I18N::Game::LootFilterLevel);
    labels.zen = Narrow(I18N::Game::LootFilterZen);
    labels.names = Narrow(I18N::Game::LootFilterNames);
    labels.nameHint = Narrow(I18N::Game::LootFilterNameHint);
    labels.nameAdd = Narrow(I18N::Game::LootFilterNameAdd);
    labels.marker = Narrow(I18N::Game::LootFilterMarker);
    labels.sound = Narrow(I18N::Game::LootFilterSound);
    labels.close = Narrow(I18N::Game::LootFilterClose);
    return labels;
}

void BindStructs(Rml::DataModelConstructor& c)
{
    auto labels = c.RegisterStruct<LootFilterLabels>();
    labels.RegisterMember("enabled", &LootFilterLabels::enabled);
    labels.RegisterMember("presets", &LootFilterLabels::presets);
    labels.RegisterMember("preset_default", &LootFilterLabels::presetDefault);
    labels.RegisterMember("preset_strict", &LootFilterLabels::presetStrict);
    labels.RegisterMember("preset_off", &LootFilterLabels::presetOff);
    labels.RegisterMember("hide", &LootFilterLabels::hide);
    labels.RegisterMember("normal", &LootFilterLabels::normal);
    labels.RegisterMember("highlight", &LootFilterLabels::highlight);
    labels.RegisterMember("categories", &LootFilterLabels::categories);
    labels.RegisterMember("rarity", &LootFilterLabels::rarity);
    labels.RegisterMember("level", &LootFilterLabels::level);
    labels.RegisterMember("zen", &LootFilterLabels::zen);
    labels.RegisterMember("names", &LootFilterLabels::names);
    labels.RegisterMember("name_hint", &LootFilterLabels::nameHint);
    labels.RegisterMember("name_add", &LootFilterLabels::nameAdd);
    labels.RegisterMember("marker", &LootFilterLabels::marker);
    labels.RegisterMember("sound", &LootFilterLabels::sound);
    labels.RegisterMember("close", &LootFilterLabels::close);

    auto category = c.RegisterStruct<LootFilterCategoryRow>();
    category.RegisterMember("label", &LootFilterCategoryRow::label);
    category.RegisterMember("index", &LootFilterCategoryRow::index);
    category.RegisterMember("verdict", &LootFilterCategoryRow::verdict);
    category.RegisterMember("can_hide", &LootFilterCategoryRow::canHide);
    c.RegisterArray<std::vector<LootFilterCategoryRow>>();

    auto rarity = c.RegisterStruct<LootFilterRarityRow>();
    rarity.RegisterMember("label", &LootFilterRarityRow::label);
    rarity.RegisterMember("index", &LootFilterRarityRow::index);
    rarity.RegisterMember("on", &LootFilterRarityRow::on);
    c.RegisterArray<std::vector<LootFilterRarityRow>>();

    auto chip = c.RegisterStruct<LootFilterNameChip>();
    chip.RegisterMember("text", &LootFilterNameChip::text);
    chip.RegisterMember("index", &LootFilterNameChip::index);
    c.RegisterArray<std::vector<LootFilterNameChip>>();
}

int FirstArg(const Rml::VariantList& args, int fallback = -1)
{
    return args.empty() ? fallback : args[0].Get<int>(fallback);
}

int SecondArg(const Rml::VariantList& args, int fallback = -1)
{
    return args.size() < 2 ? fallback : args[1].Get<int>(fallback);
}

bool AddName(LootFilter::Settings& settings, const std::wstring& name)
{
    if (name.empty() || settings.names.size() >= LootFilter::MaxNames)
        return false;
    if (std::find(settings.names.begin(), settings.names.end(), name) != settings.names.end())
        return false;
    settings.names.push_back(name);
    return true;
}

// Changes the settings by one click; true when they changed.
bool ApplyAction(const CLootFilterWindow::PendingAction& pending, LootFilter::Settings& settings,
                 const std::wstring& typedName)
{
    switch (pending.action)
    {
    case CLootFilterWindow::Action::Enabled:
        settings.enabled = !settings.enabled;
        return true;
    case CLootFilterWindow::Action::Marker:
        settings.marker = !settings.marker;
        return true;
    case CLootFilterWindow::Action::Sound:
        settings.sound = !settings.sound;
        return true;
    case CLootFilterWindow::Action::Preset:
        if (pending.first < static_cast<int>(LootFilter::Preset::Off)
            || pending.first > static_cast<int>(LootFilter::Preset::Strict))
            return false;
        LootFilter::ApplyPreset(settings, static_cast<LootFilter::Preset>(pending.first));
        return true;
    case CLootFilterWindow::Action::Category:
    {
        if (!ValidCategory(pending.first) || !ValidVerdict(pending.second))
            return false;
        const auto category = static_cast<LootFilter::Category>(pending.first);
        const auto verdict = static_cast<LootFilter::Verdict>(pending.second);
        if (verdict == LootFilter::Verdict::Hide && !LootFilter::CanHide(category))
            return false;
        settings.categories[static_cast<size_t>(pending.first)] = verdict;
        return true;
    }
    case CLootFilterWindow::Action::Rarity:
        if (pending.first < 0 || pending.first >= LootFilter::RarityFlagCount || IsRetiredRarity(pending.first))
            return false;
        settings.rarity = static_cast<std::uint8_t>(settings.rarity ^ (1u << pending.first));
        return true;
    case CLootFilterWindow::Action::Level:
    {
        const int level = std::clamp(settings.minLevel + pending.first, LootFilter::MinLevelLow, LootFilter::MinLevelHigh);
        if (level == settings.minLevel)
            return false;
        settings.minLevel = level;
        return true;
    }
    case CLootFilterWindow::Action::Zen:
    {
        const int step = std::clamp(ZenStepIndex(settings.zenMin) + pending.first, 0, static_cast<int>(ZenSteps.size()) - 1);
        const std::uint32_t zen = ZenSteps[static_cast<size_t>(step)];
        if (zen == settings.zenMin)
            return false;
        settings.zenMin = zen;
        return true;
    }
    case CLootFilterWindow::Action::AddName:
        return AddName(settings, typedName);
    case CLootFilterWindow::Action::RemoveName:
        if (pending.first < 0 || pending.first >= static_cast<int>(settings.names.size()))
            return false;
        settings.names.erase(settings.names.begin() + pending.first);
        return true;
    case CLootFilterWindow::Action::Close:
        return false;
    }
    return false;
}
} // namespace

CLootFilterWindow::CLootFilterWindow() = default;

CLootFilterWindow::~CLootFilterWindow()
{
    Release();
}

bool CLootFilterWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(INTERFACE_LOOT_FILTER, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void CLootFilterWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float CLootFilterWindow::GetLayerDepth()
{
    return LayerDepth;
}

float CLootFilterWindow::GetKeyEventOrder()
{
    return 10.f;
}

void CLootFilterWindow::OpenningProcess()
{
    m_pending.clear();
}

void CLootFilterWindow::ClosingProcess()
{
    m_pending.clear();
    if (Rml::ElementDocument* document = m_RmlView.Document())
    {
        if (Rml::Element* focused = document->GetFocusLeafNode())
            focused->Blur();
    }
}

void CLootFilterWindow::OnRmlBuilt()
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return;
    // Dragged by any part that is not a control (base.rcss blocks those).
    if (Rml::Element* panel = document->GetElementById("panel"))
        UI::RmlBridge::MakeDraggable(panel, panel, nullptr, [panel] { UI::RmlBridge::KeepInsideWindow(panel); });
}

bool CLootFilterWindow::UpdateMouseEvent()
{
    // Every control is RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CLootFilterWindow::UpdateKeyEvent()
{
    if (!IsVisible())
        return true;

    if (IsPress(VK_ESCAPE))
    {
        Close();
        return false;
    }

    Rml::ElementDocument* document = m_RmlView.Document();
    const bool typing = document != nullptr && RmlUiRuntime::Instance().GetKeyOwningDocument() == document;
    if (typing && IsPress(VK_RETURN))
    {
        Queue(Action::AddName);
        return false;
    }

    return true;
}

bool CLootFilterWindow::Update()
{
    std::vector<PendingAction> pending = std::exchange(m_pending, {});
    if (IsVisible() && !pending.empty())
    {
        LootFilter::Settings settings = LootFilterRuntime::Current();
        const bool addsName = std::any_of(pending.begin(), pending.end(),
                                          [](const PendingAction& action) { return action.action == Action::AddName; });
        const std::wstring typedName = addsName ? TakeTypedName() : std::wstring();
        bool changed = false;
        for (const PendingAction& action : pending)
        {
            if (action.action == Action::Close)
            {
                Close();
                return true;
            }
            changed |= ApplyAction(action, settings, typedName);
        }
        if (changed)
        {
            LootFilterRuntime::Apply(settings);
            PlayBuffer(SOUND_CLICK01);
        }
    }

    SyncRmlModel();
    return true;
}

bool CLootFilterWindow::Render()
{
    // Nothing native: the whole window is RmlUi. Kept because CObject requires it.
    return true;
}

void CLootFilterWindow::Queue(Action action, int first, int second)
{
    m_pending.push_back({action, first, second});
}

void CLootFilterWindow::Close()
{
    g_pNewUISystem->Hide(INTERFACE_LOOT_FILTER);
    PlayBuffer(SOUND_CLICK01);
}

std::wstring CLootFilterWindow::TakeTypedName()
{
    std::wstring name = LootFilter::NormalizeName(StringUtils::NarrowToWide(m_RmlView.GetModel().nameInput));
    // The field empties even when the name is already there.
    SyncField(m_RmlView.Binder(), &LootFilterRmlModel::nameInput, "name_input", Rml::String());
    return name;
}

void CLootFilterWindow::BindRmlModel(Rml::DataModelConstructor& c, LootFilterRmlModel& model)
{
    BindStructs(c);
    c.Bind("has_title", &model.hasTitle);
    c.Bind("title", &model.title);
    c.Bind("positioned", &model.positioned);
    c.Bind("root_x", &model.rootX);
    c.Bind("root_y", &model.rootY);
    c.Bind("labels", &model.labels);
    c.Bind("enabled", &model.enabled);
    c.Bind("marker", &model.marker);
    c.Bind("sound", &model.sound);
    c.Bind("categories", &model.categories);
    c.Bind("rarities", &model.rarities);
    c.Bind("level_text", &model.levelText);
    c.Bind("zen_text", &model.zenText);
    c.Bind("names", &model.names);
    c.Bind("name_input", &model.nameInput);
    c.Bind("can_add_name", &model.canAddName);

    const auto queue = [this](Action action)
    {
        return [this, action](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        { Queue(action, FirstArg(args), SecondArg(args)); };
    };
    c.BindEventCallback("loot_filter_close", queue(Action::Close));
    c.BindEventCallback("loot_filter_enabled", queue(Action::Enabled));
    c.BindEventCallback("loot_filter_marker", queue(Action::Marker));
    c.BindEventCallback("loot_filter_sound", queue(Action::Sound));
    c.BindEventCallback("loot_filter_preset", queue(Action::Preset));
    c.BindEventCallback("loot_filter_category", queue(Action::Category));
    c.BindEventCallback("loot_filter_rarity", queue(Action::Rarity));
    c.BindEventCallback("loot_filter_level", queue(Action::Level));
    c.BindEventCallback("loot_filter_zen", queue(Action::Zen));
    c.BindEventCallback("loot_filter_name_add", queue(Action::AddName));
    c.BindEventCallback("loot_filter_name_remove", queue(Action::RemoveName));
}

void CLootFilterWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    Rml::ElementDocument* document = m_RmlView.Document();
    if (!document)
        return;

    UI::RmlBridge::SyncDocumentVisibilityInFront(document, IsVisible());
    if (!IsVisible())
        return;

    SyncContent();
}

void CLootFilterWindow::SyncContent()
{
    const LootFilter::Settings& settings = LootFilterRuntime::Current();
    LootFilterRmlModel updated;
    updated.title = Narrow(I18N::Game::LootFilterTitle);
    updated.labels = MakeLabels();
    updated.enabled = settings.enabled;
    updated.marker = settings.marker;
    updated.sound = settings.sound;

    for (int i = 0; i < LootFilter::CategoryCount; ++i)
    {
        const auto category = static_cast<LootFilter::Category>(i);
        if (LootFilter::IsRetired(category))
            continue;
        updated.categories.push_back({Narrow(*CategoryLabels[i]), i,
                                      static_cast<int>(settings.categories[static_cast<size_t>(i)]),
                                      LootFilter::CanHide(category)});
    }

    for (int i = 0; i < LootFilter::RarityFlagCount; ++i)
    {
        if (IsRetiredRarity(i))
            continue;
        const std::wstring label = i == RarityLevelIndex ? LevelRarityLabel(settings.minLevel) : *RarityLabels[i];
        updated.rarities.push_back({Narrow(label), i, (settings.rarity & (1u << i)) != 0});
    }

    updated.levelText = "+" + std::to_string(settings.minLevel);
    updated.zenText = settings.zenMin == 0 ? Narrow(I18N::Game::LootFilterZenOff) : Narrow(Thousands(settings.zenMin));

    for (size_t i = 0; i < settings.names.size(); ++i)
        updated.names.push_back({Narrow(settings.names[i]), static_cast<int>(i)});

    const std::wstring typed = LootFilter::NormalizeName(StringUtils::NarrowToWide(m_RmlView.GetModel().nameInput));
    updated.canAddName = !typed.empty() && settings.names.size() < LootFilter::MaxNames;

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &LootFilterRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::labels, "labels", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::enabled, "enabled", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::marker, "marker", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::sound, "sound", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::categories, "categories", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::rarities, "rarities", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::levelText, "level_text", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::zenText, "zen_text", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::names, "names", updated);
    SyncFieldFrom(binder, &LootFilterRmlModel::canAddName, "can_add_name", updated);
}
