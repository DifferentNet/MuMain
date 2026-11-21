#pragma once

namespace Rml
{
class Context;
class ElementDocument;
} // namespace Rml

// Letters warmed into the font faces of the shown windows (round 29). RmlUi adds a letter to a face
// the first time a text of that face holds it, and builds the face's whole atlas again then: each
// new letter in a chat line rebuilt the chat's atlas mid-game. Each face the shown documents use
// gets Cyrillic (the server's players write Russian whatever the interface's language), the
// interface language's own letters and the HUD's symbols once, and its atlas is built at once, under
// the first frame the document shows (the world's first frame for the HUD and the chat).
namespace UI::RmlBridge
{
// Warms the faces of the documents shown since the last call. Call after the main context's
// Update(), before it renders: a document is walked once (again after the language or the UI scale
// changed), the shown ones again once the window's size or a document's own font size changed and
// stayed put a moment, and once a second the warmed faces are checked for a rebuild, which is logged.
void WarmShownFontFaces(Rml::Context& context);

// Walks the document again over the next few frames: a panel just opened in it shows lists its data
// fills in (the touch controls' skill picker and Social panel), whose text may bring new faces.
// `reason` is a string literal, for the log line the walks end with.
void RewalkFontFacesOf(Rml::ElementDocument* document, const char* reason);

// Drops every face and document kept, before RmlUi shuts down.
void ForgetWarmedFontFaces();
} // namespace UI::RmlBridge
