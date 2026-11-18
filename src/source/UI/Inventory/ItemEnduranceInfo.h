
#if !defined(AFX_NEWUIITEMENDURANCEINFO_H__ADB04FC1_C3E3_47B5_8026_C78C5800500C__INCLUDED_)
#define AFX_NEWUIITEMENDURANCEINFO_H__ADB04FC1_C3E3_47B5_8026_C78C5800500C__INCLUDED_

#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Inventory/ItemEnduranceRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/ObserverPtr.h>

#include <vector>

namespace Rml
{
class Element;
class ElementDocument;
}

namespace mu::ui::window
{
    class CItemEnduranceInfo : public CObject
    {
    protected:
        enum IMAGE_LIST
        {
            IMAGE_PETHP_FRAME = BITMAP_ITEM_ENDURANCE_INFO_BEGIN,
            IMAGE_PETHP_BAR,
            IMAGE_ITEM_DUR_BOOTS,
            IMAGE_ITEM_DUR_CAP,
            IMAGE_ITEM_DUR_GLOVES,
            IMAGE_ITEM_DUR_LOWER,
            IMAGE_ITEM_DUR_NECKLACE,
            IMAGE_ITEM_DUR_RING,
            IMAGE_ITEM_DUR_SHIELD,
            IMAGE_ITEM_DUR_UPPER,
            IMAGE_ITEM_DUR_WEAPON,
            IMAGE_ITEM_DUR_WING,
        };

        enum IMAGE_SIZE
        {
            PETHP_BAR_WIDTH = 49,
        };

        enum ITEM_DUR_ARROW_TYPE
        {
            ARROWTYPE_NONE = -1,
            ARROWTYPE_BOW = ITEM_BOW + 15,
            ARROWTYPE_CROSSBOW = ITEM_BOW + 7,
        };

    private:
        CManager* m_pNewUIMng;

        int							m_iCurArrowType;
        int							m_iItemDurImageIndex[MAX_EQUIPMENT];
        int							m_iTooltipIndex;
        // The equipment slot of each icon synced, in document order (UpdateMouseEvent()).
        std::vector<int> m_IconSlots;
        // The hit boxes UpdateMouseEvent() tests: the pet frames and the icon tints, in document order. Queried
        // again only when the document changed or the synced rows changed (RefreshHitBoxes()).
        std::vector<Rml::ObserverPtr<Rml::Element>> m_PetFrames;
        std::vector<Rml::ObserverPtr<Rml::Element>> m_IconTints;
        Rml::ObserverPtr<Rml::Element> m_HitBoxDocument;
        int m_HitBoxQueriesLeft = 0;

    public:
        CItemEnduranceInfo();
        virtual ~CItemEnduranceInfo();

        bool Create(CManager* pNewUIMng);
        void Release();


        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        bool BtnProcess();

        float GetLayerDepth();	//. 3.5f

        void OpenningProcess();
        void ClosingProcess();

        // Hides the document outside the main scene (CSystem::SyncMainSceneHudVisibility()).
        void SyncDocVisibility(bool sceneAllowsShow);

    private:
        // Right-aligns the durability icons to the world the open windows leave uncovered.

        void InitImageIndex();



        // The HUD in RmlUi (item_endurance.rml): main context, behind its other documents (the
        // original drew it at layer depth 3.5, under the panels). Render() fills it; the native
        // drawing is the fallback when RmlUi is not available.
        void BuildRmlUi();
        void SyncView();
        void SyncLeftColumn();
        void SyncIcons();
        // The image name of an IMAGE_ITEM_DUR_* durability icon; empty for none.
        static const char* DurabilityIconImage(int imageIndex);
        void SyncTooltip();
        // The rows changed: query the hit boxes again once RmlUi has rebuilt them.
        void MarkHitBoxesStale();
        void RefreshHitBoxes(Rml::ElementDocument* document);

        void BindRmlModel(Rml::DataModelConstructor& c, UI::ItemEndurance::ItemEnduranceRmlModel& model);
        UI::RmlBridge::ThemedView<UI::ItemEndurance::ItemEnduranceRmlModel> m_RmlView{"item_endurance",
            [this](Rml::DataModelConstructor& c, UI::ItemEndurance::ItemEnduranceRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/item_endurance.rml"}}};
        bool m_sceneAllowsShow = false;
    };
}

#endif // !defined(AFX_NEWUIITEMENDURANCEINFO_H__ADB04FC1_C3E3_47B5_8026_C78C5800500C__INCLUDED_)
