#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Core/Platform/WinCompat.h"

namespace StringUtils
{
    // Convert wide string (UTF-16) to narrow string (UTF-8)
    inline std::string WideToNarrow(const wchar_t* wstr)
    {
        if (!wstr) return "";
        return mu_wchar_to_utf8(wstr);
    }

    // Convert narrow string (UTF-8) to wide string (UTF-16 on Windows, UTF-32 elsewhere) --
    // symmetric counterpart to WideToNarrow above. Added for the RmlUi port
    // (RmlUiRenderInterface::LoadTexture needs it for CGlobalBitmap::LoadImage's
    // std::wstring-based API).
    inline std::wstring NarrowToWide(const std::string& str)
    {
        return mu_utf8_to_wchar(str);
    }

    // "0A1F..." -> its bytes; empty when it isn't hex (an odd length or a character that isn't a hex digit).
    inline std::vector<std::uint8_t> HexToBytes(std::wstring_view text)
    {
        auto digit = [](wchar_t c) -> int
        {
            if (c >= L'0' && c <= L'9')
                return c - L'0';
            if (c >= L'A' && c <= L'F')
                return c - L'A' + 10;
            if (c >= L'a' && c <= L'f')
                return c - L'a' + 10;
            return -1;
        };

        std::vector<std::uint8_t> bytes;
        if (text.size() % 2 != 0)
            return bytes;
        bytes.reserve(text.size() / 2);
        for (size_t i = 0; i + 1 < text.size(); i += 2)
        {
            const int high = digit(text[i]);
            const int low = digit(text[i + 1]);
            if (high < 0 || low < 0)
                return {};
            bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
        }
        return bytes;
    }
}
