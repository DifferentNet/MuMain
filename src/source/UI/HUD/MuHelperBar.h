
#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/RmlBridge/RmlThemedView.h"

namespace Rml { class ElementDocument; }

namespace mu::ui::window
{
    // RmlUi-backed "Official MU Helper" control bar (Config/Start/Stop for MUHelper::g_MuHelper)
    // plus the hero's live map/position readout. Render() is a no-op -- RmlUi draws everything via
    // SetPreSubmitCallback. UpdateMouseEvent()/UpdateKeyEvent() always report "not consumed" --
    // RmlUi's own context does hit-testing. Create()/Release()/GetLayerDepth()/Show()/Enable()/
    // IsVisible() still drive CManager's z-order/registration/visibility as before. Update() still
    // reads Hero's position every frame, now to feed the RmlUi model.
    class CMuHelperBar : public CObject
    {
    private:
        CManager* m_pNewUIMng;
        POINT m_CurHeroPosition;

    public:
        CMuHelperBar();
        virtual ~CMuHelperBar();

        bool Create(CManager* pNewUIMng);
        void Release();

        // Vestigial -- RmlUi/CSS owns this widget's position now (mu_helper_bar.rcss). No-op kept for API compatibility.

        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        float GetLayerDepth();	//. 4.3f

        void OpenningProcess();
        void ClosingProcess();

        // Gates the RmlUi doc's Show()/Hide() on IsVisible() AND sceneAllowsShow. Needed because
        // Update() only runs during MAIN_SCENE; this is called every frame regardless of scene
        // from CSystem::SyncMainSceneHudVisibility(), so leaving MAIN_SCENE still hides the doc.
        void SyncDocVisibility(bool sceneAllowsShow);


        // Vestigial -- Update() reads Hero's position directly, never through this setter.
        void SetCurHeroPosition(int x, int y) {}

        // Set by the RmlUi document's data-event-click bindings; polled and cleared like other windows' RmlClickX().
        void RmlClickConfig() { m_bRmlConfigClicked = true; }
        void RmlClickToggle() { m_bRmlToggleClicked = true; }

    private:
        struct MuHelperBarRmlModel
        {
            Rml::String positionText;   // "MapName (x, y)"
            bool muHelperActive = false; // drives which of Start/Stop is shown
            Rml::String configHint;
            Rml::String startHint;
            Rml::String stopHint;
            Rml::String pingText;       // "Ping 48 ms" under the bar (GameLogic/Character/PingHud.h), empty: hidden
            Rml::String pingLevel;      // "good", "fair", "bad" or "stale": the label's colour
            Rml::String pingHint;
        };
        void BindRmlModel(Rml::DataModelConstructor& c, MuHelperBarRmlModel& model);
        void OnRmlReloaded();
        UI::RmlBridge::ThemedView<MuHelperBarRmlModel> m_RmlView{"mu_helper_bar",
            [this](Rml::DataModelConstructor& c, MuHelperBarRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/mu_helper_bar.rml"}}, {.afterReload = [this] { OnRmlReloaded(); }}};

        bool m_bRmlConfigClicked = false;
        bool m_bRmlToggleClicked = false;

        // The hovered button's hint (0 config, 1 start, 2 stop), in the shared tooltip.

        void SyncRmlModel();
        void BuildRmlUi();
        // Writes `text` into the model field and dirties it, when it differs.
        void SyncText(Rml::String MuHelperBarRmlModel::* field, const char* boundName, const Rml::String& text);

        // SyncRmlModel()'s caches, so the texts are not formatted and converted every frame (per-frame
        // cost): the position readout for the map name and position it was built for, and each hint
        // for the language text it came from (a language switch swaps the text pointers).
        bool m_bPositionBuilt = false;
        const wchar_t* m_pPositionMapName = nullptr;
        POINT m_PositionBuiltAt{};
        Rml::String m_PositionText;
        static constexpr int HintCount = 3;
        const wchar_t* m_pHintSources[HintCount] = {};
        Rml::String m_HintTexts[HintCount];

        // The ping label's inputs: the texts are formatted again only when one of them changed.
        struct PingTextKey
        {
            bool hasReading = false;
            int lastMs = -1;
            int medianMs = -1;
            int networkMs = -1;
            bool stale = false;
            const wchar_t* labelFormat = nullptr;
            const wchar_t* hintFormat = nullptr;
            const wchar_t* hintNetworkFormat = nullptr;
            bool operator==(const PingTextKey&) const = default;
        };
        static PingTextKey CurrentPingTextKey();
        void BuildPingTexts(const PingTextKey& key);
        bool m_bPingBuilt = false;
        PingTextKey m_PingBuiltFor;
        std::string m_PingText;
        std::string m_PingLevel;
        std::string m_PingHint;
    };
}
