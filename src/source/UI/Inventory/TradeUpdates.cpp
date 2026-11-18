#include "stdafx.h"

#include "UI/Inventory/TradeUpdates.h"

#include "Engine/Object/ZzzInventory.h"
#include "I18N/All.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/Trade.h"
#include "UI/Inventory/TradeOfferChange.h"

#include <string>

namespace UI::Trade
{
bool RequestReceived(std::wstring_view requester)
{
    return g_pTrade->ProcessToReceiveTradeRequest(std::wstring(requester).c_str());
}

void RequestAnswered(RequestReply reply, const Partner& partner)
{
    g_pTrade->ProcessToReceiveTradeResult(reply, partner);
}

void OwnItemPlaced(int slot, std::span<const std::uint8_t> itemData)
{
    g_pTrade->ProcessToReceiveTradeItems(slot, itemData);
}

void PartnerItemAdded(int slot, std::span<const std::uint8_t> itemData)
{
    g_pTrade->ProcessToReceiveYourItemAdd(static_cast<BYTE>(slot), itemData);
}

void PartnerItemRemoved(int slot)
{
    g_pTrade->ProcessToReceiveYourItemDelete(static_cast<BYTE>(slot));
}

void OwnGoldAnswered(bool accepted)
{
    g_pTrade->ProcessToReceiveMyTradeGold(accepted ? 1 : 0);
}

void PartnerGoldChanged(int gold)
{
    const int before = g_pTrade->GetYourTradeGold();
    g_pTrade->SetYourTradeGold(gold);

    if (!IsZenLowered(before, gold))
        return;

    // The classic trick: 10,000,000 becomes 1,000,000 while the player looks at his own bag.
    wchar_t partner[MAX_USERNAME_SIZE + 1] = {};
    g_pTrade->GetYourID(partner);
    wchar_t beforeText[64] = {};
    wchar_t afterText[64] = {};
    ::ConvertGold(before, beforeText);
    ::ConvertGold(gold, afterText);
    wchar_t line[256] = {};
    mu_swprintf(line, I18N::Game::TradePartnerLoweredZen, partner, beforeText, afterText);
    UI::Chat::PostSystem(line, mu::ui::window::TYPE_ERROR_MESSAGE);
}

void PartnerConfirmChanged(PartnerConfirm state)
{
    g_pTrade->ProcessToReceiveYourConfirm(state);
}

void Closed(CloseReason reason)
{
    g_pTrade->ProcessToReceiveTradeExit(reason);
}
}
