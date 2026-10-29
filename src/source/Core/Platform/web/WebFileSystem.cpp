// Web build only: see WebFileSystem.h.
//
// Layout:
//   /server  WasmFS fetch mount: the files in files.txt that aren't in the startup pack, each
//            downloaded on first read. Read-only.
//   /game    in memory, the game's directory: the startup pack's files, links to the files in
//            /server, and copies of the single files (config.ini ...), which the game rewrites.
//            Anything the game creates (logs, screenshots) lands here too and is gone when the
//            page closes.

#include "Core/Platform/web/WebFileSystem.h"

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <set>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace
{
constexpr const char* GameDir = "/game";
constexpr const char* ServerDir = "/server";
constexpr unsigned ChunkSize = 1u << 20;
// The page's gzip copies of startup.pack and Main.wasm (tools/web/make_startup_pack.py). files.txt
// lists them so scripts/publish-web.py uploads them, but only the page reads them. Copied like
// config.ini, they were downloaded again at every start (120 MB in 1 MB pieces), which left a phone
// at a black screen (a Pixel 9 Pro XL, 2026-10-29).
constexpr std::string_view PageCopySuffix = ".gz";
// The map packs (packs/worldN.json and .pack.gz): the page reads them for LoadWorldFiles.
constexpr std::string_view PagePacksFolder = "packs/";

// 0 while the mount is being set up, 1 when it is ready, -1 when it failed.
std::atomic<int> g_mountState{0};

// The folder the page came from, as an absolute URL without the last slash. Emscripten's fetch
// backend resolves a relative base against the site's root, so with "." a page at
// https://skynet-mu.net/play/ fetched its files from /Data/... instead of /play/Data/...
std::string g_pageFolderUrl;

EM_JS(char*, MuWeb_PageFolderUrl, (), {
    const folder = new URL(".", location.href).href;
    return stringToNewUTF8(folder.endsWith("/") ? folder.slice(0, -1) : folder);
});

// The page starts downloading the startup pack as it loads (index.html: Module.muStartupPack, a
// promise of {files: [[path, size], ...], bytes}); this waits for it and writes its files into /game.
// Returns how many it wrote, 0 without a pack.
EM_ASYNC_JS(int, MuWeb_InstallStartupPack, (), {
    let pack = null;
    try {
        pack = await Module["muStartupPack"];
    } catch (e) {
        err("Startup pack: " + e + "; the game downloads its files one by one instead");
    }
    Module["muStartupPack"] = null;
    if (!pack) {
        return 0;
    }
    const folders = new Set();
    let offset = 0;
    let written = 0;
    for (const [path, size] of pack.files) {
        const full = "/game/" + path;
        const folder = full.substring(0, full.lastIndexOf("/"));
        if (!folders.has(folder)) {
            FS.mkdirTree(folder);
            folders.add(folder);
        }
        if (FS.writeFile(full, pack.bytes.subarray(offset, offset + size)) >= 0) {
            ++written;
        }
        offset += size;
    }
    return written;
});

// The page downloads a map's pack (index.html: Module.muMapPack(world), a promise of the same shape as
// the startup pack's, or of null where the map has none); this writes its files into /game in place of
// their links to /server, and turns the previous map's files back into links, so only one map's pack
// takes memory. Returns how many files it wrote, 0 without a pack, -1 when they are in place already.
EM_ASYNC_JS(int, MuWeb_InstallMapPack, (int world), {
    if (Module["muMapPackWorld"] === world) {
        return -1;
    }
    let pack = null;
    try {
        pack = Module["muMapPack"] ? await Module["muMapPack"](world) : null;
    } catch (e) {
        err("Map pack " + world + ": " + e + "; the game downloads the map's files one by one instead");
    }
    for (const path of Module["muMapPackFiles"] || []) {
        FS.unlink("/game/" + path);
        FS.symlink("/server/" + path, "/game/" + path);
    }
    const written = [];
    let offset = 0;
    for (const [path, size] of pack ? pack.files : []) {
        const full = "/game/" + path;
        FS.unlink(full);
        if (FS.writeFile(full, pack.bytes.subarray(offset, offset + size)) >= 0) {
            written.push(path);
        } else {
            FS.symlink("/server/" + path, full);
        }
        offset += size;
    }
    Module["muMapPackFiles"] = written;
    Module["muMapPackWorld"] = pack ? world : null;
    return written.length;
});

// Creates the folders above `path` (a path below /game), remembering the ones made.
void MakeParentFolders(const std::string& path, std::set<std::string>& made)
{
    for (std::size_t slash = path.find('/', std::string(GameDir).size() + 1); slash != std::string::npos;
         slash = path.find('/', slash + 1))
    {
        std::string folder = path.substr(0, slash);
        if (made.insert(folder).second)
        {
            ::mkdir(folder.c_str(), 0777);
        }
    }
}

// A file created inside the fetch mount downloads from "<base URL>/<its path under the mount>";
// the base is the page's folder (g_pageFolderUrl), so the files sit beside Main.js.
bool CreateOnServer(const std::string& relativePath)
{
    const std::string full = std::string(ServerDir) + "/" + relativePath;
    for (std::size_t slash = full.find('/', std::string(ServerDir).size() + 1); slash != std::string::npos;
         slash = full.find('/', slash + 1))
    {
        ::mkdir(full.substr(0, slash).c_str(), 0777);
    }

    const int fd = ::open(full.c_str(), O_CREAT | O_RDONLY, 0444);
    if (fd < 0)
    {
        return false;
    }
    ::close(fd);
    return true;
}

bool ReadWholeFile(const std::string& path, std::string& content)
{
    content.clear();
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return false;
    }
    char buffer[64 * 1024];
    ssize_t n;
    while ((n = ::read(fd, buffer, sizeof(buffer))) > 0)
    {
        content.append(buffer, static_cast<std::size_t>(n));
    }
    ::close(fd);
    return n == 0;
}

