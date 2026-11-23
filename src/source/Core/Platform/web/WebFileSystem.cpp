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
#include "Core/Platform/BundledFonts.h"
#include "Core/Platform/FirstVisitLocale.h"

#ifdef MU_WEB_FILE_TRACE
#include "stdafx.h"
#include "Engine/Object/ZzzCharacter.h"
#endif

#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/wasmfs.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
constexpr const char* GameDir = "/game";
constexpr const char* ServerDir = "/server";
constexpr unsigned ChunkSize = 1u << 20;
// What a link in /game to a file of the mount points at starts with (SetUpMount).
constexpr std::string_view ServerLinkPrefix = "/server/";
// The biggest file OpenWholeInMemory reads: the most the mount downloads on a file's first read
// (fetch-backend.js, fetchStart), every file but music and long ambient sounds.
constexpr off_t WholeReadMostBytes = 2 * ChunkSize;
// files.txt also lists the page's own files, so scripts/publish-web.py uploads them, but only the page
// reads them. Copied like config.ini, they were downloaded again at every start: the gzip copy of the
// startup pack took 120 MB in 1 MB pieces and left a phone at a black screen (a Pixel 9 Pro XL,
// 2026-10-29). They are the gzip copies (Main.wasm.gz), the packs (packs/, tools/web/make_startup_pack.py:
// the startup pack's parts and the map packs, for LoadWorldFiles) and these.
constexpr std::string_view PageCopySuffix = ".gz";
constexpr std::string_view PagePacksFolder = "packs/";
constexpr std::array<std::string_view, 4> PageFiles = {"manifest.webmanifest", "icon-192.png", "icon-512.png", "sw.js"};

bool IsPageFile(std::string_view path)
{
    return path.ends_with(PageCopySuffix) || path.starts_with(PagePacksFolder)
           || std::find(PageFiles.begin(), PageFiles.end(), path) != PageFiles.end();
}

// 0 while the mount is being set up, 1 when it is ready, -1 when it failed.
std::atomic<int> g_mountState{0};

// 1 where the page's thread downloads the mount's files, 0 where the mount's worker does
// (fetch-backend.js asks MuWeb_FilesOnPageThread). The page decides: Module.muFilesOnPageThread.
std::atomic<int> g_filesOnPageThread{0};

EM_JS(int, MuWeb_PageWantsFilesOnPageThread, (), { return Module["muFilesOnPageThread"] ? 1 : 0; });

// false where the page's address has ?nowholeread: fopen then reads the mount's files piece by piece, as
// before OpenWholeInMemory (to compare, or where it misbehaves).
std::atomic<bool> g_wholeReads{true};

EM_JS(int, MuWeb_PageWantsWholeReads, (), { return new URLSearchParams(location.search).has("nowholeread") ? 0 : 1; });

// Where the page's thread downloads the mount's files (g_filesOnPageThread: Safari, every iPhone
// browser), each one the game read was a synchronous request on the page's thread, the game's own:
// the game, its picture and its connection to the server stood still until the file came, at least
// a trip to the server each (~50 ms), and a monster seen for the first time reads some twenty. The
// service worker never sees a synchronous request (sw.js), so none was kept on the device and every
// visit downloaded them again: on a Mac in Safari, a long wait to enter the world, freezes on every
// walk and the character pulled back a few metres after them (2026-10-30). Where this is true, the
// page's thread asks for a file of the mount the game opens whole (OpenWholeInMemory) like any other
// download (MuWeb_ReadWholeAsync), through the service worker, and the game waits for it the way it
// waits for the next frame (JSPI): meanwhile the page runs, and the file is kept for the next time.
// index.html?syncreads reads as before.
std::atomic<bool> g_pageReads{false};

// Sets the reads up where the page wants them (no ?syncreads) and says whether it does. The game may
// wait for a file only where it waits for nothing else: what runs while it waits (the browser's
// events, SDL's handlers of them among them) can't wait itself. Every wait of the game goes through
// Asyncify.handleAsync (EM_ASYNC_JS, emscripten_sleep), so its waits are counted (muWaits).
EM_JS(int, MuWeb_SetUpPageReads, (), {
    if (new URLSearchParams(location.search).has("syncreads")) {
        return 0;
    }
    if (!Module["muWaits"]) {
        Module["muWaits"] = {count: 0};
        const handleAsync = Asyncify.handleAsync;
        Asyncify.handleAsync = async (startAsync) => {
            Module["muWaits"].count++;
            try {
                return await handleAsync(startAsync);
            } finally {
                Module["muWaits"].count--;
            }
        };
    }
    // The files' hashes (startup.json "versions", as fetch-backend.js reads them), asked for now.
    Module["muFileVersions"] = fetch(new URL("startup.json", location.href), {cache: "no-cache"})
        .then((response) => response.ok ? response.json() : {})
        .then((index) => index.versions || {}, () => ({}));
    return 1;
});

EM_JS(int, MuWeb_GameWaitsForNothing, (), { return Module["muWaits"] && Module["muWaits"].count == 0 ? 1 : 0; });

