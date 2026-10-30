#include "stdafx.h"
#include "RmlNativeTextFit.h"
#include "RmlTheme.h"
#include "UI/Scaling/UITransform.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Property.h>
#include <RmlUi/Core/TextShapingContext.h>

#include <algorithm>
#include <functional>
#include <string_view>
#include <unordered_map>

namespace UI::RmlBridge
{
namespace
{
const Rml::String kNativeFitClass = "native-fit";
const Rml::String kNativeFitSmallClass = "native-fit-small";

// How far below the renderer's smallest size a .native-fit-small element may shrink.
constexpr float kSmallFitFloor = 0.75f;

// What an element was last fitted for; unchanged inputs skip the measurement.
struct FitState
{
    Rml::ObserverPtr<Rml::Element> element; // expires with the element; guards a reused address
    size_t textHash = 0;
    float boxWidth = 0.f;
    float nativePx = 0.f;
    float scale = 1.f; // the element's own font-size in em; 1 = none set
};

std::unordered_map<Rml::Element*, FitState>& FitStates()
{
    static std::unordered_map<Rml::Element*, FitState> states;
    return states;
}

bool IsHidden(Rml::Element* element)
{
    return element->GetComputedValues().display() == Rml::Style::Display::None;
}

// The text of the element and of its shown child elements, and which of those are shown.
void HashText(Rml::Element* element, size_t& hash)
{
    for (int i = 0; i < element->GetNumChildren(); ++i)
    {
        Rml::Element* child = element->GetChild(i);
        if (const auto* text = dynamic_cast<const Rml::ElementText*>(child))
            hash = hash * 31 + std::hash<std::string_view>{}(text->GetText());
        else if (IsHidden(child))
            hash = hash * 31 + 1;
        else
        {
            hash = hash * 31 + 2;
            HashText(child, hash);
        }
    }
}

// The element's text at `nativePx`, its size without the fit, and the horizontal margins, borders and padding of
// the child elements it is split into (a row of differently coloured parts), which the fit does not shrink.
struct TextWidth
{
    float text = 0.f;
    float frames = 0.f;
};

void MeasureTextWidth(Rml::Element* element, float nativePx, TextWidth& width)
{
    const auto& values = element->GetComputedValues();
    Rml::FontEngineInterface* fonts = Rml::GetFontEngineInterface();
    const Rml::FontFaceHandle face = fonts->GetFontFaceHandle(values.font_family(), values.font_style(),
                                                              values.font_weight(), static_cast<int>(nativePx));
    const Rml::TextShapingContext shaping{values.language(), values.direction(), values.font_kerning(),
                                          values.letter_spacing()};
    for (int i = 0; i < element->GetNumChildren(); ++i)
    {
        Rml::Element* child = element->GetChild(i);
        if (const auto* text = dynamic_cast<const Rml::ElementText*>(child))
        {
            if (face != 0)
                width.text += static_cast<float>(fonts->GetStringWidth(face, text->GetText(), shaping));
        }
        else if (!IsHidden(child))
        {
            width.frames += child->GetBox().GetSizeAcross(Rml::BoxDirection::Horizontal, Rml::BoxArea::Margin,
                                                          Rml::BoxArea::Padding);
            MeasureTextWidth(child, nativePx, width);
        }
    }
}

// Returns whether the element's font-size changed.
bool FitElement(Rml::Element* element)
{
    const Rml::Element* parent = element->GetParentNode();
    const float boxWidth = element->GetBox().GetSize(Rml::BoxArea::Content).x;
    if (parent == nullptr || boxWidth <= 0.f)
        return false;

    const float nativePx = parent->GetComputedValues().font_size();
    size_t textHash = 0;
    HashText(element, textHash);
    FitState& state = FitStates()[element];
    const bool sameElement = state.element.get() == element;
    if (sameElement && state.textHash == textHash && state.boxWidth == boxWidth && state.nativePx == nativePx)
        return false;

    const float previousScale = sameElement ? state.scale : 1.f;
    TextWidth width;
    MeasureTextWidth(element, nativePx, width);
    float minimumPx = UI::Scaling::MinimumTextPixelSize(UI::Scaling::FontRole::Normal);
    if (element->IsClassSet(kNativeFitSmallClass))
        minimumPx *= kSmallFitFloor;
    const float fittedPx = UI::Scaling::FitTextPixelSizeToWidth(nativePx, width.text,
                                                                std::max(boxWidth - width.frames, 1.f), minimumPx);
    const float scale = nativePx > 0.f ? fittedPx / nativePx : 1.f;
    state = {element->GetObserverPtr(), textHash, boxWidth, nativePx, scale};

    if (scale >= 1.f)
    {
        if (previousScale >= 1.f)
            return false;
        element->RemoveProperty(Rml::PropertyId::FontSize);
        return true;
    }
    if (scale == previousScale)
        return false;
    element->SetProperty(Rml::PropertyId::FontSize, Rml::Property(scale, Rml::Unit::EM));
    return true;
}

// Returns whether any font-size under `element` changed; skips hidden subtrees.
bool FitSubtree(Rml::Element* element)
{
    if (element->GetComputedValues().display() == Rml::Style::Display::None)
        return false;

    bool changed = element->IsClassSet(kNativeFitClass) && FitElement(element);
    for (int i = 0; i < element->GetNumChildren(); ++i)
        changed |= FitSubtree(element->GetChild(i));
    return changed;
}

void ForgetRemovedElements()
{
    std::erase_if(FitStates(), [](const auto& entry) { return entry.second.element.get() != entry.first; });
}
} // namespace

void FitNativeTextToBoxes(Rml::Context* context)
{
    if (!context || !ThemeUsesNativeTextSize())
        return;

    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (document->IsVisible() && FitSubtree(document))
            document->UpdateDocument();
    }
    ForgetRemovedElements();
}
} // namespace UI::RmlBridge