bool WriteWholeFile(const std::string& path, const std::string& content)
{
    const int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0)
    {
        return false;
    }
    const bool ok = ::write(fd, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    ::close(fd);
    return ok;
}

// Runs on a worker thread: WasmFS can't create a fetch backend on the browser's main thread.
void SetUpMount()
{
    backend_t fetch = wasmfs_create_fetch_backend(g_pageFolderUrl.c_str(), ChunkSize);
    std::string list;
    if (!fetch || wasmfs_create_directory(ServerDir, 0777, fetch) != 0 || !CreateOnServer("files.txt")
        || !ReadWholeFile(std::string(ServerDir) + "/files.txt", list))
    {
        g_mountState = -1;
        return;
    }

    std::size_t count = 0;
    std::size_t linked = 0;
    std::set<std::string> madeFolders;
    for (std::size_t start = 0; start < list.size();)
    {
        std::size_t end = list.find('\n', start);
        if (end == std::string::npos)
        {
            end = list.size();
        }
        std::string line = list.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty() || line == "files.txt" || line.ends_with(PageCopySuffix) || line.starts_with(PagePacksFolder))
        {
            continue;
        }
        const std::string inGame = std::string(GameDir) + "/" + line;
        if (::access(inGame.c_str(), F_OK) == 0)
        {
            ++count; // from the startup pack
            continue;
        }
        if (!CreateOnServer(line))
        {
            continue;
        }
        ++count;

        if (line.find('/') == std::string::npos)
        {
            // A single file beside Main.js (config.ini ...): the game may rewrite it, so it gets a
            // writable copy.
            std::string content;
            if (ReadWholeFile(std::string(ServerDir) + "/" + line, content))
            {
                WriteWholeFile(inGame, content);
            }
        }
        else
        {
            // Links per file, not per folder: the folders also hold the startup pack's files.
            MakeParentFolders(inGame, madeFolders);
            if (::symlink((std::string(ServerDir) + "/" + line).c_str(), inGame.c_str()) == 0)
            {
                ++linked;
            }
        }
    }

    std::printf("Web file system: %zu game files, %zu of them downloaded when first read\n", count, linked);
    g_mountState = count > 0 ? 1 : -1;
}

// The player's config.ini from earlier visits (index.html keeps it in the browser's storage), with
// the server copy's [CONNECTION SETTINGS], so a moved server still reaches everyone. Returns a new
// string to free, or null without a saved copy.
EM_JS(char*, MuWeb_MergeSavedConfig, (const char* serverText), {
    let saved = null;
    try {
        saved = localStorage.getItem("mu-web/config.ini");
    } catch (e) {
    }
    if (!saved) {
        return 0;
    }
    const findSection = (text, name) => {
        const start = text.indexOf("[" + name + "]");
        if (start < 0) {
            return null;
        }
        const next = text.indexOf("\n[", start + 1);
        return [start, next < 0 ? text.length : next + 1];
    };
    const server = UTF8ToString(serverText);
    const fromServer = findSection(server, "CONNECTION SETTINGS");
    if (!fromServer) {
        return stringToNewUTF8(saved);
    }
    let section = server.substring(fromServer[0], fromServer[1]);
    if (!section.endsWith("\n")) {
        section += "\n";
    }
    const inSaved = findSection(saved, "CONNECTION SETTINGS");
    const merged = inSaved ? saved.substring(0, inSaved[0]) + section + saved.substring(inSaved[1])
                           : saved + (saved.endsWith("\n") ? "" : "\n") + section;
    return stringToNewUTF8(merged);
});
} // namespace

bool mu::web::MountGameFiles()
{
    if (::mkdir(GameDir, 0777) != 0)
    {
        return false;
    }
    const int packed = MuWeb_InstallStartupPack();
    if (packed > 0)
    {
        std::printf("Web file system: %d files from the startup pack\n", packed);
    }

    // Asked here, on the page's thread: a worker's location is its script's.
    char* pageFolder = MuWeb_PageFolderUrl();
    g_pageFolderUrl = pageFolder;
    std::free(pageFolder);

    std::thread worker(SetUpMount);
    while (g_mountState.load() == 0)
    {
        emscripten_sleep(10);
    }
    worker.join();
    if (g_mountState.load() != 1)
    {
        std::fprintf(stderr, "Web file system: can't read files.txt from the web server\n");
        return false;
    }

    const std::string configPath = std::string(GameDir) + "/config.ini";
    std::string config;
    if (ReadWholeFile(configPath, config))
    {
        if (char* merged = MuWeb_MergeSavedConfig(config.c_str()))
        {
            WriteWholeFile(configPath, merged);
            std::free(merged);
            std::printf("Web file system: config.ini from this browser's earlier visits\n");
        }
    }
    return ::chdir(GameDir) == 0;
}

void mu::web::LoadWorldFiles(int world)
{
    const int written = MuWeb_InstallMapPack(world);
    if (written >= 0)
    {
        std::printf("Web file system: map folder %d, %d files from its pack\n", world, written);
    }
}
