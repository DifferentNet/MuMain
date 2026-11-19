#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/HUD/SlideTicker.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Types.h>

#include <string>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
    class CManager;

    class CSlideWindow : public CObject
    {
        CManager* m_pNewUIMng;
    public:
        CSlideWindow();
        virtual ~CSlideWindow();

        bool Create(CManager* pNewUIMng);
        void Release();

        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        float GetLayerDepth();		// 1.91f

        // wrapping
        void Init() { m_pSlideMgr->Init(); }
        void CreateSlideText() { m_pSlideMgr->CreateSlideText(); }
        void AddSlide(int iLoopCount, int iLoopDelay, const wchar_t* strText, int iType, float fSpeed, DWORD dwTextColor = (255 << 24) + (200 << 16) + (220 << 8) + (230))
        {
            m_pSlideMgr->AddSlide(iLoopCount, iLoopDelay, strText, iType, fSpeed, dwTextColor);
        }

    private:
        struct SlideNoticeRmlModel
        {
            float rootScale = 1.f;
            float textPx = 0.f;
            bool shown = false;
            float textX = 0.f;
            float textTop = 0.f;
            float bandTop = 0.f;
            float bandHeight = 0.f;
            Rml::String bandColor;
            Rml::String textColor;
            Rml::String text;
        };
        void BindRmlModel(Rml::DataModelConstructor& c, SlideNoticeRmlModel& model);
        UI::RmlBridge::ThemedView<SlideNoticeRmlModel> m_RmlView{"slide_notice",
            [this](Rml::DataModelConstructor& c, SlideNoticeRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/slide_notice.rml"}}};

        void BuildRmlUi();
        void SyncRmlModel();
        void SyncColors(const UI::HUD::SlideDisplay& d);
        void SyncText(const wchar_t* text);

        UI::HUD::SlideTicker* m_pSlideMgr;

        // What the colours and the text were last built from: built again only on a change, not every
        // frame (the ticker keeps its text in one buffer it edits in place, so the text is compared).
        int m_BandColorAlpha = -1;
        unsigned m_TextColorRgb = 0;
        int m_TextColorAlpha = -1;
        bool m_TextBuilt = false;
        std::wstring m_TextSource;
    };
}

