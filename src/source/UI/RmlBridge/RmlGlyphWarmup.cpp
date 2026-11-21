#include "stdafx.h"
#include "UI/RmlBridge/RmlGlyphWarmup.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "I18N/All.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/FontMetrics.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/TextShapingContext.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace UI::RmlBridge
{
namespace
{
// The letters players write in chat whatever the interface's language, and the HUD's symbols beyond
// ASCII. ASCII is in every face from the start.
constexpr const char* kChatLetters = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                                     "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";
constexpr const char* kHudSymbols = "×·—–…«»№→°±";

// The interface language's own letters beyond ASCII and Cyrillic. Japanese and Chinese are left out:
// their thousands of signs come from the fallback faces as they are drawn.
struct LocaleLetters
{
    const char* locale;
    const char* letters;
};
constexpr LocaleLetters kLocaleLetters[] = {
    {"de", "ÄÖÜäöüß"},
    {"es", "ÁÉÍÑÓÚÜáéíñóúü¡¿"},
    {"pl", "ĄĆĘŁŃÓŚŹŻąćęłńóśźż"},
    {"pt", "ÀÁÂÃÇÉÊÍÓÔÕÚàáâãçéêíóôõú"},
    {"tl", "Ññ"},
    {"uk", "ҐЄІЇґєії"},
};

// Faces above this size are titles and big numbers, which show the interface's own words, not chat:
// their glyphs are large and they are left to add letters as they come.
constexpr int kLargestWarmedPx = 40;

// How often the warmed faces are asked whether their atlas was built again.
constexpr double kRebuildCheckSeconds = 1.0;

// How long a size has to stay put before the documents are walked again for its faces: a window
// dragged to a new size, or the Button size stepped through, does not walk every frame.
constexpr double kSettleSeconds = 0.25;

// The frames a document is walked again after a panel opened in it: the lists the panel shows (the
// skill picker's skills, the Social panel's players) are filled in by its data over them.
constexpr int kOpenedPanelWalks = 4;

// The layer configuration of a face without font effects (FontEngineInterface::GenerateString).
constexpr Rml::FontEffectsHandle kPlainLayers = 0;

struct WalkedDocument
{
    // Expires with the document, so another document at a reused address is walked again.
    Rml::ObserverPtr<Rml::Element> document;
    // The document's own font size when last walked: touch_controls.rml sets it to the finger unit,
    // which follows the Button size and the screen, and every text size in it follows.
    float fontSize = 0.f;
};

// A walk asked for again, of one document or of every shown one, made once it is due; one line
// logs it after its last walk.
struct Rewalk
{
    Rml::ObserverPtr<Rml::Element> document;
    bool everyDocument = false;
    const char* reason = "";
    double dueAt = 0.0;
    int walksLeft = 1;
    int warmed = 0;
    int larger = 0;
    double ms = 0.0;
};

struct WarmupState
{
    // The documents walked since the language or the UI scale last changed, by address.
    std::unordered_map<const Rml::Element*, WalkedDocument> walkedDocuments;
    // Each warmed face and its atlas version when last seen: a later version is a rebuild.
    std::unordered_map<Rml::FontFaceHandle, int> faceVersions;
    // The faces above kLargestWarmedPx met so far, left as they are.
    std::unordered_set<Rml::FontFaceHandle> largeFaces;
    std::vector<Rewalk> rewalks;
    std::string locale;
    float dpRatio = 0.f;
    Rml::Vector2i dimensions{0, 0};
    size_t shownSignature = 0;
    double nextRebuildCheck = 0.0;
};

struct WarmResult
{
    int warmed = 0;
    int larger = 0;
};

WarmupState& State()
{
    static WarmupState state;
    return state;
}

double Now()
{
    return Rml::GetSystemInterface()->GetElapsedTime();
}

double MillisecondsSince(std::chrono::steady_clock::time_point started)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

Rml::String WarmupLetters(const std::string& locale)
{
    Rml::String letters = kChatLetters;
    letters += kHudSymbols;
    for (const LocaleLetters& entry : kLocaleLetters)
    {
        if (locale == entry.locale)
            letters += entry.letters;
    }
    return letters;
}

// A new language brings its letters into every face again; a new UI scale brings new face sizes.
void ResetOnLanguageOrScale(WarmupState& state, Rml::Context& context)
{
    const char* locale = I18N::GetCurrentLocale();
    const float dpRatio = context.GetDensityIndependentPixelRatio();
    if (state.locale == locale && state.dpRatio == dpRatio)
        return;
    if (state.locale != locale)
    {
        state.faceVersions.clear();
        state.largeFaces.clear();
    }
    state.locale = locale;
    state.dpRatio = dpRatio;
    state.walkedDocuments.clear();
    state.shownSignature = 0;
}

// Which documents are shown: a change asks for a walk of the ones not walked yet.
size_t ShownSignature(Rml::Context& context)
{
    size_t signature = 1;
    for (int i = 0; i < context.GetNumDocuments(); ++i)
    {
        const Rml::ElementDocument* document = context.GetDocument(i);
        if (document->IsVisible())
            signature = signature * 31 + std::hash<const void*>{}(document);
    }
    return signature;
}

// Returns false if the document was walked already.
bool MarkWalked(WarmupState& state, Rml::ElementDocument* document)
{
    const float fontSize = document->GetComputedValues().font_size();
    auto [entry, added] =
        state.walkedDocuments.try_emplace(document, WalkedDocument{document->GetObserverPtr(), fontSize});
    if (added)
        return true;
    if (entry->second.document.get() == document)
        return false;
    entry->second = WalkedDocument{document->GetObserverPtr(), fontSize};
    return true;
}

void CollectFaces(Rml::Element* element, const WarmupState& state, std::unordered_set<Rml::FontFaceHandle>& faces)
{
    const Rml::FontFaceHandle face = element->GetFontFaceHandle();
    if (face != 0 && !state.faceVersions.contains(face) && !state.largeFaces.contains(face))
        faces.insert(face);
    for (int i = 0; i < element->GetNumChildren(); ++i)
        CollectFaces(element->GetChild(i), state, faces);
}

std::unordered_set<Rml::FontFaceHandle> UnwarmedFacesOfNewDocuments(WarmupState& state, Rml::Context& context)
{
    std::unordered_set<Rml::FontFaceHandle> faces;
    for (int i = 0; i < context.GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context.GetDocument(i);
        if (document->IsVisible() && MarkWalked(state, document))
            CollectFaces(document, state, faces);
    }
    return faces;
}

void WarmFace(Rml::FontEngineInterface& fonts, Rml::RenderManager& renderManager, Rml::FontFaceHandle face,
              const Rml::String& letters)
{
    static const Rml::String kNoLanguage;
    const Rml::TextShapingContext shaping{kNoLanguage};
    fonts.GetStringWidth(face, letters, shaping);

    // Builds the atlas with the new letters now, not when a text of the face next changes.
    Rml::TexturedMeshList meshes;
    fonts.GenerateString(renderManager, face, kPlainLayers, " ", Rml::Vector2f(0.f), Rml::ColourbPremultiplied(),
                         1.f, shaping, meshes);
}

WarmResult WarmFaces(WarmupState& state, Rml::Context& context, const std::unordered_set<Rml::FontFaceHandle>& faces)
{
    WarmResult result;
    Rml::FontEngineInterface* fonts = Rml::GetFontEngineInterface();
    if (!fonts || faces.empty())
        return result;
    const Rml::String letters = WarmupLetters(state.locale);
    for (const Rml::FontFaceHandle face : faces)
    {
        if (fonts->GetFontMetrics(face).size > kLargestWarmedPx)
        {
            state.largeFaces.insert(face);
            ++result.larger;
            continue;
        }
        WarmFace(*fonts, context.GetRenderManager(), face, letters);
        state.faceVersions[face] = fonts->GetVersion(face);
        ++result.warmed;
    }
    return result;
}

void WarmNewDocuments(WarmupState& state, Rml::Context& context)
{
    const std::unordered_set<Rml::FontFaceHandle> faces = UnwarmedFacesOfNewDocuments(state, context);
    if (faces.empty())
        return;

    const auto started = std::chrono::steady_clock::now();
    const WarmResult result = WarmFaces(state, context, faces);
    mu::log::Get("ui")->info("[RmlUi fonts] letters warmed into {} new faces in {:.1f} ms ({} larger ones left)",
                             result.warmed, MillisecondsSince(started), result.larger);
}

// Asks for a walk again, `walks` frames from `dueAt`; asked again for the same cause, the walk waits
// from the new time (a size still moving).
void AskRewalk(WarmupState& state, Rml::Element* document, const char* reason, double dueAt, int walks)
{
    const bool everyDocument = document == nullptr;
    for (Rewalk& rewalk : state.rewalks)
    {
        if (rewalk.everyDocument == everyDocument && rewalk.document.get() == document &&
            std::string_view(rewalk.reason) == reason)
        {
            rewalk.dueAt = dueAt;
            rewalk.walksLeft = std::max(rewalk.walksLeft, walks);
            return;
        }
    }
    Rewalk rewalk;
    if (document != nullptr)
        rewalk.document = document->GetObserverPtr();
    rewalk.everyDocument = everyDocument;
    rewalk.reason = reason;
    rewalk.dueAt = dueAt;
    rewalk.walksLeft = walks;
    state.rewalks.push_back(rewalk);
}

// Text sizes change without a new UI scale: with the window's size (the chat's readable size on a
// phone follows the screen), and with a document's own font size (the touch controls' finger unit,
// set by the Button size and the screen). The faces of the new sizes are warmed once it stays put.
void WatchSizes(WarmupState& state, Rml::Context& context, double now)
{
    const Rml::Vector2i dimensions = context.GetDimensions();
    if (dimensions != state.dimensions)
    {
        const bool first = state.dimensions == Rml::Vector2i(0, 0);
        state.dimensions = dimensions;
        if (!first)
            AskRewalk(state, nullptr, "window size", now + kSettleSeconds, 1);
    }
    for (int i = 0; i < context.GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context.GetDocument(i);
        if (!document->IsVisible())
            continue;
        const auto entry = state.walkedDocuments.find(document);
        if (entry == state.walkedDocuments.end() || entry->second.document.get() != document)
            continue;
        const float fontSize = document->GetComputedValues().font_size();
        if (fontSize == entry->second.fontSize)
            continue;
        entry->second.fontSize = fontSize;
        AskRewalk(state, document, "font size", now + kSettleSeconds, 1);
    }
}

// The document's file name for the log ("touch_controls.rml").
Rml::String DocumentName(const Rml::Element* element)
{
    const Rml::ElementDocument* document = element->GetOwnerDocument();
    if (document == nullptr)
        return "a document";
    const Rml::String& url = document->GetSourceURL();
    const size_t slash = url.find_last_of("/\\");
    return slash == Rml::String::npos ? url : url.substr(slash + 1);
}

// Makes the walks that are due; a walk only warms the faces no walk met before.
void RunRewalks(WarmupState& state, Rml::Context& context, double now)
{
    for (auto it = state.rewalks.begin(); it != state.rewalks.end();)
    {
        Rewalk& rewalk = *it;
        if (now < rewalk.dueAt)
        {
            ++it;
            continue;
        }
        Rml::Element* document = rewalk.document.get();
        if (!rewalk.everyDocument && document == nullptr)
        {
            it = state.rewalks.erase(it);
            continue;
        }

        const auto started = std::chrono::steady_clock::now();
        std::unordered_set<Rml::FontFaceHandle> faces;
        if (rewalk.everyDocument)
        {
            for (int i = 0; i < context.GetNumDocuments(); ++i)
            {
                Rml::ElementDocument* shown = context.GetDocument(i);
                if (shown->IsVisible())
                    CollectFaces(shown, state, faces);
            }
        }
        else if (document->IsVisible())
        {
            CollectFaces(document, state, faces);
        }
        const WarmResult result = WarmFaces(state, context, faces);
        rewalk.ms += MillisecondsSince(started);
        rewalk.warmed += result.warmed;
        rewalk.larger += result.larger;
        if (--rewalk.walksLeft > 0)
        {
            ++it;
            continue;
        }
        const Rml::String walked = rewalk.everyDocument ? Rml::String("the shown documents") : DocumentName(document);
        mu::log::Get("ui")->info(
            "[RmlUi fonts] {} walked again ({}): {} new faces warmed in {:.2f} ms ({} larger ones left)", walked,
            rewalk.reason, rewalk.warmed, rewalk.ms, rewalk.larger);
        it = state.rewalks.erase(it);
    }
}

// Logs how often the warmed faces' atlases were built again since the last check: a letter none of
// them had came in (0 is the aim, so nothing is logged then).
void CheckRebuilds(WarmupState& state, double now)
{
    if (now < state.nextRebuildCheck)
        return;
    state.nextRebuildCheck = now + kRebuildCheckSeconds;

    Rml::FontEngineInterface* fonts = Rml::GetFontEngineInterface();
    if (!fonts)
        return;
    int rebuilds = 0;
    for (auto& [face, version] : state.faceVersions)
    {
        const int current = fonts->GetVersion(face);
        rebuilds += current - version;
        version = current;
    }
    if (rebuilds > 0)
        mu::log::Get("ui")->info("[RmlUi fonts] {} atlas rebuild(s) of warmed faces in the last second", rebuilds);
}
} // namespace

void WarmShownFontFaces(Rml::Context& context)
{
    WarmupState& state = State();
    const double now = Now();
    ResetOnLanguageOrScale(state, context);
    const size_t signature = ShownSignature(context);
    if (signature != state.shownSignature)
    {
        state.shownSignature = signature;
        WarmNewDocuments(state, context);
    }
    WatchSizes(state, context, now);
    RunRewalks(state, context, now);
    CheckRebuilds(state, now);
}

void RewalkFontFacesOf(Rml::ElementDocument* document, const char* reason)
{
    if (document == nullptr)
        return;
    AskRewalk(State(), document, reason, Now(), kOpenedPanelWalks);
}

void ForgetWarmedFontFaces()
{
    State() = WarmupState{};
}
} // namespace UI::RmlBridge
