#pragma once

// Web build only. In the browser the game's files (Data/, fonts/, shaders/, config.ini ...) stay on
// the web server. The files the game reads before it can be played come in one download,
// startup.pack (tools/web/make_startup_pack.py), which the page fetches while the game's code loads;
// the others download through a WasmFS fetch mount, in chunks, the first time the game reads them.
namespace mu::web
{
// Puts the startup pack's files and links to all other files listed in the server's files.txt
// (written by the build) into /game, and makes it the game's working directory. Blocks until that's
// done, yielding to the browser meanwhile. Returns false when the file list can't be read.
bool MountGameFiles();

// Brings the files of the map folders Data/World<world> and Data/Object<world> in one download (the
// map's pack, packs/world<world>, tools/web/make_startup_pack.py) before the game reads them, instead of
// a request per file. Call it before loading a map; blocks until done, yielding to the browser meanwhile.
// Without a pack the files still download one by one as the game opens them.
void LoadWorldFiles(int world);

// Where the page's thread downloads the game's files (Safari), reads the file at `path` whole now, the
// game waiting for it the way it waits for a frame, through the service worker, which keeps it on the
// device; the next fopen of it, on any thread, gets it from memory. For a file another thread opens:
// miniaudio opens a music stream on its own, and there the file came by synchronous requests on the
// page's thread, a stat's 2 MB and then 1 MB parts as it played, each one stopping the game. One such
// file waits at a time. False where it didn't read it (other browsers, a file not in the mount, a failed
// download): open it as usual.
bool ReadWholeAhead(const char* path);
} // namespace mu::web
