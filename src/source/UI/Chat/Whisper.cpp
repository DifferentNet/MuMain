#include "stdafx.h"
#include "UI/Chat/Whisper.h"

#include "Engine/Object/ZzzInfomation.h"     // CharacterAttribute
#include "UI/Core/WindowSystem.h"            // g_pSystemLogBox
#include "UI/HUD/ChatLogWindow.h" // mu::ui::window::TYPE_SYSTEM_MESSAGE
#include "I18N/All.h"
#include "Render/Text/CUIRenderText.h"

namespace
{
    constexpr int WHISPER_ID_SLOTS = 10;

    int     WhisperID_Num = 0;
    // One name per slot, each with room for its terminator.
    wchar_t WhisperRegistID[WHISPER_ID_SLOTS][MAX_USERNAME_SIZE + 1];
    static_assert(sizeof(WhisperRegistID[0]) == sizeof(wchar_t) * (MAX_USERNAME_SIZE + 1),
                  "each slot holds one whole name");
}

namespace UI::Chat::Whisper
{
    bool CheckLevel(int requiredLevel, wchar_t* targetId)
    {
        int level = CharacterAttribute->Level;

        if (level >= requiredLevel)
            return true;

        for (int i = 0; i < WHISPER_ID_SLOTS; ++i)
        {
            if (wcscmp(targetId, WhisperRegistID[i]) == 0)
            {
                return true;
            }
        }

        g_pSystemLogBox->AddText(I18N::Game::YouCanUseTheWhisperCommandAtCharacterLevel6, mu::ui::window::TYPE_SYSTEM_MESSAGE);

        return false;
    }

    void Register(int requiredLevel, wchar_t* targetId)
    {
        int level = CharacterAttribute->Level;

        if (level < requiredLevel)
        {
            bool noMatch = true;
            for (int i = 0; i < WHISPER_ID_SLOTS; ++i)
            {
                if (wcscmp(targetId, WhisperRegistID[i]) == 0)
                {
                    noMatch = false;
                    break;
                }
            }

            if (noMatch)
            {
                wchar_t* slot = WhisperRegistID[WhisperID_Num];
                wcsncpy(slot, targetId, MAX_USERNAME_SIZE);
                slot[MAX_USERNAME_SIZE] = L'\0';
                WhisperID_Num++;

                if (WhisperID_Num >= WHISPER_ID_SLOTS)
                {
                    WhisperID_Num = 0;
                }
            }
        }
    }

    void Clear()
    {
        ZeroMemory(WhisperRegistID, sizeof(WhisperRegistID));
        WhisperID_Num = 0;
    }

    void RenderList()
    {
        for (int i = 0; i < WHISPER_ID_SLOTS; ++i)
        {
            g_pRenderText->SetTextColor(255, 255, 255, 255);
            g_pRenderText->SetBgColor(0, 0, 0, 255);
            g_pRenderText->RenderText(100, 100 + (i * 10), WhisperRegistID[i]);
        }
    }
}
