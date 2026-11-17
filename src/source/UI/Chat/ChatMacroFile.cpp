#include "UI/Chat/ChatMacroFile.h"

namespace UI::Chat::Macros
{
MacroSlots ParseLegacyFile(std::wstring_view text)
{
    MacroSlots slots;
    int slot = 0;
    while (slot < kSlotCount && !text.empty())
    {
        const std::size_t lineEnd = text.find(L'\n');
        std::wstring_view line = text.substr(0, lineEnd);
        if (!line.empty() && line.back() == L'\r')
        {
            line.remove_suffix(1);
        }
        slots[slot++] = std::wstring(line.substr(0, kMaxLength));
        if (lineEnd == std::wstring_view::npos)
        {
            break;
        }
        text.remove_prefix(lineEnd + 1);
    }
    return slots;
}

int SlotKeyNumber(int slot)
{
    return (slot + 1) % kSlotCount;
}

namespace
{
constexpr char32_t kReplacement = 0xFFFD;

// The code points of a wide string: UTF-16 on Windows (surrogate pairs joined), UTF-32 elsewhere.
std::u32string CodePoints(std::wstring_view text)
{
    std::u32string out;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        char32_t c = static_cast<char32_t>(text[i]);
        if constexpr (sizeof(wchar_t) == 2)
        {
            c &= 0xFFFF;
            if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size())
            {
                const char32_t low = static_cast<char32_t>(text[i + 1]) & 0xFFFF;
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                    ++i;
                }
            }
        }
        out.push_back(c);
    }
    return out;
}

void AppendWide(std::wstring& out, char32_t c)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        if (c >= 0x10000)
        {
            c -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (c >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (c & 0x3FF)));
            return;
        }
    }
    out.push_back(static_cast<wchar_t>(c));
}

void AppendUtf8(std::string& out, char32_t c)
{
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        c = kReplacement;
    if (c < 0x80)
        out.push_back(static_cast<char>(c));
    else if (c < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else if (c < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
}

// UTF-8 bytes to a wide string; a broken sequence becomes U+FFFD.
std::wstring WideFromUtf8(const std::string& bytes)
{
    std::wstring out;
    std::size_t i = 0;
    while (i < bytes.size())
    {
        const unsigned char lead = static_cast<unsigned char>(bytes[i]);
        int extra = 0;
        char32_t c = 0;
        if (lead < 0x80) { c = lead; }
        else if ((lead & 0xE0) == 0xC0) { c = lead & 0x1F; extra = 1; }
        else if ((lead & 0xF0) == 0xE0) { c = lead & 0x0F; extra = 2; }
        else if ((lead & 0xF8) == 0xF0) { c = lead & 0x07; extra = 3; }
        else { AppendWide(out, kReplacement); ++i; continue; }
        if (i + static_cast<std::size_t>(extra) >= bytes.size())
        {
            // The sequence is cut off at the end.
            AppendWide(out, kReplacement);
            break;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k)
        {
            const unsigned char next = static_cast<unsigned char>(bytes[i + k]);
            if ((next & 0xC0) != 0x80) { ok = false; break; }
            c = (c << 6) | (next & 0x3F);
        }
        if (!ok)
        {
            AppendWide(out, kReplacement);
            ++i;
            continue;
        }
        AppendWide(out, c);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return out;
}

int HexValue(wchar_t c)
{
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return -1;
}
} // namespace

std::wstring EncodeForIni(std::wstring_view text)
{
    static constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    std::string bytes;
    for (const char32_t c : CodePoints(text))
        AppendUtf8(bytes, c);

    std::wstring out;
    for (const char ch : bytes)
    {
        const unsigned char b = static_cast<unsigned char>(ch);
        if (b >= 0x21 && b <= 0x7E && b != '%' && b != '"')
        {
            out.push_back(static_cast<wchar_t>(b));
        }
        else
        {
            out.push_back(L'%');
            out.push_back(kHex[b >> 4]);
            out.push_back(kHex[b & 0x0F]);
        }
    }
    return out;
}

std::wstring DecodeFromIni(std::wstring_view value)
{
    if (value.find(L'%') == std::wstring_view::npos)
        return std::wstring(value);

    std::string bytes;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == L'%' && i + 2 < value.size() && HexValue(value[i + 1]) >= 0 && HexValue(value[i + 2]) >= 0)
        {
            bytes.push_back(static_cast<char>(HexValue(value[i + 1]) * 16 + HexValue(value[i + 2])));
            i += 2;
        }
        else
        {
            std::wstring_view rest = value.substr(i, 1);
            if constexpr (sizeof(wchar_t) == 2)
            {
                const char32_t c = static_cast<char32_t>(value[i]) & 0xFFFF;
                if (c >= 0xD800 && c <= 0xDBFF && i + 1 < value.size())
                {
                    rest = value.substr(i, 2);
                    ++i;
                }
            }
            for (const char32_t c : CodePoints(rest))
                AppendUtf8(bytes, c);
        }
    }
    return WideFromUtf8(bytes);
}
} // namespace UI::Chat::Macros
