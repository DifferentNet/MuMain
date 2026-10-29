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
} // namespace mu::web
