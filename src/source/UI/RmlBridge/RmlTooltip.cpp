#include "stdafx.h"
#include "RmlTooltip.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlTooltipPlacement.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Scaling/UITransform.h"
#include "UI/Tooltip/ItemTooltipLayout.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementUtilities.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace UI::RmlBridge::Tooltip
{
    namespace
    {
        // Bool-per-color flags, matching every other themed document's own data-class-* binding
        // convention (RmlUi's DataModelConstructor has no enum-to-class mapping) -- White is the
        // implicit default when no flag is set, same convention MainFrameWindow's
        // SkillTooltipLineEntry already uses.
        struct TooltipLineEntry
        {
            Rml::String text;
            bool colorBlue = false;
            bool colorGray = false;
            bool colorRed = false;
            bool colorYellow = false;
            bool colorGreen = false;
            bool colorPurple = false;
            bool colorRedPurple = false;
            bool colorViolet = false;
            bool colorOrange = false;
            bool highlightDarkRed = false;
            bool highlightDarkBlue = false;
            bool highlightDarkYellow = false;
            bool highlightGreenBlue = false;
            bool bold = false;
            bool isHalfSpacer = false;
            bool isFullSpacer = false;
            // The rich item layout's line roles (Line::role) and header flags; all false otherwise.
            bool roleName = false;
            bool roleKind = false;
            bool roleCp = false;
            bool roleSection = false;
            bool rolePieceOwned = false;
            bool rolePieceMissing = false;
            bool roleHint = false;
            bool head = false;
            bool headLast = false;

            bool operator==(const TooltipLineEntry&) const = default;
        };

        struct TooltipRmlModel
        {
            std::vector<TooltipLineEntry> lines;
            float posX = 0.0f;
            float posY = 0.0f;
            bool centerText = false;
            // What the native text renderer would use under the caller's transform -- the legacy
            // theme's tooltip.rml binds these to match RenderTipTextList(); others may ignore them.
            float textPx = 0.0f;
            float borderPx = 0.0f;
            float paddingPx = 0.0f;
            float fixedWidthPx = 0.0f;
            // The native renderer's line heights, which the theme spaces the rows from.
            float lineHeightPx = 0.0f;
            float boldLineHeightPx = 0.0f;
            bool buttonHint = false;
            // The rich item layout (Config::rich): the name's size and one flag per rarity tier (none
            // for Normal) and for the +13..+15 band.
            bool rich = false;
            float namePx = 0.0f;
            bool rarMagic = false;
            bool rarFine = false;
            bool rarExc = false;
            bool rarAnc = false;
            bool rarSocket = false;
            bool rarSpecial = false;
            bool rarDivine = false;
            bool upGold = false;
            // A rich tooltip still taller than the window at its smallest text (RichFit): no shadow, no glow.
            bool tall = false;
        };

        // The rich layout's sizes that follow its text size, which fitting it to the window's height scales.
        struct RichMetrics
        {
            float textPx = 0.0f;
            float namePx = 0.0f;
            float paddingPx = 0.0f;
            float lineHeightPx = 0.0f;
            float boldLineHeightPx = 0.0f;
        };

        // RenderTipTextList() (ZzzInventory.cpp) layout: the box is the widest line plus 4 units
        // (2 per side) with no vertical padding, framed by a 1-unit border. The rows' spacing is the
        // theme's.
        constexpr float kNativePaddingUnits = 2.0f;
        constexpr float kNativeBorderUnits = 1.0f;
        // A button's hover text (CNewUIButton::Render()): a box 6 units wider than the text, no frame.
        constexpr float kButtonHintPaddingUnits = 3.0f;
        // The rich item layout: room for its header bar and frame, and the name a size larger.
        constexpr float kRichPaddingUnits = 8.0f;
        constexpr float kRichNameScale = 1.3f;

        // Row heights come from the native text renderer itself (MeasureText's logical height,
        // as RenderTipTextList() uses), not from RmlUi's font metrics, which round differently.
        void ApplyNativeMetrics(TooltipRmlModel& model, const Config& config)
        {
            const UI::Scaling::Transform transform = config.transform.value_or(UI::Scaling::TypographyUnitsTransform(
                static_cast<int>(WindowWidth), static_cast<int>(WindowHeight)));
            model.centerText = (config.textAlign == Config::TextAlign::Center);
            model.fixedWidthPx = config.fixedWidth * transform.scaleX;
            model.textPx = UI::Scaling::NativeTextPixelSize(UI::Scaling::FontRole::Normal, transform);
            model.buttonHint = (config.box == Config::Box::ButtonHint);
            model.borderPx = model.buttonHint ? 0.0f : kNativeBorderUnits * transform.scaleX;
            model.paddingPx = (model.buttonHint ? kButtonHintPaddingUnits : kNativePaddingUnits) * transform.scaleX;

            // The native line height follows the active transform: measure under the chosen one.
            const UI::Scaling::ScopedActiveTransform measureScope(transform);
            model.lineHeightPx =
                static_cast<float>(CUIRenderTextSDLTtf::LineHeight(UI::Scaling::FontRole::Normal)) * transform.scaleY;
            model.boldLineHeightPx =
                static_cast<float>(CUIRenderTextSDLTtf::LineHeight(UI::Scaling::FontRole::Bold)) * transform.scaleY;
            if (config.rich)
                model.paddingPx = kRichPaddingUnits * transform.scaleX;
        }

        void ApplyRichLayout(TooltipRmlModel& model, const Config& config)
        {
            using GameLogic::Items::Rarity::Tier;
            model.rich = config.rich;
            if (!config.rich)
                return;

            model.namePx = std::round(model.textPx * kRichNameScale);
            model.rarMagic = config.rarity == Tier::Magic;
            model.rarFine = config.rarity == Tier::Fine;
            model.rarExc = config.rarity == Tier::Excellent;
            model.rarAnc = config.rarity == Tier::Ancient;
            model.rarSocket = config.rarity == Tier::Socket;
            model.rarSpecial = config.rarity == Tier::Special;
            model.rarDivine = config.rarity == Tier::Divine;
            model.upGold = config.band == GameLogic::Items::Rarity::UpgradeBand::Gold;
        }

        RichMetrics MetricsOf(const TooltipRmlModel& model)
        {
            return {model.textPx, model.namePx, model.paddingPx, model.lineHeightPx, model.boldLineHeightPx};
        }

        // `base` at `scale`, the font sizes kept whole pixels.
        RichMetrics Scaled(const RichMetrics& base, float scale)
        {
            if (scale == 1.0f)
                return base;
            return {std::round(base.textPx * scale), std::round(base.namePx * scale), base.paddingPx * scale,
                    base.lineHeightPx * scale, base.boldLineHeightPx * scale};
        }

        void SetMetrics(TooltipRmlModel& model, const RichMetrics& metrics)
        {
            model.textPx = metrics.textPx;
            model.namePx = metrics.namePx;
            model.paddingPx = metrics.paddingPx;
            model.lineHeightPx = metrics.lineHeightPx;
            model.boldLineHeightPx = metrics.boldLineHeightPx;
        }

        Owner s_CurrentOwner = nullptr;
        // The showing tooltip's refreshEachFrame, and whether Show() ran since ExpireUnrefreshed().
        bool s_RefreshEachFrame = false;
        bool s_Refreshed = false;

        // #tooltip_panel as last measured: its border box and the frame edges the anchor sits inside.
        struct Measurement
        {
            Rml::Vector2f size;
            Rml::Vector2f frameTopLeft;
            float frameBottom = 0.0f;
            // The context's dp ratio the panel was laid out at.
            float dpRatio = 0.0f;
        };
        // Valid while the document holds the model's current content: hiding only changes its
        // visibility, which leaves the layout as it was. Cleared when the document is rebuilt.
        std::optional<Measurement> s_Measured;
        // The width last set on #tooltip_panel; empty once the document is rebuilt.
        std::string s_PanelWidth;
        // The scale last set on #tooltip_panel to fit the screen's height (1: none set); 1 once the
        // document is rebuilt.
        float s_FitScale = 1.0f;
        // On the phone a tooltip taller than the screen (a full excellent ancient set part) is drawn
        // smaller until it fits, but no smaller than this: below it the text no longer reads.
        constexpr float kPhoneMinFitScale = 0.65f;

        // A rich item tooltip taller than the window (an ancient's full view with its set) is laid out with smaller
        // text until all it draws, its shadow and glow too, fits the window's height less a margin; no smaller than
        // kRichMinFitScale. Taller than the window itself even there, it goes without its shadow and glow (tt-tall),
        // whose textures would outgrow the window; the margin only takes up the few pixels the fitted layout lands
        // off its estimate. Worked out when the content or the window changes, kept for the frames between.
        struct RichFit
        {
            Rml::Vector2i viewport;
            float dpRatio = 0.0f;
            float scale = 1.0f;
            bool tall = false;
        };
        std::optional<RichFit> s_RichFit;
        constexpr float kRichMinFitScale = 0.75f;
        constexpr float kRichFitMarginDp = 8.0f;

        void BindModel(Rml::DataModelConstructor& c, TooltipRmlModel& model)
        {
            auto line = c.RegisterStruct<TooltipLineEntry>();
            line.RegisterMember("text", &TooltipLineEntry::text);
            line.RegisterMember("color_blue", &TooltipLineEntry::colorBlue);
            line.RegisterMember("color_gray", &TooltipLineEntry::colorGray);
            line.RegisterMember("color_red", &TooltipLineEntry::colorRed);
            line.RegisterMember("color_yellow", &TooltipLineEntry::colorYellow);
            line.RegisterMember("color_green", &TooltipLineEntry::colorGreen);
            line.RegisterMember("color_purple", &TooltipLineEntry::colorPurple);
            line.RegisterMember("color_redpurple", &TooltipLineEntry::colorRedPurple);
            line.RegisterMember("color_violet", &TooltipLineEntry::colorViolet);
            line.RegisterMember("color_orange", &TooltipLineEntry::colorOrange);
            line.RegisterMember("highlight_darkred", &TooltipLineEntry::highlightDarkRed);
            line.RegisterMember("highlight_darkblue", &TooltipLineEntry::highlightDarkBlue);
            line.RegisterMember("highlight_darkyellow", &TooltipLineEntry::highlightDarkYellow);
            line.RegisterMember("highlight_greenblue", &TooltipLineEntry::highlightGreenBlue);
            line.RegisterMember("bold", &TooltipLineEntry::bold);
            line.RegisterMember("is_half_spacer", &TooltipLineEntry::isHalfSpacer);
            line.RegisterMember("is_full_spacer", &TooltipLineEntry::isFullSpacer);
            line.RegisterMember("role_name", &TooltipLineEntry::roleName);
            line.RegisterMember("role_kind", &TooltipLineEntry::roleKind);
            line.RegisterMember("role_cp", &TooltipLineEntry::roleCp);
            line.RegisterMember("role_section", &TooltipLineEntry::roleSection);
            line.RegisterMember("role_piece_owned", &TooltipLineEntry::rolePieceOwned);
            line.RegisterMember("role_piece_missing", &TooltipLineEntry::rolePieceMissing);
            line.RegisterMember("role_hint", &TooltipLineEntry::roleHint);
            line.RegisterMember("head", &TooltipLineEntry::head);
            line.RegisterMember("head_last", &TooltipLineEntry::headLast);
            c.RegisterArray<std::vector<TooltipLineEntry>>();

            c.Bind("lines", &model.lines);
            c.Bind("pos_x", &model.posX);
            c.Bind("pos_y", &model.posY);
            c.Bind("center_text", &model.centerText);
            c.Bind("text_px", &model.textPx);
            c.Bind("border_px", &model.borderPx);
            c.Bind("padding_px", &model.paddingPx);
            c.Bind("fixed_width_px", &model.fixedWidthPx);
            c.Bind("line_height_px", &model.lineHeightPx);
            c.Bind("bold_line_height_px", &model.boldLineHeightPx);
            c.Bind("button_hint", &model.buttonHint);
            c.Bind("rich", &model.rich);
            c.Bind("name_px", &model.namePx);
            c.Bind("rar_magic", &model.rarMagic);
            c.Bind("rar_fine", &model.rarFine);
            c.Bind("rar_exc", &model.rarExc);
            c.Bind("rar_anc", &model.rarAnc);
            c.Bind("rar_socket", &model.rarSocket);
            c.Bind("rar_special", &model.rarSpecial);
            c.Bind("rar_divine", &model.rarDivine);
            c.Bind("up_gold", &model.upGold);
            c.Bind("tall", &model.tall);
        }

        // A theme switch leaves it hidden and ownerless: whichever caller has the hover shows it
        // again on its next hover check.
        void OnReloaded();

        UI::RmlBridge::ThemedView<TooltipRmlModel> s_View{"tooltip",
                                                          BindModel,
                                                          {{"Data/Interface/RmlUi/tooltip.rml"}},
                                                          {.afterBuild =
                                                               []
                                                           {
                                                               s_Measured.reset();
                                                               s_RichFit.reset();
                                                               s_PanelWidth.clear();
                                                               s_FitScale = 1.0f;
                                                           },
                                                           .afterReload = [] { OnReloaded(); }}};

        void OnReloaded()
        {
            s_View.Hide();
            s_CurrentOwner = nullptr;
            s_Measured.reset();
            s_RichFit.reset();
        }

        TooltipLineEntry ToLineEntry(const Line& line)
        {
            TooltipLineEntry entry;
            entry.text = line.text;
            entry.bold = line.bold;
            entry.isHalfSpacer = (line.kind == Line::Kind::HalfSpacer);
            entry.isFullSpacer = (line.kind == Line::Kind::FullSpacer);
            entry.roleName = line.role == LineRole::Name;
            entry.roleKind = line.role == LineRole::Kind;
            entry.roleCp = line.role == LineRole::CombatPower;
            entry.roleSection = line.role == LineRole::Section;
            entry.rolePieceOwned = line.role == LineRole::PieceOwned;
            entry.rolePieceMissing = line.role == LineRole::PieceMissing;
            entry.roleHint = line.role == LineRole::Hint;
            entry.head = line.head;
            entry.headLast = line.headLast;

            switch (line.color)
            {
            case LineColor::Blue: entry.colorBlue = true; break;
            case LineColor::Gray: entry.colorGray = true; break;
            case LineColor::Red: entry.colorRed = true; break;
            case LineColor::Yellow: entry.colorYellow = true; break;
            case LineColor::Green: entry.colorGreen = true; break;
            case LineColor::Purple: entry.colorPurple = true; break;
            case LineColor::RedPurple: entry.colorRedPurple = true; break;
            case LineColor::Violet: entry.colorViolet = true; break;
            case LineColor::Orange: entry.colorOrange = true; break;
            case LineColor::DarkRedHighlight: entry.highlightDarkRed = true; break;
            case LineColor::DarkBlueHighlight: entry.highlightDarkBlue = true; break;
            case LineColor::DarkYellowHighlight: entry.highlightDarkYellow = true; break;
            case LineColor::GreenBlueHighlight: entry.highlightGreenBlue = true; break;
            case LineColor::White: default: break;
            }
            return entry;
        }

        // Sets a field of the shown model, marking it when its value changed; true when it did.
        template <typename Field, typename Value> bool AssignField(Field& field, Value&& value, const char* name)
        {
            if (field == value)
                return false;
            field = std::forward<Value>(value);
            s_View.Binder().MarkDirty(name);
            return true;
        }

        // Copies `next` into the model, marking only the fields whose values changed. True when any did.
        bool UpdateContent(TooltipRmlModel& model, TooltipRmlModel&& next)
        {
            bool changed = false;
            const auto assign = [&changed](auto& field, auto&& value, const char* name)
            { changed = AssignField(field, std::forward<decltype(value)>(value), name) || changed; };
            assign(model.lines, std::move(next.lines), "lines");
            assign(model.centerText, next.centerText, "center_text");
            assign(model.textPx, next.textPx, "text_px");
            assign(model.borderPx, next.borderPx, "border_px");
            assign(model.paddingPx, next.paddingPx, "padding_px");
            assign(model.fixedWidthPx, next.fixedWidthPx, "fixed_width_px");
            assign(model.lineHeightPx, next.lineHeightPx, "line_height_px");
            assign(model.boldLineHeightPx, next.boldLineHeightPx, "bold_line_height_px");
            assign(model.buttonHint, next.buttonHint, "button_hint");
            assign(model.rich, next.rich, "rich");
            assign(model.namePx, next.namePx, "name_px");
            assign(model.rarMagic, next.rarMagic, "rar_magic");
            assign(model.rarFine, next.rarFine, "rar_fine");
            assign(model.rarExc, next.rarExc, "rar_exc");
            assign(model.rarAnc, next.rarAnc, "rar_anc");
            assign(model.rarSocket, next.rarSocket, "rar_socket");
            assign(model.rarSpecial, next.rarSpecial, "rar_special");
            assign(model.rarDivine, next.rarDivine, "rar_divine");
            assign(model.upGold, next.upGold, "up_gold");
            assign(model.tall, next.tall, "tall");
            return changed;
        }

        // Puts the rich sizes and the tall flag on the shown model, marking what changed.
        void ApplyRichFit(TooltipRmlModel& model, const RichMetrics& metrics, bool tall)
        {
            AssignField(model.textPx, metrics.textPx, "text_px");
            AssignField(model.namePx, metrics.namePx, "name_px");
            AssignField(model.paddingPx, metrics.paddingPx, "padding_px");
            AssignField(model.lineHeightPx, metrics.lineHeightPx, "line_height_px");
            AssignField(model.boldLineHeightPx, metrics.boldLineHeightPx, "bold_line_height_px");
            AssignField(model.tall, tall, "tall");
        }

        // A rich line's text sits in its own element (tooltip_rich.rcss frames the Combat Power as a
        // pill): its side padding and border add to the text's width.
        float InlineFrameWidth(Rml::Element& lineElement)
        {
            Rml::Element* text = lineElement.GetFirstChild();
            if (text == nullptr)
                return 0.0f;
            const Rml::Box& box = text->GetBox();
            return box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Left) +
                   box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Right) +
                   box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Left) +
                   box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Right);
        }

        // Lays the panel out for the model's content and measures it. The context update creates the
        // data-for rows with their resolved fonts; the width they need is then set explicitly and only
        // this document is laid out again.
        Measurement Measure(Rml::Context& context, const Config& config)
        {
            context.Update();

            // #tooltip_panel must not rely on shrink-to-fit width: this build's box-width computation
            // for an absolutely-positioned block with multiple block children undersizes it (the same
            // family of bug engine-findings.md documents for a single pre-line text node -- confirmed
            // in practice by main_frame.rcss's #skill_tooltip needing an explicit, if fixed, width for
            // exactly this reason). A tooltip's width genuinely varies with content (an item tooltip's
            // longest line is nothing like a skill tooltip's), so a fixed width isn't an option here --
            // measure the real per-line text width via RmlUi's own font engine (the same one that will
            // actually draw it, so this can't drift from the real render the way a native GDI
            // measurement transplanted onto RmlUi's own font metrics could) and set an explicit `width`
            // from that instead of trusting auto-sizing.
            Measurement measured;
            measured.dpRatio = context.GetDensityIndependentPixelRatio();
            Rml::Element* panel = s_View.Document()->GetElementById("tooltip_panel");
            if (!panel)
                return measured;

            float maxLineWidth = 0.0f;
            const int lineCount = panel->GetNumChildren();
            for (int i = 0; i < lineCount && i < static_cast<int>(config.lines.size()); ++i)
            {
                const Line& line = config.lines[static_cast<size_t>(i)];
                if (line.kind != Line::Kind::Text)
                    continue; // spacer lines render no text -- nothing to measure.
                if (Rml::Element* lineElement = panel->GetChild(i))
                {
                    const float textWidth =
                        static_cast<float>(Rml::ElementUtilities::GetStringWidth(lineElement, line.text));
                    const float frameWidth = config.rich ? InlineFrameWidth(*lineElement) : 0.0f;
                    maxLineWidth = std::max(maxLineWidth, textWidth + frameWidth);
                }
            }
            // +1px slack -- GetStringWidth() is a font-metrics estimate, not a guarantee against
            // sub-pixel rounding wrapping the very last character early.
            const std::string width = std::to_string(static_cast<int>(maxLineWidth) + 1) + "px";
            if (width != s_PanelWidth)
            {
                panel->SetProperty("width", width);
                s_PanelWidth = width;
            }

            // Lay out again at that width, so the box below has the final wrapped height.
            s_View.Document()->UpdateDocument();

            // #tooltip_panel's own box, not the document's: the document body spans the viewport.
            const Rml::Box& box = panel->GetBox();
            measured.size = box.GetSize(Rml::BoxArea::Border);
            measured.frameTopLeft.x = box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Left);
            measured.frameTopLeft.y = box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Top);
            measured.frameBottom = box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Bottom);
            return measured;
        }

        // The height of all #tooltip_panel draws: its frame and shadow and, after its `lineCount` lines, the shown
        // gold frame's glow and the corner diamonds. As RmlUi sizes each box-shadow's texture: the border box and
        // the shadow's reach.
        float InkHeight(Rml::Element& panel, std::size_t lineCount)
        {
            Rml::Rectanglef ink;
            if (!Rml::ElementUtilities::GetBoundingBox(ink, &panel, Rml::BoxArea::Auto))
                return 0.0f;
            for (int i = static_cast<int>(lineCount); i < panel.GetNumChildren(); ++i)
            {
                Rml::Element* child = panel.GetChild(i);
                Rml::Rectanglef box;
                if (child != nullptr && child->IsVisible() &&
                    Rml::ElementUtilities::GetBoundingBox(box, child, Rml::BoxArea::Auto))
                    ink = ink.Join(box);
            }
            return ink.Height();
        }

        // Measures a rich tooltip at its own sizes and, when all it draws is taller than the window, lays it out
        // again at the scale that fits, in this same frame, so it never shows a frame too tall (RichFit).
        Measurement MeasureRich(Rml::Context& context, const Config& config, const RichMetrics& base)
        {
            TooltipRmlModel& model = s_View.Binder().GetModel();
            ApplyRichFit(model, base, false);
            Measurement measured = Measure(context, config);
            RichFit fit{context.GetDimensions(), measured.dpRatio};
            Rml::Element* panel = s_View.Document()->GetElementById("tooltip_panel");
            if (panel == nullptr)
            {
                s_RichFit = fit;
                return measured;
            }

            const float available = static_cast<float>(fit.viewport.y) - kRichFitMarginDp * measured.dpRatio;
            float ink = InkHeight(*panel, config.lines.size());
            // The lines and padding shrink with the text; the frame and the shadow's reach are counted as fixed.
            const float scaled = measured.size.y - measured.frameTopLeft.y - measured.frameBottom;
            fit.scale = ::UI::Tooltip::FitScale(scaled, ink - scaled, available, kRichMinFitScale);
            const RichMetrics fitted = Scaled(base, fit.scale);
            if (fit.scale < 1.0f)
            {
                ApplyRichFit(model, fitted, false);
                measured = Measure(context, config);
                ink = InkHeight(*panel, config.lines.size());
            }

            // Taller than the window even so: the shadow and glow go before the first frame is drawn (the layout stays
            // as it is). Checked against the window, not the margin: the dividers do not scale and the font sizes
            // are rounded, so a fitted tooltip may end a few pixels past `available` and still fit the window.
            fit.tall = ink > static_cast<float>(fit.viewport.y);
            if (fit.tall)
            {
                ApplyRichFit(model, fitted, true);
                context.Update();
            }
            s_RichFit = fit;
            return measured;
        }

        // The scale #tooltip_panel is drawn at: on the phone, small enough to fit the screen's height.
        // The PC's tooltips keep their size.
        float FitScale(const Measurement& measured, Rml::Vector2i viewport)
        {
            if (!UI::Mobile::IsPhoneView())
                return 1.0f;
            return TooltipPlacement::FitScale(measured.size.y, static_cast<float>(viewport.y), kPhoneMinFitScale);
        }

        // Draws #tooltip_panel at the scale, around its top-left corner (the body's place), as it is
        // placed; takes the transform off again at 1.
        void ApplyFitScale(float scale)
        {
            if (scale == s_FitScale)
                return;
            Rml::Element* panel = s_View.Document()->GetElementById("tooltip_panel");
            if (!panel)
                return;
            if (scale == 1.0f)
            {
                panel->RemoveProperty("transform");
                panel->RemoveProperty("transform-origin");
            }
            else
            {
                panel->SetProperty("transform", "scale(" + std::to_string(scale) + ")");
                panel->SetProperty("transform-origin", "left top");
            }
            s_FitScale = scale;
        }

        // Top-left of the panel for the anchor, kept fully inside the viewport. The panel is drawn at
        // `scale` around that corner.
        Rml::Vector2f Place(const Config& config, const Measurement& measured, Rml::Vector2i viewport, float scale)
        {
            // The anchor places the panel's inner (padding) box; a theme's frame lies outside it, as
            // RenderTipTextList() draws its 1-unit frame around the box it anchored.
            const Rml::Vector2f size = measured.size * scale;
            const Rml::Vector2f frameTopLeft = measured.frameTopLeft * scale;
            const float frameBottom = measured.frameBottom * scale;
            float left =
                config.centerHorizontally ? (config.anchorX - size.x * 0.5f) : (config.anchorX - frameTopLeft.x);
            const bool above = (config.anchor == AnchorPoint::AboveLeft);
            // The anchors as the panel's border box edges: above meets its bottom, below its top.
            const auto edge = [&](float anchorY, bool growsUp)
            { return growsUp ? anchorY + frameBottom : anchorY - frameTopLeft.y; };
            std::optional<float> flipEdge;
            if (config.flipAnchorY)
                flipEdge = edge(*config.flipAnchorY, !above);
            const float anchorEdge = edge(config.anchorY, above);
            left = TooltipPlacement::LeftOrBeside(left, anchorEdge, above, config.besideX ? &*config.besideX : nullptr,
                                                  size.y, static_cast<float>(viewport.y));
            const float top = TooltipPlacement::Top(anchorEdge, above, flipEdge ? &*flipEdge : nullptr, size.y,
                                                    static_cast<float>(viewport.y));

            // Clamped on all four sides to the real viewport (this document has no parent transform),
            // not REFERENCE_WIDTH/HEIGHT. Wider or taller than the viewport sticks to its left/top.
            left = std::max(0.0f, std::min(left, static_cast<float>(viewport.x) - size.x));
            return {left, top};
        }
    }

    void Show(const Config& config, Owner owner)
    {
        if (!RmlUiRuntime::Instance().IsCreated() || config.lines.empty())
            return;

        if (!s_View.Ensure())
            return;

        s_CurrentOwner = owner;
        s_RefreshEachFrame = config.refreshEachFrame;
        s_Refreshed = true;

        TooltipRmlModel next;
        next.lines.reserve(config.lines.size());
        for (const Line& line : config.lines)
            next.lines.push_back(ToLineEntry(line));
        ApplyNativeMetrics(next, config);
        ApplyRichLayout(next, config);

        auto& model = s_View.Binder().GetModel();
        Rml::ElementDocument* document = s_View.Document();
        Rml::Context& context = *document->GetContext();
        const float dpRatio = context.GetDensityIndependentPixelRatio();
        // A rich tooltip keeps the fit worked out for this window, so unchanged content stays unchanged.
        const RichMetrics richBase = MetricsOf(next);
        const bool richFitKnown =
            config.rich && s_RichFit && s_RichFit->viewport == context.GetDimensions() && s_RichFit->dpRatio == dpRatio;
        if (richFitKnown)
        {
            SetMetrics(next, Scaled(richBase, s_RichFit->scale));
            next.tall = s_RichFit->tall;
        }
        const bool contentChanged = UpdateContent(model, std::move(next));
        if (!document->IsVisible())
            document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        if (contentChanged || !s_Measured || s_Measured->dpRatio != dpRatio || (config.rich && !richFitKnown))
            s_Measured = config.rich ? MeasureRich(context, config, richBase) : Measure(context, config);

        const float scale = FitScale(*s_Measured, context.GetDimensions());
        ApplyFitScale(scale);
        const Rml::Vector2f position = Place(config, *s_Measured, context.GetDimensions(), scale);
        if (model.posX != position.x)
        {
            model.posX = position.x;
            s_View.Binder().MarkDirty("pos_x");
        }
        if (model.posY != position.y)
        {
            model.posY = position.y;
            s_View.Binder().MarkDirty("pos_y");
        }
    }

    void Hide(Owner owner)
    {
        if (!s_View.Document())
            return;
        // See Owner's own comment (RmlTooltip.h) -- only actually hide if this caller (or an
        // ownerless caller) is the one the tooltip is currently showing for, so a not-hovered
        // caller's own per-frame Hide() can't clobber a different caller's legitimate Show() from
        // earlier the same frame. Bug fixed here: the old condition also required
        // `s_CurrentOwner != nullptr` before protecting anything, so a NON-null-owner Hide() (e.g.
        // MainFrameWindow's own `Hide(g_pSkillList)`, called every frame no skill hotkey is
        // hovered) always fell through to hiding unconditionally whenever the current owner
        // happened to be nullptr (any ownerless caller, e.g. MyInventory's Set/Socket tooltip) --
        // it's the CALLER's own owner that must be nullptr (or match) to win, not the current
        // owner's.
        if (owner != nullptr && owner != s_CurrentOwner)
            return;
        s_View.Document()->Hide();
        s_CurrentOwner = nullptr;
    }

    void ExpireUnrefreshed()
    {
        Rml::ElementDocument* document = s_View.Document();
        if (document && document->IsVisible() && s_RefreshEachFrame && !s_Refreshed)
            Hide(s_CurrentOwner);
        s_Refreshed = false;
    }
}