// The whole file at `path` (below the page's folder) in a new allocation of `headerBytes` and then the
// file's bytes, its size in *size; or null where it has no hash in startup.json (so the service worker
// wouldn't keep it), is bigger than `mostBytes` or doesn't come, for the mount to read as before. A
// file the service worker keeps is read from its store directly (as fetch-backend.js readKept does).
EM_ASYNC_JS(void*, MuWeb_ReadWholeAsync, (const char* path, int headerBytes, int mostBytes, int* size), {
    const file = UTF8ToString(path);
    const hash = (await Module["muFileVersions"])[file];
    if (!hash) {
        return 0;
    }
    const url = new URL(`${file}?v=${hash}`, location.href).href;
    let blob = null;
    try {
        const kept = self.caches && await (await caches.open("mu-web/files")).match(url);
        blob = kept ? await kept.blob() : null;
    } catch (e) {
    }
    try {
        if (!blob) {
            const response = await fetch(url, {credentials: "same-origin"});
            if (response.status != 200 || Number(response.headers.get("Content-Length") || 0) > mostBytes) {
                response.body && response.body.cancel().catch(() => {});
                return 0;
            }
            blob = await response.blob();
        }
        if (blob.size > mostBytes) {
            return 0;
        }
        const bytes = new Uint8Array(await blob.arrayBuffer());
        // Unsigned: the game's memory may pass 2 GB.
        const memory = _malloc(headerBytes + bytes.length) >>> 0;
        if (!memory) {
            return 0;
        }
        HEAPU8.set(bytes, memory + headerBytes);
        HEAP32[(size >>> 0) >>> 2] = bytes.length;
        return memory;
    } catch (e) {
        return 0;
    }
});

// The biggest file MuWeb_ReadWholeAsync reads: music too, which the mount otherwise reads 1 MB at a
// time while it plays, each piece another synchronous request.
constexpr int PageReadMostBytes = 32 << 20;

// The folder the page came from, as an absolute URL without the last slash. Emscripten's fetch
// backend resolves a relative base against the site's root, so with "." a page at
// https://skynet-mu.net/play/ fetched its files from /Data/... instead of /play/Data/...
std::string g_pageFolderUrl;

EM_JS(char*, MuWeb_PageFolderUrl, (), {
    const folder = new URL(".", location.href).href;
    return stringToNewUTF8(folder.endsWith("/") ? folder.slice(0, -1) : folder);
});

