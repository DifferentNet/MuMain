#include "UI/Chat/ItemLinkAttach.h"

namespace UI::Chat::ItemLinkAttach
{
Renamed Rename(std::wstring_view text, const std::vector<std::optional<Span>>& spans,
               const std::vector<std::wstring>& names)
{
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < spans.size(); ++i)
    {
        if (spans[i])
            order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&spans](std::size_t a, std::size_t b) { return spans[a]->start < spans[b]->start; });

    Renamed renamed;
    renamed.spans.resize(spans.size());
    std::size_t at = 0;
    for (const std::size_t i : order)
    {
        const Span& span = *spans[i];
        if (span.start < at || span.start + span.length > text.size())
            continue;
        renamed.text.append(text.substr(at, span.start - at));
        const std::wstring_view name = i < names.size() && !names[i].empty()
                                           ? std::wstring_view(names[i])
                                           : text.substr(span.start, span.length);
        renamed.spans[i] = Span{renamed.text.size(), name.size()};
        renamed.text.append(name);
        at = span.start + span.length;
    }
    renamed.text.append(text.substr(std::min(at, text.size())));
    return renamed;
}

std::size_t KeepLinksWhole(std::size_t split, const std::vector<Span>& spans)
{
    for (const Span& span : spans)
    {
        if (split <= span.start || split >= span.start + span.length)
            continue;
        return span.start > 0 ? span.start : span.start + span.length;
    }
    return split;
}

std::vector<Segment> Segments(std::wstring_view lead, std::wstring_view text, const std::vector<Span>& spans)
{
    std::vector<Segment> segments;
    std::wstring plain(lead);
    std::size_t at = 0;
    for (std::size_t i = 0; i < spans.size(); ++i)
    {
        const Span& span = spans[i];
        if (span.start < at || span.length == 0 || span.start + span.length > text.size())
            continue;
        plain.append(text.substr(at, span.start - at));
        if (!plain.empty())
            segments.push_back({std::move(plain), -1});
        plain.clear();
        segments.push_back({std::wstring(text.substr(span.start, span.length)), static_cast<int>(i)});
        at = span.start + span.length;
    }
    plain.append(text.substr(at));
    if (!plain.empty())
        segments.push_back({std::move(plain), -1});
    return segments;
}
} // namespace UI::Chat::ItemLinkAttach
