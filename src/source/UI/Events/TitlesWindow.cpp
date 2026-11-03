#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/TitlesWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/Titles.h"
#include "Render/RmlUi/RmlAdditiveMeshElement.h"
#include "UI/Character/TitlePlateEffects.h"
#include "UI/Character/TitlePlates.h"
#include "UI/Character/TitlePlatesArt.h"
#include "UI/Core/WindowSystem.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/SystemInterface.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cwchar>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::Titles::Titles;

namespace
{
// While the window is open, the list is asked for again this often: titles come and go with the weeks.
constexpr auto RefreshInterval = std::chrono::seconds(60);
// Without an answer this long after opening, the server doesn't send titles.
constexpr auto AnswerTimeout = std::chrono::seconds(5);

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

std::wstring Printf(const wchar_t* format, ...)
{
    wchar_t text[512] = {};
    va_list args;
    va_start(args, format);
    std::vswprintf(text, std::size(text), format, args);
    va_end(args);
    return text;
}

// "3 d 4 h left", "4 h 10 min left", "10 min left".
std::wstring TimeLeft(long long seconds)
{
    const long long minutes = std::max<long long>(1, (seconds + 59) / 60);
    if (minutes >= 24 * 60)
        return Printf(I18N::Game::TitlesLeftDays, static_cast<int>(minutes / (24 * 60)),
                      static_cast<int>(minutes / 60 % 24));
    if (minutes >= 60)
        return Printf(I18N::Game::TitlesLeftHours, static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    return Printf(I18N::Game::TitlesLeftMinutes, static_cast<int>(minutes));
}

// "3", "2.5": a percent with a dot, whatever the C locale says.
std::wstring Percent(float value)
{
    const int tenths = static_cast<int>(value * 10.f + 0.5f);
    return tenths % 10 == 0 ? std::to_wstring(tenths / 10)
                            : std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
}

// The bonus of a title while it is worn: "+3% HP, -1% damage taken, +1% damage".
std::wstring BonusText(const GameLogic::Titles::Entry& entry)
{
    std::wstring text;
    auto add = [&text](wchar_t sign, float value, const wchar_t* what)
    {
        if (value <= 0.f)
            return;
        if (!text.empty())
            text += L", ";
        text += sign + Percent(value) + L"% " + what;
    };
    add(L'+', entry.BonusHealth, I18N::Game::TitlesBonusHealth);
    add(L'-', entry.BonusTaken, I18N::Game::TitlesBonusTaken);
    add(L'+', entry.BonusDamage, I18N::Game::TitlesBonusDamage);
    return text;
}

// How long a title not owned yet would last.
const wchar_t* TermText(GameLogic::Titles::Term term)
{
    switch (term)
    {
    case GameLogic::Titles::Term::Week:
        return I18N::Game::TitlesTermWeek;
    case GameLogic::Titles::Term::Season:
        return I18N::Game::TitlesTermSeason;
    case GameLogic::Titles::Term::WhileHeld:
        return I18N::Game::TitlesWhileHeld;
    default:
        return I18N::Game::TitlesForever;
    }
}
} // namespace

mu::ui::window::CTitlesWindow::CTitlesWindow() = default;

mu::ui::window::CTitlesWindow::~CTitlesWindow()
{
    Release();
}

bool mu::ui::window::CTitlesWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_TITLES, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CTitlesWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CTitlesWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CTitlesWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CTitlesWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_lastRequest = m_openedAt;
    m_tab = 0;
    m_hoverRow = -1;
    m_pendingHover = -2;
    // The document may be new (a theme switch), so the preview is built again.
    m_previewRarity = -2;
    m_previewId.clear();
    Titles().RequestList();
}

void mu::ui::window::CTitlesWindow::ClosingProcess() {}