// The page starts downloading the startup pack as it loads (index.html: Module.muStartupPack, a
// promise of its parts, each a promise of {files: [[path, size], ...], bytes}); this writes each
// part's files into /game as it arrives and lets go of it, so the page never holds the whole pack.
// A part that fails leaves its files to download one by one. Returns how many files it wrote, 0
// without a pack; the time spent writing (not waiting for the parts) is left for
// MuWeb_StartupPackWriteMs.
EM_ASYNC_JS(int, MuWeb_InstallStartupPack, (), {
    let parts = null;
    try {
        parts = await Module["muStartupPack"];
    } catch (e) {
        err("Startup pack: " + e + "; the game downloads its files one by one instead");
    }
    Module["muStartupPack"] = null;
    const folders = new Set();
    let written = 0;
    let writeMs = 0;
    for (let i = 0; parts && i < parts.length; i++) {
        let pack = null;
        try {
            pack = await parts[i];
        } catch (e) {
            err("Startup pack part " + (i + 1) + ": " + e + "; the game downloads its files one by one instead");
        }
        parts[i] = null;
        const writeStart = performance.now();
        let offset = 0;
        for (const [path, size] of pack ? pack.files : []) {
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
        writeMs += performance.now() - writeStart;
    }
    Module["muStartupPackWriteMs"] = writeMs;
    return written;
});

EM_JS(double, MuWeb_StartupPackWriteMs, (), {
    return Module["muStartupPackWriteMs"] || 0;
});

// The page downloads a map's pack (index.html: Module.muMapPack(world), a promise of the same shape as
// the startup pack's, or of null where the map has none); this writes its files into /game in place of
// their links to /server, and turns the previous map's files back into links, so only one map's pack
// takes memory. A world without a pack (the login and character scenes, which the startup pack holds)
// leaves the previous map's files in place: a relog back into that map then needs nothing, where it
// read the whole pack again (Lorencia's 438 files); so does a pack that failed to load. Returns how many
// files it wrote, 0 without a pack, -1 when they are in place already, -2 when the pack failed to load.
EM_ASYNC_JS(int, MuWeb_InstallMapPack, (int world), {
    if (Module["muMapPackWorld"] === world) {
        return -1;
    }
    let pack = null;
    let failed = false;
    try {
        pack = Module["muMapPack"] ? await Module["muMapPack"](world) : null;
    } catch (e) {
        failed = true;
        err("Map pack " + world + ": " + e + "; the game downloads the map's files one by one instead");
    }
    if (!pack) {
        return failed ? -2 : 0;
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
    Module["muMapPackWorld"] = world;
    return written.length;
});

// index.html Module.muMapPackPrefetch(world): starts reading the map's pack before the server answers;
// MuWeb_InstallMapPack then takes that read. Nothing is written into /game here. Not for the map whose
// pack is in place already.
EM_JS(void, MuWeb_PrefetchMapPack, (int world), {
    if (Module["muMapPackWorld"] !== world && Module["muMapPackPrefetch"]) {
        Module["muMapPackPrefetch"](world);
    }
});

// The map folder each character was last in, by name, in the browser's storage ("mu-web/character-maps",
// a JSON object, the most recent last, at most 32 names), for the character select screen. 0 where none.
EM_JS(void, MuWeb_RememberCharacterWorld, (const char* name, int world), {
    try {
        const key = "mu-web/character-maps";
        const who = "c:" + UTF8ToString(name);   // never an integer-like key, which objects order first
        let maps = {};
        try { maps = JSON.parse(localStorage.getItem(key) || "{}") || {}; } catch (e) { maps = {}; }
        if (maps[who] === world) return;
        delete maps[who];
        maps[who] = world;
        const names = Object.keys(maps);
        for (let i = 0; i < names.length - 32; i++) delete maps[names[i]];
        localStorage.setItem(key, JSON.stringify(maps));
    } catch (e) { /* no storage */ }
});
EM_JS(int, MuWeb_RememberedCharacterWorld, (const char* name), {
    try {
        const maps = JSON.parse(localStorage.getItem("mu-web/character-maps") || "{}") || {};
        const world = maps["c:" + UTF8ToString(name)];
        return Number.isInteger(world) && world > 0 ? world : 0;
    } catch (e) {
        return 0;
    }
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
// the base is the page's folder (g_pageFolderUrl), so the files sit beside Main.js. `madeFolders`
// remembers the folders made: each mkdir is a call to the mount's thread, and the ~11,000 files of
// files.txt asked for their folders again every time.
bool CreateOnServer(const std::string& relativePath, std::set<std::string>& madeFolders)
{
    const std::string full = std::string(ServerDir) + "/" + relativePath;
    for (std::size_t slash = full.find('/', std::string(ServerDir).size() + 1); slash != std::string::npos;
         slash = full.find('/', slash + 1))
    {
        std::string folder = full.substr(0, slash);
        if (!madeFolders.contains(folder))
        {
            ::mkdir(folder.c_str(), 0777);
            madeFolders.insert(std::move(folder));
        }
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

// A file of the mount read into memory (OpenWholeInMemory): this header, then the file's bytes, in
// one allocation that the FILE frees when it closes.
struct MemoryFile
{
    std::size_t size;
    std::size_t at;
};

ssize_t ReadMemoryFile(void* cookie, char* out, std::size_t length)
{
    auto* file = static_cast<MemoryFile*>(cookie);
    if (file->at >= file->size)
    {
        return 0;
    }
    const std::size_t count = std::min(length, file->size - file->at);
    std::memcpy(out, reinterpret_cast<const char*>(file + 1) + file->at, count);
    file->at += count;
    return static_cast<ssize_t>(count);
}

int SeekMemoryFile(void* cookie, off_t* offset, int whence)
{
    auto* file = static_cast<MemoryFile*>(cookie);
    off_t base = 0;
    if (whence == SEEK_CUR)
    {
        base = static_cast<off_t>(file->at);
    }
    else if (whence == SEEK_END)
    {
        base = static_cast<off_t>(file->size);
    }
    else if (whence != SEEK_SET)
    {
        errno = EINVAL;
        return -1;
    }
    if (base + *offset < 0)
    {
        errno = EINVAL;
        return -1;
    }
    *offset += base;
    file->at = static_cast<std::size_t>(*offset);
    return 0;
}

int CloseMemoryFile(void* cookie)
{
    std::free(cookie);
    return 0;
}

// The file's bytes in memory, read with one read call, or null where `fd` isn't a file of up to
// WholeReadMostBytes or the read fails.
MemoryFile* ReadIntoMemory(int fd)
{
    struct stat info;
    if (::fstat(fd, &info) != 0 || info.st_size <= 0 || info.st_size > WholeReadMostBytes)
    {
        return nullptr;
    }
    const auto size = static_cast<std::size_t>(info.st_size);
    auto* file = static_cast<MemoryFile*>(std::malloc(sizeof(MemoryFile) + size));
    if (!file)
    {
        return nullptr;
    }
    if (::read(fd, file + 1, size) != static_cast<ssize_t>(size))
    {
        std::free(file);
        return nullptr;
    }
    file->size = size;
    file->at = 0;
    return file;
}

// The page's thread downloads the file (g_pageReads) while the game waits, or null where it can't.
MemoryFile* ReadIntoMemoryOnPage(const char* fileInMount)
{
    if (!g_pageReads.load(std::memory_order_relaxed) || !emscripten_is_main_browser_thread() || !MuWeb_GameWaitsForNothing())
    {
        return nullptr;
    }
    int size = 0;
    auto* file = static_cast<MemoryFile*>(MuWeb_ReadWholeAsync(fileInMount, sizeof(MemoryFile), PageReadMostBytes, &size));
    if (file)
    {
        file->size = static_cast<std::size_t>(size);
        file->at = 0;
    }
    return file;
}

// The file of the mount that `path`, a link in /game, points to: its path in the mount, in `target`; null
// where `path` isn't such a link.
const char* FileInMount(const char* path, char (&target)[PATH_MAX])
{
    const ssize_t targetLength = ::readlink(path, target, sizeof(target) - 1);
    if (targetLength <= static_cast<ssize_t>(ServerLinkPrefix.size())
        || std::string_view(target, ServerLinkPrefix.size()) != ServerLinkPrefix)
    {
        return nullptr;
    }
    target[targetLength] = '\0';
    return target + ServerLinkPrefix.size();
}

// The file read ahead (mu::web::ReadWholeAhead) for the next fopen of it, on any thread: its path in the
// mount and its bytes. One at a time.
std::mutex g_readAheadLock;
std::string g_readAheadFile;
MemoryFile* g_readAhead = nullptr;

MemoryFile* TakeReadAhead(const char* fileInMount)
{
    std::lock_guard lock(g_readAheadLock);
    if (!g_readAhead || g_readAheadFile != fileInMount)
    {
        return nullptr;
    }
    g_readAheadFile.clear();
    return std::exchange(g_readAhead, nullptr);
}

// For __wrap_fopen: a FILE reading the whole file from memory where `path` is a link in /game to a file
// of the mount, opened to read, of up to WholeReadMostBytes (PageReadMostBytes where the page reads it,
// or read ahead); otherwise null, and fopen opens it as usual.
FILE* OpenWholeInMemory(const char* path, const char* mode)
{
    if (!g_wholeReads.load(std::memory_order_relaxed) || !path || !mode || mode[0] != 'r' || std::strchr(mode, '+'))
    {
        return nullptr;
    }
    char target[PATH_MAX];
    const char* fileInMount = FileInMount(path, target);
    if (!fileInMount)
    {
        return nullptr;
    }
    MemoryFile* memory = TakeReadAhead(fileInMount);
    if (!memory)
    {
        memory = ReadIntoMemoryOnPage(fileInMount);
    }
    if (!memory)
    {
        const int fd = ::open(path, O_RDONLY);
        if (fd < 0)
        {
            return nullptr;
        }
        memory = ReadIntoMemory(fd);
        ::close(fd);
    }
    if (!memory)
    {
        return nullptr;
    }
    FILE* file = ::fopencookie(memory, "rb",
                               {.read = ReadMemoryFile, .write = nullptr, .seek = SeekMemoryFile, .close = CloseMemoryFile});
    if (!file)
    {
        std::free(memory);
    }
    return file;
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

// Milliseconds since `start`.
double MsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

// The frames a pack's files were written in, for its log line.
struct PackInstallTiming
{
    double totalMs = 0;
    double longestMs = 0;
    int frames = 0;

    void Add(double ms)
    {
        totalMs += ms;
        longestMs = std::max(longestMs, ms);
        ++frames;
    }
};

// The text of files.txt the page downloaded beside the packs (index.html Module.muFileList), handed over
// once; null where it didn't come (the mount reads it then).
EM_ASYNC_JS(char*, MuWeb_TakeFileList, (), {
    const loading = Module["muFileList"];
    Module["muFileList"] = null;
    let text = null;
    try {
        text = loading ? await loading : null;
    } catch (e) {
        text = null;
    }
    return text ? stringToNewUTF8(text) : 0;
});

// Runs on a worker thread: WasmFS can't create a fetch backend on the browser's main thread. `pageList`:
// files.txt as the page downloaded it, or empty, where the mount downloads it.
void SetUpMount(const std::string& pageList)
{
    const auto started = std::chrono::steady_clock::now();
    backend_t fetch = wasmfs_create_fetch_backend(g_pageFolderUrl.c_str(), ChunkSize);
    std::string list = pageList;
    std::set<std::string> madeServerFolders;
    if (!fetch || wasmfs_create_directory(ServerDir, 0777, fetch) != 0
        || (list.empty()
            && (!CreateOnServer("files.txt", madeServerFolders)
                || !ReadWholeFile(std::string(ServerDir) + "/files.txt", list))))
    {
        g_mountState = -1;
        return;
    }
    const double listMs = MsSince(started);
    const auto filesStarted = std::chrono::steady_clock::now();

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
        if (line.empty() || line == "files.txt" || IsPageFile(line))
        {
            continue;
        }
        const std::string inGame = std::string(GameDir) + "/" + line;
        if (::access(inGame.c_str(), F_OK) == 0)
        {
            ++count; // from the startup pack
            continue;
        }
        if (!CreateOnServer(line, madeServerFolders))
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

    // The time for files.txt (a download), then for the files and links made from it.
    std::printf("Web file system: %zu game files, %zu of them downloaded when first read (files.txt %s %.0f ms, "
                "files and links %.0f ms, mount %.0f ms)\n",
                count, linked, pageList.empty() ? "from the mount" : "from the page", listMs, MsSince(filesStarted),
                MsSince(started));
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

// The language picked on the site, then the browser's languages, one a line. The site's is /play/?lang=
// when the page takes it (en, ru, ro, pt, es: index.html pickPageLang), else the one the page kept from an earlier
// ?lang= (mu-page-lang); an empty line when none.
EM_JS(char*, MuWeb_PageLanguages, (), {
    let site = "";
    try {
        const asked = (new URLSearchParams(location.search).get("lang") || "").toLowerCase();
        site = ["en", "ru", "ro", "pt", "es"].indexOf(asked) >= 0 ? asked : (localStorage.getItem("mu-page-lang") || "");
    } catch (e) {
    }
    const browser = navigator.languages && navigator.languages.length ? Array.from(navigator.languages)
                                                                      : [navigator.language || ""];
    return stringToNewUTF8([site].concat(browser).join("\n"));
});

// The site's language the game saw on the last visit; empty before the first time.
EM_JS(char*, MuWeb_LastSiteLanguage, (), {
    let last = "";
    try {
        last = localStorage.getItem("mu-web/game-site-lang") || "";
    } catch (e) {
    }
    return stringToNewUTF8(last);
});

// Keeps site (when not empty) as the language the game has seen, after the config.ini that holds the
// language picked for it (configText; null when the language stayed): index.html's saveConfig keeps
// config.ini only every 3 s, and a game that crashed before would come back with the old language and
// the new site language marked as seen. No storage room for the config: the site's isn't marked either.
EM_JS(void, MuWeb_KeepSiteLanguage, (const char* site, const char* configText), {
    try {
        if (configText) {
            localStorage.setItem("mu-web/config.ini", UTF8ToString(configText));
        }
        const now = UTF8ToString(site);
        if (now) {
            localStorage.setItem("mu-web/game-site-lang", now);
        }
    } catch (e) {
    }
});

// The game's language for this visit (Core/Platform/FirstVisitLocale.h), into config's [UI] Locale. A
// browser's first visit (no config.ini kept) plays in the site's or the browser's language when the game
// has it, the template's English otherwise. A later visit keeps the player's language, unless the language
// picked on the site changed since the last visit: then the game picks again as on a first visit, so the
// page and the game speak the same language (ghinzuro 2026-11-21). The changed config.ini is kept at once
// (MuWeb_KeepSiteLanguage), so the next visit starts with it.
void PickGameLocale(std::string& config, bool firstVisit)
{
    char* text = MuWeb_PageLanguages();
    std::vector<std::string> lines;
    std::string_view rest(text);
    for (;;)
    {
        const auto end = rest.find('\n');
        lines.emplace_back(rest.substr(0, end));
        if (end == std::string_view::npos)
        {
            break;
        }
        rest.remove_prefix(end + 1);
    }
    std::free(text);
    const std::string site = lines.front();
    lines.erase(lines.begin());
    char* last = MuWeb_LastSiteLanguage();
    const bool siteChanged = Core::Platform::SiteLanguageChanged(site, last);
    std::free(last);
    if (!firstVisit && !siteChanged)
    {
        return;
    }
    const std::string why = firstVisit ? std::string("first visit in this browser")
                                       : "the site's language changed (" + site + ")";
    // Japanese and traditional Chinese wait for Options: their font comes after the first map here.
    const Core::Platform::FirstVisitLocale pick = Core::Platform::PickFirstVisitLocale(site, lines, false);
    if (!pick.code.empty() && Core::Platform::SetConfigLocale(config, pick.code))
    {
        MuWeb_KeepSiteLanguage(site.c_str(), config.c_str());
        std::printf("Web file system: %s, the game's language %s (from %s)\n", why.c_str(), pick.code.c_str(),
                    pick.fromSite ? "the site" : "the browser");
    }
    else
    {
        MuWeb_KeepSiteLanguage(site.c_str(), nullptr);
        std::printf("Web file system: %s, the game's language stays %s\n", why.c_str(),
                    firstVisit ? "English" : "as it was");
    }
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int MuWeb_FilesOnPageThread()
{
    return g_filesOnPageThread.load();
}

namespace
{
// The maps entered so far (mu::web::LoadWorldFiles) and the last one's folder number: fetch-backend.js writes a
// line per map entry about the files then read one at a time.
std::atomic<int> g_mapEntries{0};
std::atomic<int> g_mapEntryWorld{0};
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int MuWeb_MapEntries()
{
    return g_mapEntries.load();
}

extern "C" EMSCRIPTEN_KEEPALIVE int MuWeb_MapEntryWorld()
{
    return g_mapEntryWorld.load();
}

extern "C" FILE* __real_fopen(const char* path, const char* mode);

#ifdef MU_WEB_FILE_TRACE
// For tools/web/make_map_files.py, in a build made with -DMU_WEB_FILE_TRACE only: the files the game
// opens, on the page's thread, to show a monster or an NPC. MuWeb_TraceMonster(type) puts one beside the hero and takes it
// away again; it returns the model the game gave it and, a line each, the files it opened for it. A
// file is read once in a run, so the tool adds what the model's first call read to the later ones.
// MuWeb_TraceTake() returns the files of the mount opened since its last call, a line each: the ones no
// pack held when the game wanted them.
std::string g_traceFiles;
std::string g_traceMountFiles;
bool g_tracing = false;

void TraceOpen(const char* path)
{
    if (!path || !emscripten_is_main_browser_thread())
    {
        return;
    }
    if (g_tracing)
    {
        g_traceFiles += path;
        g_traceFiles += '\n';
    }
    char target[PATH_MAX];
    if (const char* fileInMount = FileInMount(path, target))
    {
        g_traceMountFiles += fileInMount;
        g_traceMountFiles += '\n';
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* MuWeb_TraceTake()
{
    static std::string result;
    result = std::exchange(g_traceMountFiles, {});
    return result.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* MuWeb_TraceMonster(int type)
{
    constexpr int TraceKey = 9990;
    static std::string result;
    g_traceFiles.clear();
    g_tracing = true;
    CHARACTER* monster = Hero ? CreateMonster(static_cast<EMonsterType>(type), Hero->PositionX, Hero->PositionY, TraceKey) : nullptr;
    g_tracing = false;
    result = std::to_string(monster ? static_cast<int>(monster->Object.Type) : -1) + "\n" + g_traceFiles;
    if (monster)
    {
        DeleteCharacter(TraceKey);
    }
    return result.c_str();
}
#endif

// Every fopen of the program comes here (src/CMakeLists.txt links with -Wl,--wrap=fopen), std::ifstream's
// and miniaudio's among them. Each read of a file of the mount is a round trip to the thread that runs it
// (fetch-backend.js), tens of microseconds even with the file in memory there, and the game reads in
// small pieces: stdio 1 KB at a time, std::ifstream 4 KB (a .jpg or .tga texture is read through
// istreambuf_iterator), miniaudio seeking around a .wav. Read whole in one (OpenWholeInMemory), the rest
// comes from memory: when monsters came into sight on a walk out of Lorencia, the page thread waited
// for files in 1 and 9 profile samples instead of 9 and 11 (Edge, 2026-10-30).
extern "C" FILE* __wrap_fopen(const char* path, const char* mode)
{
#ifdef MU_WEB_FILE_TRACE
    TraceOpen(path);
#endif
    if (FILE* inMemory = OpenWholeInMemory(path, mode))
    {
        return inMemory;
    }
    return __real_fopen(path, mode);
}

bool mu::web::ReadWholeAhead(const char* path)
{
    char target[PATH_MAX];
    const char* fileInMount = path && g_wholeReads.load(std::memory_order_relaxed) ? FileInMount(path, target) : nullptr;
    MemoryFile* file = fileInMount ? ReadIntoMemoryOnPage(fileInMount) : nullptr;
    if (!file)
    {
        return false;
    }
    MemoryFile* earlier;
    {
        std::lock_guard lock(g_readAheadLock);
        earlier = std::exchange(g_readAhead, file);
        g_readAheadFile = fileInMount;
    }
    std::free(earlier);
    return true;
}

namespace
{
// The files of the deferred parts (index.html Module.muDeferredFiles: the startup pack's sounds and
// music, tools/web/make_startup_pack.py) whose part hasn't come or failed yet, as the game opens them
// (below /game). The page's thread only.
std::unordered_set<std::string> g_waitingDeferredFiles;

// Index.html's Module.muDeferredFiles: the paths of the deferred parts' files, a line each, to free; null
// without any.
EM_JS(char*, MuWeb_DeferredFiles, (), {
    const files = Module["muDeferredFiles"] ? Module["muDeferredFiles"]() : [];
    return files.length ? stringToNewUTF8(files.join("\n")) : 0;
});

// What MuWeb_InstallDeferredSlice tells about the part it worked on.
struct DeferredSlice
{
    int written = 0; // files of the part written so far
    int files = 0;   // files in the part
    int done = 0;    // 1 once the part's last file was written (or the part failed)
};

// Index.html's Module.muDeferredPart: the next deferred part that came ({files, bytes}) or failed (bytes
// null), handed over once. Writes the part's files into /game in place of their links to /server, as the
// late pack's (a file that doesn't write keeps its link), as many as `budgetMs` allows (at least one),
// the rest at the next calls, and returns the paths it dealt with, a line each, to free (a failed part's
// all at once), filling *slice; null while no part waits.
EM_JS(char*, MuWeb_InstallDeferredSlice, (double budgetMs, DeferredSlice* slice), {
    let job = Module["muDeferredJob"];
    if (!job) {
        const part = Module["muDeferredPart"] ? Module["muDeferredPart"]() : null;
        if (!part) {
            return 0;
        }
        job = Module["muDeferredJob"] = {part, at: 0, offset: 0, count: 0, failed: 0, firstError: null};
    }
    const part = job.part;
    const files = part.files;
    const paths = [];
    if (!part.bytes) {
        files.forEach(([path]) => paths.push(path));
        job.at = files.length;
    }
    const start = performance.now();
    while (part.bytes && job.at < files.length) {
        const [path, size] = files[job.at++];
        const offset = job.offset;
        job.offset += size;
        paths.push(path);
        const full = "/game/" + path;
        try {
            FS.unlink(full);
        } catch (e) {
        }
        // A file that doesn't write gets its link to /server back, so the game downloads it alone;
        // the part's other files still go in.
        let wrote = false;
        try {
            FS.mkdirTree(full.substring(0, full.lastIndexOf("/")));
            wrote = FS.writeFile(full, part.bytes.subarray(offset, offset + size)) >= 0;
        } catch (e) {
            job.firstError = job.firstError || e;
        }
        if (wrote) {
            ++job.count;
        } else {
            ++job.failed;
            try {
                FS.unlink(full);
            } catch (e) {
            }
            try {
                FS.symlink("/server/" + path, full);
            } catch (e) {
                job.firstError = job.firstError || e;
            }
        }
        if (performance.now() - start >= budgetMs) {
            break;
        }
    }
    const done = job.at >= files.length;
    if (done) {
        Module["muDeferredJob"] = null;
        if (job.failed) {
            err("Deferred part: " + job.failed + " files didn't write (" + (job.firstError || "no reason given") +
                "); the game downloads them one by one");
        }
    }
    const at = (slice >>> 0) >>> 2;
    HEAP32[at] = job.count;
    HEAP32[at + 1] = files.length;
    HEAP32[at + 2] = done ? 1 : 0;
    return stringToNewUTF8(paths.join("\n"));
});

// The lines of `text`, one call each. Returns how many there were.
template <typename Visit>
std::size_t ForEachLine(std::string_view text, Visit visit)
{
    std::size_t count = 0;
    while (!text.empty())
    {
        const std::size_t end = std::min(text.find('\n'), text.size());
        visit(text.substr(0, end));
        text.remove_prefix(std::min(end + 1, text.size()));
        ++count;
    }
    return count;
}

PackInstallTiming g_deferredPartTiming;

// Writes the deferred parts' files that came for at most `budgetMs` (at least one file where it is above
// 0), letting go of their paths in g_waitingDeferredFiles as they are written, so the sounds waiting for
// them load.
void InstallDeferredPartsWithin(double budgetMs)
{
    if (g_waitingDeferredFiles.empty() || !(budgetMs > 0))
    {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    for (double left = budgetMs; left > 0; left = budgetMs - MsSince(started))
    {
        const auto sliceStarted = std::chrono::steady_clock::now();
        DeferredSlice slice;
        char* paths = MuWeb_InstallDeferredSlice(left, &slice);
        if (!paths)
        {
            return;
        }
        ForEachLine(paths, [](std::string_view path) { g_waitingDeferredFiles.erase(std::string(path)); });
        std::free(paths);
        g_deferredPartTiming.Add(MsSince(sliceStarted));
        if (!slice.done)
        {
            return; // the budget is used up
        }
        const PackInstallTiming timing = std::exchange(g_deferredPartTiming, {});
        if (slice.written > 0)
        {
            std::printf("Web file system: %d of %d files from a deferred part (sounds, music) in %.0f ms over %d "
                        "frames (at most %.1f ms a frame)\n",
                        slice.written, slice.files, timing.totalMs, timing.frames, timing.longestMs);
        }
        else
        {
            std::printf("Web file system: a deferred part failed; its %d files are read one by one\n", slice.files);
        }
    }
}

// Index.html's Module.muWantDeferred: downloads the deferred part of `folder` it held back, if any.
EM_JS(void, MuWeb_WantDeferredPart, (const char* folder), {
    if (Module["muWantDeferred"]) {
        Module["muWantDeferred"](UTF8ToString(folder));
    }
});

// Fills g_waitingDeferredFiles from the page.
void NoteDeferredFiles()
{
    char* paths = MuWeb_DeferredFiles();
    if (!paths)
    {
        return;
    }
    ForEachLine(paths, [](std::string_view path) { g_waitingDeferredFiles.emplace(path); });
    std::free(paths);
}
} // namespace

bool mu::web::MountGameFiles()
{
    if (::mkdir(GameDir, 0777) != 0)
    {
        return false;
    }
    const auto packStart = std::chrono::steady_clock::now();
    const int packed = MuWeb_InstallStartupPack();
    if (packed > 0)
    {
        const double packMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - packStart).count();
        std::printf("Web file system: %d files from the startup pack in %.0f ms (%.0f ms writing)\n", packed, packMs,
                    MuWeb_StartupPackWriteMs());
    }

    // Asked here, on the page's thread: a worker's location is its script's.
    char* pageFolder = MuWeb_PageFolderUrl();
    g_pageFolderUrl = pageFolder;
    std::free(pageFolder);

    g_filesOnPageThread = MuWeb_PageWantsFilesOnPageThread();
    g_wholeReads = MuWeb_PageWantsWholeReads() != 0;
    if (g_filesOnPageThread && g_wholeReads)
    {
        g_pageReads = MuWeb_SetUpPageReads() != 0;
        std::printf("Web file system: files the game opens whole come %s\n",
                    g_pageReads ? "through the service worker while the page runs"
                                : "by synchronous requests (?syncreads)");
    }
    // Taken here: a worker's Module isn't the page's.
    std::string pageList;
    if (char* list = MuWeb_TakeFileList())
    {
        pageList = list;
        std::free(list);
    }
    std::thread worker([&pageList] { SetUpMount(pageList); });
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
            std::string saved(merged);
            std::free(merged);
            std::printf("Web file system: config.ini from this browser's earlier visits\n");
            PickGameLocale(saved, false);
            WriteWholeFile(configPath, saved);
        }
        else
        {
            PickGameLocale(config, true);
            WriteWholeFile(configPath, config);
        }
    }
    NoteDeferredFiles();
    // A part kept on this device may be here already: then the game loads its sounds as before, the
    // whole part now (no frame to keep).
    InstallDeferredPartsWithin(std::numeric_limits<double>::infinity());
    return ::chdir(GameDir) == 0;
}

// Downloads the files (paths below the page's folder, one a line), or reads them from the service
// worker's store, where it also keeps them for the next visit under the URL the mount asks for
// (fetch-backend.js readKept). Their bytes wait in Module.muLateFontFiles for MuWeb_InstallLateFonts.
// A file without a hash in startup.json is in the startup pack: nothing to do.
// Module.muLateFonts: "loading", "ready", or unset (not started, or failed).
EM_JS(void, MuWeb_DownloadLateFonts, (const char* paths), {
    if (Module["muLateFonts"]) {
        return;
    }
    Module["muLateFonts"] = "loading";
    const files = UTF8ToString(paths).split("\n").filter(file => file);
    (async () => {
        // startup.json's hashes: asked for already where the page reads files (MuWeb_SetUpPageReads).
        const versions = await (Module["muFileVersions"] ||
            fetch(new URL("startup.json", location.href), {cache: "no-cache"})
                .then((response) => response.ok ? response.json() : {})
                .then((index) => index.versions || {}, () => ({})));
        const store = self.caches ? await caches.open("mu-web/files").catch(() => null) : null;
        const arrived = [];
        for (const file of files) {
            const hash = versions && versions[file];
            if (!hash) {
                continue;
            }
            const url = new URL(`${file}?v=${hash}`, location.href).href;
            let response = store ? await store.match(url).catch(() => null) : null;
            if (!response) {
                response = await fetch(url, {credentials: "same-origin"});
                if (!response.ok) {
                    throw new Error(`${file}: ${response.status}`);
                }
                if (store) {
                    await store.put(url, response.clone()).catch(() => {});
                }
            }
            arrived.push([file, new Uint8Array(await response.arrayBuffer())]);
        }
        return arrived;
    })().then(arrived => {
        Module["muLateFontFiles"] = arrived;
        Module["muLateFonts"] = "ready";
    }, e => {
        err("Late fonts: " + e + "; trying again at the next map");
        Module["muLateFonts"] = null;
    });
});

// Once the download finished, writes its files into /game in place of their links to /server, as the
// startup pack's are: SDL_ttf opens Noto Sans TC four times (a size each), and each open read it whole
// through the mount, 0.7 s on a PC (2026-11-03). Returns 1 then, else 0.
EM_JS(int, MuWeb_InstallLateFonts, (), {
    if (Module["muLateFonts"] !== "ready") {
        return 0;
    }
    for (const [file, bytes] of Module["muLateFontFiles"] || []) {
        const full = "/game/" + file;
        try {
            FS.unlink(full);
        } catch (e) {
        }
        if (FS.writeFile(full, bytes) < 0) {
            FS.symlink("/server/" + file, full);
        }
    }
    Module["muLateFontFiles"] = null;
    return 1;
});

void mu::web::DownloadLateFonts()
{
    static bool ready = false;
    if (ready || (ready = LateFontsReady()))
    {
        return;
    }
    std::string paths;
    for (const BundledFont& font : kBundledFallbackFonts)
    {
        if (IsLateFallbackFont(font))
        {
            for (const char* path : {font.regular, font.bold})
            {
                if (paths.find(path) == std::string::npos)
                {
                    paths.append(path).push_back('\n');
                }
            }
        }
    }
    MuWeb_DownloadLateFonts(paths.c_str());
}

bool mu::web::LateFontsReady()
{
    static bool ready = false;
    if (!ready && MuWeb_InstallLateFonts())
    {
        ready = true;
        std::printf("Web file system: Chinese and Korean fonts downloaded\n");
    }
    return ready;
}

// Index.html's Module.muLatePack: the late pack ({files, bytes}) once it is here, handed over once.
// Writes its files into /game in place of their links to /server, as many as `budgetMs` allows (at least
// one), the rest at the next calls; a file that doesn't write keeps its link. Returns how many it wrote
// so far, -1 while the pack isn't here; *done becomes 1 with the pack's last file.
EM_JS(int, MuWeb_InstallLatePackSlice, (double budgetMs, int* done), {
    let job = Module["muLatePackJob"];
    if (!job) {
        const pack = Module["muLatePack"] ? Module["muLatePack"]() : null;
        if (!pack) {
            return -1;
        }
        job = Module["muLatePackJob"] = {pack, at: 0, offset: 0, written: 0};
    }
    const files = job.pack.files;
    const start = performance.now();
    while (job.at < files.length) {
        const [path, size] = files[job.at++];
        const offset = job.offset;
        job.offset += size;
        const full = "/game/" + path;
        try {
            FS.unlink(full);
        } catch (e) {
        }
        if (FS.writeFile(full, job.pack.bytes.subarray(offset, offset + size)) >= 0) {
            ++job.written;
        } else {
            FS.symlink("/server/" + path, full);
        }
        if (performance.now() - start >= budgetMs) {
            break;
        }
    }
    if (job.at >= files.length) {
        Module["muLatePackJob"] = null;
        HEAP32[(done >>> 0) >>> 2] = 1;
    }
    return job.written;
});

// The late pack's files are all in /game (InstallLatePack).
static bool g_latePackInstalled = false;

bool mu::web::LatePackInstalled()
{
    return g_latePackInstalled;
}

double mu::web::InstallLatePack(double budgetMs)
{
    static PackInstallTiming timing;
    if (g_latePackInstalled || !(budgetMs > 0))
    {
        return 0;
    }
    const auto start = std::chrono::steady_clock::now();
    int done = 0;
    const int written = MuWeb_InstallLatePackSlice(budgetMs, &done);
    const double ms = MsSince(start);
    if (written < 0)
    {
        return ms;
    }
    timing.Add(ms);
    if (!done)
    {
        return ms;
    }
    g_latePackInstalled = true;
    std::printf("Web file system: %d files from the late pack in %.0f ms over %d frames (at most %.1f ms a frame)\n",
                written, timing.totalMs, timing.frames, timing.longestMs);
    return ms;
}

double mu::web::InstallDeferredParts(double budgetMs)
{
    const auto start = std::chrono::steady_clock::now();
    InstallDeferredPartsWithin(budgetMs);
    return MsSince(start);
}

bool mu::web::WaitsForDeferredPart(const std::string& path)
{
    return !g_waitingDeferredFiles.empty() && g_waitingDeferredFiles.contains(path);
}

std::size_t mu::web::DeferredFilesWaiting()
{
    return g_waitingDeferredFiles.size();
}

void mu::web::WantDeferredPart(const char* folder)
{
    MuWeb_WantDeferredPart(folder);
}

void mu::web::LoadWorldFiles(int world)
{
    // The world first: fetch-backend.js asks for it once it sees the count change.
    g_mapEntryWorld = world;
    ++g_mapEntries;
    const auto start = std::chrono::steady_clock::now();
    const int written = MuWeb_InstallMapPack(world);
    const double ms = MsSince(start);
    if (written > 0)
    {
        std::printf("Web file system: map folder %d, %d files from its pack in %.0f ms\n", world, written, ms);
    }
    else if (written == 0)
    {
        std::printf("Web file system: map folder %d, no pack (%.0f ms)\n", world, ms);
    }
    else if (written == -2)
    {
        std::printf("Web file system: map folder %d, its pack failed to load; keeping the previous files (%.0f ms)\n",
                    world, ms);
    }
    else
    {
        std::printf("Web file system: map folder %d, its pack is in place already\n", world);
    }
}

void mu::web::PrefetchWorldFiles(int world)
{
    if (world > 0)
    {
        MuWeb_PrefetchMapPack(world);
    }
}

void mu::web::RememberCharacterWorld(const std::string& name, int world)
{
    if (!name.empty() && world > 0)
    {
        MuWeb_RememberCharacterWorld(name.c_str(), world);
    }
}

int mu::web::RememberedCharacterWorld(const std::string& name)
{
    return name.empty() ? 0 : MuWeb_RememberedCharacterWorld(name.c_str());
}
