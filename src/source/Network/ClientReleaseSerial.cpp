#include "stdafx.h"
#include "Network/ClientReleaseSerial.h"

#include "Core/Platform/ExecutableFolder.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Network/ClientRelease.h"
#include "Network/Server/WSclient.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

extern BYTE Serial[SIZE_PROTOCOLSERIAL + 1];

namespace Network::ClientRelease
{
namespace
{
// The release script looks for these bytes in Main.exe to know that the build sends its release number. The log
// lines below print them, so the build keeps them.
constexpr char SendsReleaseMarker[] = "MUSKYNET_SENDS_RELEASE_V1";
constexpr wchar_t ReleaseFileName[] = L"release.txt";
// The file holds one short line; nothing past this is ever needed.
constexpr std::streamsize MaxReadBytes = 256;

// The number and its terminator replace the serial's first bytes; the serial's own terminator stays after them.
static_assert(ReleaseLength + 1 <= SIZE_PROTOCOLSERIAL);

// The start of the file, or nothing when it can't be opened.
std::optional<std::string> ReadFileStart(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;

    std::string text(static_cast<std::size_t>(MaxReadBytes), '\0');
    file.read(text.data(), MaxReadBytes);
    text.resize(static_cast<std::size_t>(file.gcount()));
    return text;
}
} // namespace

void LoadIntoSerial()
{
    const std::filesystem::path folder = Core::Platform::ExecutableFolder();
    const std::optional<std::string> text =
        folder.empty() ? std::nullopt : ReadFileStart(folder / ReleaseFileName);
    if (!text)
    {
        g_ErrorReport.Write(L"> Client release (%hs): no release.txt next to Main.exe, the old serial is sent.\r\n",
                            SendsReleaseMarker);
        return;
    }

    const std::optional<std::string> release = ParseReleaseText(*text);
    if (!release)
    {
        g_ErrorReport.Write(L"> Client release (%hs): release.txt has no valid number, the old serial is sent.\r\n",
                            SendsReleaseMarker);
        return;
    }

    std::memcpy(Serial, release->c_str(), release->size() + 1);
    g_ErrorReport.Write(L"> Client release (%hs): %hs, sent as the serial.\r\n", SendsReleaseMarker,
                        release->c_str());
}
} // namespace Network::ClientRelease