bool mu::ui::window::CTitlesWindow::UpdateMouseEvent()
{
    // The tabs, the rows and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CTitlesWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_TITLES))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TITLES);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CTitlesWindow::Update()
{
    const bool exit = m_pendingExit;
    const int tab = m_pendingTab;
    const int wear = m_pendingWear;
    m_pendingExit = false;
    m_pendingTab = -1;
    m_pendingWear = -1;
    if (m_pendingHover != -2)
    {
        m_hoverRow = m_pendingHover;
        m_pendingHover = -2;
    }
    if (IsVisible())
    {
        auto& store = Titles();
        const auto now = std::chrono::steady_clock::now();
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TITLES);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            if (tab >= 0 && tab <= 1 && tab != m_tab)
            {
                m_tab = tab;
                m_hoverRow = -1;
                PlayBuffer(SOUND_CLICK01);
            }

            if (wear >= 0 && wear < static_cast<int>(m_rowIds.size()) && m_rowOwned[wear])
            {
                // "No title" also when nothing is worn yet: it stops a new title from going on by itself.
                const std::wstring& id = m_rowIds[wear];
                if (id != store.Wearing() || (id.empty() && store.Automatic()))
                {
                    store.Wear(id);
                    m_lastRequest = now;
                }
                PlayBuffer(SOUND_CLICK01);
            }

            if (now - m_lastRequest >= RefreshInterval)
            {
                store.RequestList();
                m_lastRequest = now;
            }
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CTitlesWindow::Render()
{
    // Nothing native: the frame, the tabs and the rows are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CTitlesWindow::BindRmlModel(Rml::DataModelConstructor& c, TitlesRmlModel& model)
{
    auto lineType = c.RegisterStruct<TitlesLine>();
    lineType.RegisterMember("text", &TitlesLine::text);
    lineType.RegisterMember("text_px", &TitlesLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("status", &model.status);
    c.Bind("detail", &model.detail);
    c.Bind("footer", &model.footer);

    auto tabType = c.RegisterStruct<TitlesTab>();
    tabType.RegisterMember("label", &TitlesTab::label);
    tabType.RegisterMember("selected", &TitlesTab::selected);
    c.RegisterArray<std::vector<TitlesTab>>();
    c.Bind("tabs", &model.tabs);

    auto rowType = c.RegisterStruct<TitlesRow>();
    rowType.RegisterMember("index", &TitlesRow::index);
    rowType.RegisterMember("name", &TitlesRow::name);
    rowType.RegisterMember("detail", &TitlesRow::detail);
    rowType.RegisterMember("bonus", &TitlesRow::bonus);
    rowType.RegisterMember("hint", &TitlesRow::hint);
    rowType.RegisterMember("mark", &TitlesRow::mark);
    rowType.RegisterMember("rarity", &TitlesRow::rarity);
    rowType.RegisterMember("owned", &TitlesRow::owned);
    rowType.RegisterMember("worn", &TitlesRow::worn);
    c.RegisterArray<std::vector<TitlesRow>>();
    c.Bind("rows", &model.rows);

    c.Bind("exit_tooltip", &model.exitTooltip);
    c.Bind("fx", &model.effects);

    c.BindEventCallback("titles_exit",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingExit = true; });
    c.BindEventCallback("titles_tab",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingTab = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("titles_wear",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingWear = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("titles_hover",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingHover = args[0].Get<int>(-1);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CTitlesWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    if (!m_RmlView.Document())
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    SyncContent();
}

void mu::ui::window::CTitlesWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> TitlesLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };
    // A text that wraps in its box (a hint): the native size, never shrunk.
    auto wrapped = [](const std::wstring& text) -> TitlesLine
    {
        if (text.empty())
            return {};
        return {StringUtils::WideToNarrow(text.c_str()), UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal)};
    };

    auto& store = Titles();
    const auto now = std::chrono::steady_clock::now();

    TitlesRmlModel updated;
    updated.title = line(I18N::Game::TitlesTitle, WINDOW_WIDTH, true);
    updated.tabs.push_back({line(I18N::Game::TitlesTabMine, TAB_WIDTH), m_tab == 0});
    updated.tabs.push_back({line(I18N::Game::TitlesTabAll, TAB_WIDTH), m_tab == 1});

    std::vector<std::wstring> ids;
    std::vector<bool> owned;
    std::vector<GameLogic::Titles::Worn> plates;
    int wornRow = -1;
    if (!store.HasList())
    {
        updated.status = line(now - m_openedAt < AnswerTimeout ? I18N::Game::TitlesAsking : I18N::Game::TitlesNoAnswer,
                              CONTENT_WIDTH);
    }
    else
    {
        const auto& list = store.List();
        const auto worn = std::find_if(list.begin(), list.end(),
                                       [&store](const auto& e) { return e.Owned && e.Id == store.Wearing(); });
        updated.status = line(worn != list.end() ? Printf(I18N::Game::TitlesWearing, worn->Name.c_str())
                                                 : std::wstring(I18N::Game::TitlesWearingNone),
                              CONTENT_WIDTH, true);
        updated.detail =
            line(store.Automatic() ? I18N::Game::TitlesAutomatic : I18N::Game::TitlesClickHint, CONTENT_WIDTH);

        // Seconds since the list came: the time left counts down between the answers.
        const long long elapsed = static_cast<long long>((GetTickCount64() - store.ListTime()) / 1000);
        const TitlesLine wornMark = line(I18N::Game::TitlesWorn, MARK_WIDTH, true);

        if (m_tab == 0)
        {
            TitlesRow none;
            none.index = 0;
            none.name = line(I18N::Game::TitlesNone, ROW_WIDTH, true);
            none.detail = line(I18N::Game::TitlesNoneDetail, ROW_WIDTH - MARK_WIDTH);
            none.rarity = -1;
            none.owned = true;
            none.worn = worn == list.end();
            if (none.worn)
                none.mark = wornMark;
            updated.rows.push_back(std::move(none));
            ids.emplace_back();
            owned.push_back(true);
            plates.emplace_back();
        }

        int ownedCount = 0;
        for (const auto& entry : list)
        {
            if (entry.Owned)
                ++ownedCount;
            if (m_tab == 0 && !entry.Owned)
                continue;

            TitlesRow row;
            row.index = static_cast<int>(updated.rows.size());
            row.name = line(entry.Name, ROW_WIDTH, true);
            row.rarity = static_cast<int>(entry.Color);
            row.owned = entry.Owned;
            row.worn = entry.Owned && entry.Id == store.Wearing();
            if (row.worn)
            {
                row.mark = wornMark;
                wornRow = row.index;
            }
            if (entry.HasBonus())
                row.bonus = wrapped(BonusText(entry));

            if (!entry.Owned)
            {
                row.detail = line(TermText(entry.Lasts), ROW_WIDTH - MARK_WIDTH);
                row.hint = wrapped(entry.Hint);
            }
            else if (entry.SecondsLeft < 0)
            {
                row.detail = line(I18N::Game::TitlesWhileHeld, ROW_WIDTH - MARK_WIDTH);
            }
            else if (entry.SecondsLeft == 0)
            {
                row.detail = line(I18N::Game::TitlesForever, ROW_WIDTH - MARK_WIDTH);
            }
            else
            {
                row.detail =
                    line(TimeLeft(std::max<long long>(0, entry.SecondsLeft - elapsed)), ROW_WIDTH - MARK_WIDTH);
            }

            updated.rows.push_back(std::move(row));
            ids.push_back(entry.Id);
            owned.push_back(entry.Owned);
            plates.push_back({entry.Id, entry.Color, entry.Name, entry.Name});
        }

        if (m_tab == 0 && ownedCount == 0)
            updated.footer = wrapped(I18N::Game::TitlesEmpty);
        else if (m_tab == 1)
            updated.footer =
                line(Printf(I18N::Game::TitlesOwnedCount, ownedCount, static_cast<int>(list.size())), CONTENT_WIDTH);
    }

    m_rowIds = std::move(ids);
    m_rowOwned = std::move(owned);
    m_rowPlates = std::move(plates);
    m_wornRow = wornRow;
    updated.effects = UI::TitlePlates::Effects::Enabled();

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &TitlesRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::detail, "detail", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::footer, "footer", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::effects, "fx", updated);

    SyncPreview();
    SyncRowShine();
}

// The light over the worn row (the row's <additive-mesh>, there while the effects are on): a soft band crossing it in
// a second, every five seconds. RmlUi's gradients need shaders the renderer doesn't have, so it is a mesh of one
// white texel of the effects atlas with the light in the vertex colours.
void mu::ui::window::CTitlesWindow::SyncRowShine()
{
    constexpr double Period = 5.0;
    constexpr double Run = 1.1;
    constexpr float BandShare = 0.35f;
    // (255, 240, 200) at 30 %, as ABGR (the alpha unused: added).
    constexpr std::uint32_t Peak = (60u << 16) | (72u << 8) | 77u;
    // Inside silver's silhouette: solid white.
    constexpr float U = 160.f / UI::TitlePlates::Art::FxAtlasWidth;
    constexpr float V = 33.f / UI::TitlePlates::Art::FxAtlasHeight;

    Rml::ElementList shines;
    m_RmlView.Document()->GetElementsByClassName(shines, "tt-row-shine");
    for (Rml::Element* element : shines)
    {
        const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
        const double now = Rml::GetSystemInterface()->GetElapsedTime();
        const double local = std::fmod(now, Period);
        std::vector<Render::Renderer::RecordedVertex> quads;
        if (local < Run && size.x > 0.f && size.y > 0.f)
        {
            const float band = size.x * BandShare;
            const float x0 = -band + (size.x + band) * static_cast<float>(local / Run);
            const float xs[] = {x0, x0 + band / 2.f, x0 + band};
            const float light[] = {0.f, 1.f, 0.f};
            const auto colour = [](float t)
            {
                const auto channel = [t](int shift)
                { return static_cast<std::uint32_t>(((Peak >> shift) & 0xFFu) * t + 0.5f) << shift; };
                return channel(0) | channel(8) | channel(16);
            };
            for (int i = 0; i < 2; ++i)
            {
                // Cut to the row, the light carried to the cut.
                const float a = std::clamp(xs[i], 0.f, size.x);
                const float b = std::clamp(xs[i + 1], 0.f, size.x);
                if (b <= a)
                    continue;
                const auto at = [&](float x)
                { return light[i] + (light[i + 1] - light[i]) * (x - xs[i]) / (xs[i + 1] - xs[i]); };
                const std::uint32_t ca = colour(at(a));
                const std::uint32_t cb = colour(at(b));
                quads.push_back({a, 0.f, U, V, ca});
                quads.push_back({a, size.y, U, V, ca});
                quads.push_back({b, size.y, U, V, cb});
                quads.push_back({b, 0.f, U, V, cb});
            }
        }
        static_cast<Render::RmlUi::AdditiveMeshElement*>(element)->SetMesh(UI::TitlePlates::Effects::FxAtlasSource,
                                                                           quads, 1.f);
    }
}

void mu::ui::window::CTitlesWindow::SyncPreview()
{
    namespace Art = UI::TitlePlates::Art;
    namespace Effects = UI::TitlePlates::Effects;

    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Element* box = document->GetElementById("tt_preview");
    Rml::Element* glow = document->GetElementById("tt_preview_glow");
    Rml::Element* plateImage = document->GetElementById("tt_preview_plate");
    Rml::Element* light = document->GetElementById("tt_preview_fx");
    Rml::Element* text = document->GetElementById("tt_preview_text");
    if (box == nullptr || glow == nullptr || plateImage == nullptr || light == nullptr || text == nullptr)
        return;

    // The row under the mouse ("No title": none), else the worn one.
    const int row = m_hoverRow >= 0 && m_hoverRow < static_cast<int>(m_rowPlates.size()) ? m_hoverRow : m_wornRow;
    const GameLogic::Titles::Worn* title =
        row >= 0 && row < static_cast<int>(m_rowPlates.size()) && !m_rowPlates[row].Id.empty() ? &m_rowPlates[row]
                                                                                               : nullptr;
    const Rml::Vector2f size = box->GetBox().GetSize();
    const int rarity = title != nullptr ? static_cast<int>(title->Color) : -1;
    if (title == nullptr || size.x <= 0.f || size.y <= 0.f)
    {
        if (m_previewRarity != -1)
        {
            m_previewRarity = -1;
            m_previewId.clear();
            box->SetProperty("visibility", "hidden");
        }
        return;
    }

    // One scale for all six plates, the widest and tallest fitting the box, and every field's middle at one height:
    // the plates keep their sizes against each other, as over the heads.
    static const std::array<float, 3> bounds = []
    {
        float widest = 0.f, above = 0.f, below = 0.f;
        for (const Art::Plate& p : Art::Plates)
        {
            const float middle = (p.fieldTop + p.fieldBottom) / 2.f;
            widest = std::max(widest, p.width);
            above = std::max(above, middle);
            below = std::max(below, p.height - middle);
        }
        return std::array<float, 3>{widest, above, below};
    }();
    const float k = std::min(size.x / bounds[0], size.y / (bounds[1] + bounds[2]));
    const float middleY = (bounds[1] * k + size.y - bounds[2] * k) / 2.f;
    const Art::Plate& plate = Art::Plates[rarity];
    const float left = size.x / 2.f - (plate.fieldLeft + plate.fieldRight) / 2.f * k;
    const float top = middleY - (plate.fieldTop + plate.fieldBottom) / 2.f * k;
    const float fieldLeft = left + plate.fieldLeft * k;
    const float fieldTop = top + plate.fieldTop * k;
    const float fieldWidth = (plate.fieldRight - plate.fieldLeft) * k;
    const float fieldHeight = (plate.fieldBottom - plate.fieldTop) * k;

    // The panel's scale (transform: scale(--root-scale)): two screen points 100 px apart, brought into the box.
    Rml::Vector2f origin(0.f, 0.f), across(100.f, 0.f);
    const float projected = box->Project(origin) && box->Project(across) ? across.x - origin.x : 0.f;
    const float rootScale = projected > 0.f ? 100.f / projected : 1.f;

    const auto px = [](float value) { return Rml::Property(value, Rml::Unit::PX); };
    if (m_previewId != title->Id || m_previewText != title->Text() || m_previewPlate != plateImage ||
        m_previewRarity != rarity || m_previewWidth != size.x || m_previewHeight != size.y ||
        std::abs(m_previewScale - rootScale) > 0.001f)
    {
        if (m_previewRarity < 0)
            box->SetProperty("visibility", "visible");
        m_previewId = title->Id;
        m_previewText = title->Text();
        m_previewPlate = plateImage;
        m_previewRarity = rarity;
        m_previewWidth = size.x;
        m_previewHeight = size.y;
        m_previewScale = rootScale;

        // The art as a quad of its atlas (an <img rect> would take the rectangle in the document's dp, not the file's).
        const float right = left + plate.width * k;
        const float bottom = top + plate.height * k;
        const float u0 = plate.x / Art::AtlasSize;
        const float v0 = plate.y / Art::AtlasSize;
        const float u1 = (plate.x + plate.width) / Art::AtlasSize;
        const float v1 = (plate.y + plate.height) / Art::AtlasSize;
        const Render::Renderer::RecordedVertex quad[] = {{left, top, u0, v0, 0xFFFFFFFFu},
                                                         {left, bottom, u0, v1, 0xFFFFFFFFu},
                                                         {right, bottom, u1, v1, 0xFFFFFFFFu},
                                                         {right, top, u1, v0, 0xFFFFFFFFu}};
        static_cast<Render::RmlUi::AdditiveMeshElement*>(plateImage)
            ->SetMesh(UI::TitlePlates::AtlasSource, quad, 1.f, false);

        // The text in bold, as large as the field lets it be (a long title in the regular font), sharp: a .sharp-text
        // layer, so its place is in the panel's px and its size, line and font in the screen's.
        const std::wstring& name = title->Text();
        g_pRenderText->SetFont(g_hFontBold);
        const float boldWidth = static_cast<float>(MeasureInReferenceUnits(name));
        g_pRenderText->SetFont(g_hFont);
        const float regularWidth = static_cast<float>(MeasureInReferenceUnits(name));
        const float room = fieldWidth * 0.86f;
        const float fieldPx = fieldHeight * 0.62f * rootScale;
        float textPx =
            std::min(fieldPx, UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Bold, boldWidth, room));
        bool bold = textPx >= fieldPx * 0.85f;
        if (!bold)
        {
            const float regularPx =
                std::min(fieldPx, UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Normal, regularWidth, room));
            bold = regularPx <= textPx;
            textPx = std::max(textPx, regularPx);
        }
        text->SetInnerRML(Rml::StringUtilities::EncodeRml(StringUtils::WideToNarrow(name.c_str())));
        text->SetProperty(Rml::PropertyId::Left, px(fieldLeft));
        text->SetProperty(Rml::PropertyId::Top, px(fieldTop));
        text->SetProperty(Rml::PropertyId::Width, px(fieldWidth * rootScale));
        text->SetProperty(Rml::PropertyId::Height, px(fieldHeight * rootScale));
        text->SetProperty(Rml::PropertyId::LineHeight, px(fieldHeight * rootScale));
        text->SetProperty(Rml::PropertyId::FontSize, px(textPx));
        text->SetProperty("font-weight", bold ? "bold" : "normal");
    }

    // The effects move every frame: the hero's own moments, so the preview flashes with the hero's plate.
    const bool effects = Effects::Enabled();
    const float phase = Effects::PhaseOf(Hero != nullptr ? Hero->ID : nullptr);
    const Effects::Placement at{left, top, k};
    const auto mesh = [](Rml::Element* element) { return static_cast<Render::RmlUi::AdditiveMeshElement*>(element); };
    m_previewVertices.clear();
    if (effects)
        Effects::BuildUnder(*title, phase, at, m_previewVertices);
    mesh(glow)->SetMesh(Effects::FxAtlasSource, m_previewVertices, 1.f);
    m_previewVertices.clear();
    if (effects)
        Effects::BuildOver(*title, phase, at, m_previewVertices);
    mesh(light)->SetMesh(Effects::FxAtlasSource, m_previewVertices, 1.f);

    const auto& base = UI::TitlePlates::BaseTextColor(title->Color);
    const std::array<unsigned char, 3> color =
        effects ? Effects::TextColor(*title, phase, base) : std::array<unsigned char, 3>{base[0], base[1], base[2]};
    char rgb[32];
    std::snprintf(rgb, sizeof(rgb), "rgb(%d, %d, %d)", color[0], color[1], color[2]);
    text->SetProperty("color", rgb);
}
