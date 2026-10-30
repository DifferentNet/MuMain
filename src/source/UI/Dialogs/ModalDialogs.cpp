#include "stdafx.h"
#include "UI/Dialogs/ModalDialogs.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "UI/Dialogs/MessageBox.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

namespace
{
// What makes an RmlUi element a button: a click bound in the document, or the themes' button class.
constexpr const char* ClickAttribute = "data-event-click";
constexpr const char* ButtonClass = "btn";

bool IsButton(Rml::Element& element)
{
    return element.HasAttribute(ClickAttribute) || element.IsClassSet(ButtonClass);
}
} // namespace

namespace UI::Dialogs::Modal
{
bool IsOpen()
{
    using mu::ui::window::g_pGenericConfirmDialog;
    using mu::ui::window::g_pGenericMenuDialog;
    return !g_MessageBox->IsEmpty() || (g_pGenericConfirmDialog != nullptr && g_pGenericConfirmDialog->IsVisible()) ||
           (g_pGenericMenuDialog != nullptr && g_pGenericMenuDialog->IsVisible());
}

bool IsPointerOnButton()
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
        return false;
    for (Rml::Element* element = context->GetHoverElement(); element != nullptr; element = element->GetParentNode())
    {
        if (IsButton(*element))
            return true;
    }
    return false;
}
} // namespace UI::Dialogs::Modal
