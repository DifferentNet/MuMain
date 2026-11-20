/**
 * Web build: the game's fetch mount (/server, source/Core/Platform/web/WebFileSystem.cpp) with one
 * request per file instead of two. It replaces Emscripten's own (src/lib/libwasmfs_fetch.js,
 * Emscripten 6.0.13; compare it when updating Emscripten), which first asks for a file's size (a HEAD
 * request) and only then for its data. The game opens its files one after another and waits for each,
 * so on a phone each file cost two trips to the server: entering a map with a few hundred of them took
 * a minute (2026-10-29).
 *
 * This one asks for the first two chunks at once. The answer brings the file's size too (its
 * Content-Range), and a file of up to two chunks (nearly all of them) is complete with it. Bigger ones
 * (music, long sounds) read the rest chunk by chunk, as Emscripten's does.
 *
 * A file with a hash in startup.json ("versions", tools/web/make_startup_pack.py) is asked for as
 * path?v=<hash>: the page's service worker (sw.js) keeps it on the device under that URL and goes to
 * the server again only when a release changes the file, and so its hash. A file it already keeps is
 * read from there directly (readKept). Without startup.json or a hash, the plain URL. startup.json is
 * the page's copy (index.html, Module.muReleaseIndexText: the release the running code belongs to),
 * and this asks the server for its own only where the page has none: a game this device keeps, opened
 * while a busy server refused that second ask, read every file without its hash from the server, 3 s
 * each and failing, instead of from the device (2026-11-20).
 *
 * Files the game read together on an earlier visit (a monster's models, textures and sounds, the first
 * time it is seen) are read together again: when the first of them is read, the rest start too
 * (readGroups), instead of one after another as the game asks for them; and while the game reads
 * nothing new, the groups' files are read from the device ahead of time (warmUp). On the first visit
 * nothing is learned yet: startup.json's "groups" say which files follow a model (its textures and
 * sounds, tools/web/make_startup_pack.py), and those start with the model too (builtGroups). A phone
 * that saw a monster for the first time waited for some twenty downloads one after another, a second
 * or two of a still picture (2026-11-10).
 *
 * The downloads run in the mount's worker, except where the page decides (index.html,
 * MuWeb_FilesOnPageThread in WebFileSystem.cpp) that the page's thread runs them: WebKit (Safari,
 * every iPhone browser) starts a worker's download on the page's thread, and the game, which runs
 * there, holds it while it waits for this worker, so the download never started (Safari 27,
 * 2026-10-29). While it waits, the page's thread runs work other threads send it (Emscripten's
 * emscripten_futex_wait.c), and a synchronous request needs no event loop: the worker sends it
 * downloadOnPageThread.
 *
 * A busy web server answers 429 or 503 with Retry-After (server/web/serve-web.py caps the file
 * requests it answers at a time, per player address and in all): such a read is asked again
 * (askWhenFree, BUSY_GIVE_UP_MS), where before it failed at once and the game had a file missing.
 */
addToLibrary({
  // On the page's thread: bytes start..end of the file at `url` (a C string) go to `buffer` when they
  // fit in `capacity`; the answer's status, the file's size (from Content-Range, -1 without), its
  // Retry-After in seconds (-1 without) and 1 where the service worker answered it from the device
  // (sw.js, X-Mu-Kept; else 0) go to `info` (four doubles). Returns how many bytes the answer has, -1
  // when none came.
  $downloadOnPageThread__proxy: 'sync',
  $downloadOnPageThread__deps: ['$UTF8ToString'],
  $downloadOnPageThread: function(url, start, end, buffer, capacity, info) {
    var request = new XMLHttpRequest();
    try {
      request.open('GET', UTF8ToString(url), false);
      request.setRequestHeader('Range', `bytes=${start}-${end}`);
      // A synchronous request can't ask for bytes: one character per byte instead.
      request.overrideMimeType('text/plain; charset=x-user-defined');
      request.send();
    } catch (_e) {
      return -1;
    }
    var text = request.responseText;
    var total = /\/(\d+)$/.exec(request.getResponseHeader('Content-Range') || '');
    var retryAfter = request.getResponseHeader('Retry-After');
    var kept = request.getResponseHeader('X-Mu-Kept') ? 1 : 0;
    {{{ makeSetValue('info', 0, 'request.status', 'double') }}};
    {{{ makeSetValue('info', 8, 'total ? parseInt(total[1], 10) : -1', 'double') }}};
    {{{ makeSetValue('info', 16, 'retryAfter && isFinite(retryAfter) ? Number(retryAfter) : -1', 'double') }}};
    {{{ makeSetValue('info', 24, 'kept', 'double') }}};
    if (text.length <= capacity) {
      for (var i = 0; i < text.length; i++) {
        HEAPU8[buffer + i] = text.charCodeAt(i) & 0xff;
      }
    }
    return text.length;
  },

  // On the page's thread: startup.json's text as the page read it (index.html, Module.muReleaseIndexText,
  // handed over once), in memory this mount frees (malloc); 0 where the page has none.
  $releaseIndexOnPageThread__proxy: 'sync',
  $releaseIndexOnPageThread__deps: ['$stringToNewUTF8'],
  $releaseIndexOnPageThread: function() {
    var text = Module['muReleaseIndexText'] ? Module['muReleaseIndexText']() : null;
    return typeof text == 'string' ? stringToNewUTF8(text) : 0;
  },

  _wasmfs_create_fetch_backend_js__deps: [
    '$wasmFS$backends',
    '$wasmFS$JSMemoryRanges',
    '$UTF8ToString',
    '$stringToNewUTF8',
    '$downloadOnPageThread',
    '$releaseIndexOnPageThread',
    'malloc',
    'free',
    'MuWeb_FilesOnPageThread',
    '_wasmfs_fetch_get_file_url',
    '_wasmfs_fetch_get_chunk_size',
  ],
  _wasmfs_create_fetch_backend_js: async function(backend) {
    // This runs in a worker, whose location is Main.js's: the page's folder.
    var pageFolder = new URL('.', self.location.href).href;

    // A busy server's answer (HTTP 429 or 503) is asked again (askWhenFree): after its Retry-After, and
    // at least BUSY_FIRST_WAIT_MS, doubling with each busy answer in a row up to BUSY_MOST_WAIT_MS, up to
    // a quarter longer at random, so reads refused together don't all come back together; for up to
    // BUSY_GIVE_UP_MS in all (some six tries), and then the busy answer fails the read as before. The
    // game waits for each read, one after another, so once a read has given up the next ones get only
    // BUSY_AFTER_GIVE_UP_MS until the server answers something else (serverBusy): a map's hundred reads
    // don't each hold the game for half a minute. The page does the same for its own downloads
    // (index.html, fetchWhenFree).
    var BUSY_FIRST_WAIT_MS = 1000;
    var BUSY_MOST_WAIT_MS = 8000;
    var BUSY_GIVE_UP_MS = 30000;
    var BUSY_AFTER_GIVE_UP_MS = 3000;
    var serverBusy = false;
    // At most one line every BUSY_NOTE_MS about the reads waiting for a busy server.
    var BUSY_NOTE_MS = 10000;
    var busyNotedAt = -BUSY_NOTE_MS;
    var busySinceNote = 0;

    function busy(answer) {
      return answer.status == 429 || answer.status == 503;
    }

    // Retry-After in seconds, -1 without (or in its date form, which the web server doesn't send).
    function retryAfterSeconds(text) {
      return text && isFinite(text) ? Number(text) : -1;
    }

    // An answer from the server that isn't busy ends serverBusy; one the service worker gave from the
    // device (`kept`, sw.js X-Mu-Kept) doesn't: the server may still be refusing the files it lacks.
    function heard(answer) {
      if (answer.status && !busy(answer) && !answer.kept) {
        serverBusy = false;
      }
      return answer;
    }

    // `ask()` (one try: a promise of {status, retryAfter, ...}) until its answer isn't a busy server's
    // or the time is up (see BUSY_GIVE_UP_MS); gives the last answer. `what` names it in the log.
    async function askWhenFree(ask, what) {
      var started = performance.now();
      var budget = serverBusy ? BUSY_AFTER_GIVE_UP_MS : BUSY_GIVE_UP_MS;
      for (var tries = 0; ; tries++) {
        var answer = await ask();
        if (!busy(answer)) {
          return answer;
        }
        var wait = Math.max(answer.retryAfter * 1000, Math.min(BUSY_FIRST_WAIT_MS * 2 ** tries, BUSY_MOST_WAIT_MS)) *
                   (1 + Math.random() / 4);
        var waited = performance.now() - started;
        if (waited + wait > budget) {
          serverBusy = true;
          console.warn(`Web files: ${what}: the server stayed busy (HTTP ${answer.status}) for ` +
                       `${(waited / 1000).toFixed(1)} s, ${tries + 1} tries; given up`);
          return answer;
        }
        busySinceNote++;
        if (performance.now() - busyNotedAt >= BUSY_NOTE_MS) {
          console.warn(`Web files: the server is busy (HTTP ${answer.status}): ${busySinceNote} busy answer(s) since ` +
                       `the last line; reads ask again after its Retry-After, at least 1 s doubling up to 8 s`);
          busyNotedAt = performance.now();
          busySinceNote = 0;
        }
        await new Promise((wake) => setTimeout(wake, wait));
      }
    }

    var versions = null;
    var builtGroups = new Map(); // a model's path -> the paths of the files read after it
    // startup.json's "versions" (path -> hash), with its "groups" ([[model, file, ...]], numbers counting
    // the paths of "versions" from 0) put in builtGroups.
    function hashesOf(index) {
      var hashes = index.versions || {};
      var paths = Object.keys(hashes);
      for (var group of index.groups || []) {
        builtGroups.set(paths[group[0]], group.slice(1).map((number) => paths[number]));
      }
      return hashes;
    }

    // The page's copy of startup.json (releaseIndexOnPageThread), parsed; null where the page has none.
    function pageReleaseIndex() {
      var text = releaseIndexOnPageThread();
      if (!text) {
        return null;
      }
      try {
        return JSON.parse(UTF8ToString(text));
      } catch (_e) {
        return null;
      } finally {
        _free(text);
      }
    }

    var pageIndexAsked = false;
    function fileVersions() {
      if (!versions && !pageIndexAsked) {
        pageIndexAsked = true;
        var index = pageReleaseIndex();
        try {
          if (index) {
            versions = Promise.resolve(hashesOf(index));
          }
        } catch (_e) {
          // Malformed "groups": this asks the server for its own below.
        }
      }
      if (!versions) {
        // The page has no copy: this asks the server for its own.
        if (serverBusy) {
          // A read has given up on the busy server: asking for startup.json first would hold each read
          // twice as long. Asked for again once the server answers something else.
          return Promise.resolve({});
        }
        versions = askWhenFree(async () => {
          var response = await fetch(new URL('startup.json', pageFolder), {cache: 'no-cache'});
          return heard({status: response.status, retryAfter: retryAfterSeconds(response.headers.get('Retry-After')), response});
        }, 'startup.json')
          .then((answer) => {
            if (busy(answer)) {
              // Asked for again at the next read: without the hashes every file would come from the
              // server for the rest of the visit, even those this device keeps.
              versions = null;
              return {};
            }
            return answer.response.ok ? answer.response.json() : {};
          })
          .then(hashesOf, () => ({}));
      }
      return versions;
    }

    async function fileUrl(file) {
      var url = UTF8ToString(__wasmfs_fetch_get_file_url(file));
      if (url.indexOf('://') === -1) {
        try {
          url = new URL(url, self.location.origin).toString();
        } catch (_e) {
          throw {status: 404};
        }
      }
      if (!url.startsWith(pageFolder)) {
        return url;
      }
      // The mount joins its folder and the file's path with a slash too many (Emscripten's
      // fetch_backend.cpp: folder + '/' + '/Data/...').
      var path = url.slice(pageFolder.length).replace(/^\/+/, '');
      var hash = (await fileVersions())[path];
      return pageFolder + path + (hash ? `?v=${hash}` : '');
    }

    // See downloadOnPageThread: {status, size, bytes, retryAfter, kept}, or {status: 0} without an answer.
    function askPageThread(url, start, end) {
      var urlText = stringToNewUTF8(url);
      var info = _malloc(32);
      var capacity = end - start + 1;
      var buffer = _malloc(capacity);
      try {
        var length = buffer ? downloadOnPageThread(urlText, start, end, buffer, capacity, info) : -1;
        if (length > capacity) {
          // A server that ignores ranges sent the whole file: once more with room for it.
          _free(buffer);
          capacity = length;
          buffer = _malloc(capacity);
          length = buffer ? downloadOnPageThread(urlText, start, end, buffer, capacity, info) : -1;
        }
        if (length < 0 || length > capacity) {
          return {status: 0};
        }
        return {
          status: {{{ makeGetValue('info', 0, 'double') }}},
          size: {{{ makeGetValue('info', 8, 'double') }}},
          bytes: HEAPU8.slice(buffer, buffer + length),
          retryAfter: {{{ makeGetValue('info', 16, 'double') }}},
          kept: {{{ makeGetValue('info', 24, 'double') }}} == 1,
        };
      } finally {
        _free(urlText);
        _free(info);
        _free(buffer);
      }
    }

    // The files the page's service worker keeps on the device (sw.js, FILES): a kept file is read from
    // there without asking the service worker, which took 1.4 ms a file against 0.65 (Edge, 2026-10-30).
    // A monster seen for the first time opens some twenty files one after another, and the game's frame
    // waited for all of them. null where the browser keeps nothing for this page.
    var keptFiles;
    function keptFileStore() {
      if (keptFiles === undefined) {
        keptFiles = self.caches ? caches.open('mu-web/files').catch(() => null) : Promise.resolve(null);
      }
      return keptFiles;
    }

    // Bytes start..end of a file kept on the device, as download answers them (sw.js byteRange); null
    // where it isn't kept (yet: the service worker keeps a file a moment after it arrives).
    async function readKept(url, start, end) {
      if (url.indexOf('?v=') === -1) {
        return null;
      }
      try {
        var store = await keptFileStore();
        var kept = store && await store.match(url);
        if (!kept) {
          return null;
        }
        var blob = await kept.blob();
        var size = blob.size;
        if (start >= size) {
          return {status: 416, size: size, bytes: null};
        }
        var whole = start == 0 && end >= size - 1;
        var bytes = await (whole ? blob : blob.slice(start, end + 1)).arrayBuffer();
        return {status: 206, size: size, bytes: new Uint8Array(bytes)};
      } catch (_e) {
        return null;
      }
    }

    // Bytes start..end of a file: {status, size (the whole file's, from Content-Range; -1 without),
    // bytes (where the status is a success), retryAfter (seconds, -1 without)}. A busy server is asked
    // again (askWhenFree), unless `once`: a read ahead (readEarlyNow), which the game's own read takes
    // over from (takeEarly).
    async function download(url, start, end, once) {
      var ask = () => downloadOnce(url, start, end);
      return once ? ask() : askWhenFree(ask, pagePath(url) || url);
    }

    // One try of download: from the device where the file is kept, else from the server.
    async function downloadOnce(url, start, end) {
      if (_MuWeb_FilesOnPageThread()) {
        return heard(askPageThread(url, start, end));
      }
      var kept = await readKept(url, start, end);
      if (kept) {
        return kept;
      }
      var response = await fetch(url, {headers: {'Range': `bytes=${start}-${end}`}});
      var total = /\/(\d+)$/.exec(response.headers.get('Content-Range') || '');
      return heard({
        status: response.status,
        size: total ? parseInt(total[1], 10) : -1,
        bytes: response.ok ? new Uint8Array(await response.arrayBuffer()) : null,
        retryAfter: retryAfterSeconds(response.headers.get('Retry-After')),
        // The service worker kept the file after readKept looked.
        kept: response.headers.has('X-Mu-Kept'),
      });
    }

    function succeeded(answer) {
      return answer.status >= 200 && answer.status < 300;
    }

    // Read groups. A monster seen for the first time opens some twenty files one after another, each
    // a trip to the device's storage (readKept) that the game's frame waits for: 20 ms of a 67 ms
    // frame (Edge, 2026-10-30), while twenty read at once take 6. The game doesn't say which files
    // come next, so this learns it: the files first read close together on a visit (a burst) are a
    // group, named by the first of them, and the groups are kept on the device (readGroupsUrl). On the
    // next visit, reading a group's first file starts reading the others (readEarly), and the game's
    // own reads of them find them done or under way (takeEarly). Not where the page's thread
    // downloads (askPageThread): there the reads would only queue up behind each other.
    var readGroupsUrl = pageFolder + 'mu-web-read-groups.json';
    var READ_GROUPS_VERSION = 1;
    // Reads further apart than this start a new burst. A frame is 16.7 ms; between two of a model's
    // files the game spends a few ms decoding the first.
    var BURST_GAP_MS = 30;
    // The most files in a group, its first one included (a map's loading reads hundreds: they are cut
    // into groups of this size); and the most groups kept, the oldest dropped.
    var GROUP_MOST_FILES = 48;
    var MOST_GROUPS = 4000;
    // A file read early that the game hasn't asked for by then is let go.
    var EARLY_KEEP_MS = 10000;
    var SAVE_AFTER_MS = 3000;
    // Early reads at once, in the group's order (four at a time made the game wait longer for them).
    var EARLY_AT_ONCE = 16;
    // Warm-up (warmUp): once the game has read its first file of the visit, after WARM_AFTER_MS and then
    // whenever it has read nothing new for WARM_QUIET_MS, the files of the groups learned before are read
    // from the device one at a time, the newest groups first, up to WARM_MOST_BYTES in all. A monster's
    // files are then here before it shows, its first one included, which a group's early reads can't
    // help with: the frame that shows it waited 2.5 ms for that file alone.
    var WARM_AFTER_MS = 15000;
    var WARM_QUIET_MS = 1000;
    var WARM_MOST_BYTES = 8 << 20;

    var readGroups = new Map(); // first file's path -> the other files' paths
    var readThisVisit = new Set(); // paths
    var burst = null; // {paths, last}
    var saveTimer = 0;
    var groupsChanged = false;
    // url -> {end, waiting (true until its read starts), answer (a promise of download's answer, or of
    // null), settle (gives the answer), timer}; the warm-up's have no settle or timer.
    var early = new Map();
    var earlyWaiting = []; // urls, in order
    var earlyReading = 0;
    var lastFirstRead = 0; // performance.now() of the game's last first read of a file
    var firstReadEnd = 0; // the last byte of a first read (fetchStart's end)
    var warmTimer = 0;

    function loadReadGroups() {
      keptFileStore()
        .then((store) => store && store.match(readGroupsUrl))
        .then((kept) => kept && kept.json())
        .then((saved) => {
          if (!saved || saved.version !== READ_GROUPS_VERSION || !saved.groups) {
            return;
          }
          // Those learned on this visit meanwhile are newer.
          var learned = readGroups;
          readGroups = new Map(Object.entries(saved.groups));
          for (var [first, rest] of learned) {
            readGroups.delete(first);
            readGroups.set(first, rest);
          }
        })
        .catch(() => {});
    }

    function saveReadGroups() {
      saveTimer = 0;
      if (burst && performance.now() - burst.last > BURST_GAP_MS) {
        endBurst();
      }
      if (!groupsChanged) {
        return;
      }
      groupsChanged = false;
      var text = JSON.stringify({version: READ_GROUPS_VERSION, groups: Object.fromEntries(readGroups)});
      keptFileStore()
        .then((store) => store && store.put(readGroupsUrl, new Response(text, {headers: {'Content-Type': 'application/json'}})))
        .catch(() => {});
    }

    function endBurst() {
      var paths = burst.paths;
      burst = null;
      if (paths.length < 2) {
        return;
      }
      var first = paths[0];
      var rest = paths.slice(1);
      var before = readGroups.get(first);
      if (before && before.length == rest.length && before.every((path, i) => path == rest[i])) {
        return;
      }
      readGroups.delete(first);
      readGroups.set(first, rest);
      while (readGroups.size > MOST_GROUPS) {
        readGroups.delete(readGroups.keys().next().value);
      }
      groupsChanged = true;
    }

    // The file's path below the page's folder ("Data/..."), or null for a file from elsewhere.
    function pagePath(url) {
      return url.startsWith(pageFolder) ? url.slice(pageFolder.length).split('?')[0] : null;
    }

    // A file read for the first time on this visit (its first `end` + 1 bytes): learns the bursts, and
    // starts reading the rest of the group this file begins.
    function noteFirstRead(url, end) {
      var path = pagePath(url);
      if (!path || _MuWeb_FilesOnPageThread()) {
        return;
      }
      readThisVisit.add(path);
      var now = performance.now();
      lastFirstRead = now;
      firstReadEnd = end;
      if (!warmTimer) {
        warmTimer = setTimeout(warmUp, WARM_AFTER_MS);
      }
      if (burst && (now - burst.last > BURST_GAP_MS || burst.paths.length >= GROUP_MOST_FILES)) {
        endBurst();
      }
      if (burst) {
        burst.paths.push(path);
        burst.last = now;
      } else {
        burst = {paths: [path], last: now};
      }
      clearTimeout(saveTimer);
      saveTimer = setTimeout(saveReadGroups, SAVE_AFTER_MS);
      // The files learned on this device first, in the order the game read them, then the model's.
      var rest = (readGroups.get(path) || []).concat(builtGroups.get(path) || []);
      if (rest.length) {
        readEarly(rest, end);
      }
    }

    async function readEarly(paths, end) {
      var hashes = await fileVersions();
      var listed = Object.keys(hashes).length > 0;
      for (var path of paths) {
        if (readThisVisit.has(path)) {
          continue;
        }
        var hash = hashes[path];
        if (listed && !hash) {
          continue; // gone in a newer release
        }
        var url = pageFolder + path + (hash ? `?v=${hash}` : '');
        if (early.has(url)) {
          continue;
        }
        var read = {end: end, waiting: true};
        read.answer = new Promise((settle) => (read.settle = settle));
        read.timer = setTimeout(forgetEarly, EARLY_KEEP_MS, url, read);
        early.set(url, read);
        earlyWaiting.push(url);
      }
      startEarlyReads();
    }

    function forgetEarly(url, read) {
      if (early.get(url) === read) {
        early.delete(url);
      }
    }

    function startEarlyReads() {
      while (earlyReading < EARLY_AT_ONCE && earlyWaiting.length) {
        var url = earlyWaiting.shift();
        var read = early.get(url);
        if (!read || !read.waiting) {
          continue; // let go, or taken by the game before its turn
        }
        read.waiting = false;
        readEarlyNow(url, read);
      }
    }

    async function readEarlyNow(url, read) {
      earlyReading++;
      var answer = await download(url, 0, read.end, true).catch(() => null);
      earlyReading--;
      read.settle(answer);
      startEarlyReads();
    }

    // A promise of the answer for bytes 0..end from an early read of the file (read again if that
    // failed), or null without one. One the game asks for before its turn comes is left to the game's
    // own read, which starts at once.
    function takeEarly(url, end) {
      var read = early.get(url);
      if (!read || read.end != end) {
        return null;
      }
      early.delete(url);
      clearTimeout(read.timer);
      if (read.waiting) {
        read.waiting = false;
        return null;
      }
      return read.answer.then((answer) =>
        answer && (succeeded(answer) || answer.status == 416) ? answer : download(url, 0, end));
    }

    // See WARM_AFTER_MS. A file read on this visit already, or being read early, is left out, and so is
    // one the device doesn't keep (readKept answers null): nothing is downloaded from here.
    async function warmUp() {
      var hashes = await fileVersions();
      var bytes = 0;
      for (var [first, rest] of Array.from(readGroups).reverse()) {
        for (var path of [first].concat(rest)) {
          var hash = hashes[path];
          var url = pageFolder + path + `?v=${hash}`;
          if (!hash || readThisVisit.has(path) || early.has(url)) {
            continue;
          }
          while (performance.now() - lastFirstRead < WARM_QUIET_MS) {
            await new Promise((wake) => setTimeout(wake, WARM_QUIET_MS));
          }
          if (readThisVisit.has(path) || early.has(url)) {
            continue; // the game read it meanwhile
          }
          var read = {end: firstReadEnd, waiting: false, answer: readKept(url, 0, firstReadEnd)};
          early.set(url, read);
          var answer = await read.answer;
          if (!answer) {
            forgetEarly(url, read);
            continue;
          }
          bytes += answer.bytes ? answer.bytes.length : 0;
          if (bytes >= WARM_MOST_BYTES) {
            return;
          }
        }
      }
    }

    loadReadGroups();

    // The file's size and its first two chunks (or all of it), in one request.
    async function fetchStart(file, url, chunkSize) {
      var end = 2 * chunkSize - 1;
      // This file's read starts before the early reads of the group it begins (started after it arrived,
      // they made the frame longer: 54 ms against 50, Edge, 2026-10-30).
      var reading = takeEarly(url, end) || download(url, 0, end);
      noteFirstRead(url, end);
      var answer = await reading;
      if (answer.status == 416) {
        // Nothing to give from byte 0 on: an empty file.
        wasmFS$JSMemoryRanges[file] = {size: 0, chunks: [], chunkSize: chunkSize};
        return;
      }
      if (!succeeded(answer)) {
        throw answer;
      }
      var bytes = answer.bytes;
      // A 200 is the whole file (a server that ignores ranges); a 206 says the size after the slash.
      var size = answer.status == 206 && answer.size >= 0 ? answer.size : bytes.length;
      if (size <= bytes.length) {
        wasmFS$JSMemoryRanges[file] = {size: bytes.length, chunks: [bytes], chunkSize: Math.max(bytes.length, 1)};
        return;
      }
      var chunks = [];
      for (var i = 0; (i + 1) * chunkSize <= bytes.length; i++) {
        chunks[i] = bytes.slice(i * chunkSize, (i + 1) * chunkSize);
      }
      wasmFS$JSMemoryRanges[file] = {size: size, chunks: chunks, chunkSize: chunkSize};
    }

    // Makes sure the chunks holding bytes offset .. offset + len - 1 are here.
    async function getFileRange(file, offset, len) {
      var url = await fileUrl(file);
      if (!wasmFS$JSMemoryRanges[file]) {
        await fetchStart(file, url, __wasmfs_fetch_get_chunk_size(file));
      }
      var info = wasmFS$JSMemoryRanges[file];
      len = Math.min(len, info.size - offset);
      if (len <= 0) {
        return;
      }
      var chunkSize = info.chunkSize;
      var firstChunk = (offset / chunkSize) | 0;
      var lastChunk = ((offset + len - 1) / chunkSize) | 0;
      var missing = false;
      for (var i = firstChunk; i <= lastChunk; i++) {
        if (!info.chunks[i]) {
          missing = true;
          break;
        }
      }
      if (!missing) {
        return;
      }
      // One request for all the chunks needed, up to the last byte of the last one.
      var start = firstChunk * chunkSize;
      var end = Math.min((lastChunk + 1) * chunkSize, info.size);
      var answer = await download(url, start, end - 1);
      if (!succeeded(answer)) {
        throw answer;
      }
      var bytes = answer.bytes;
      for (var i = firstChunk; i <= lastChunk; i++) {
        info.chunks[i] = bytes.slice(i * chunkSize - start, (i + 1) * chunkSize - start);
      }
    }

    wasmFS$backends[backend] = {
      // Not asynchronous, but the caller expects a promise.
      allocFile: async (file) => {},
      freeFile: async (file) => {
        wasmFS$JSMemoryRanges[file] = undefined;
      },
      write: async (file, buffer, length, offset) => {
        console.error('The fetch mount is read-only');
      },
      read: async (file, buffer, length, offset) => {
        if (offset < 0 || length <= 0) {
          return 0;
        }
        try {
          await getFileRange(file, offset || 0, length);
        } catch (failedResponse) {
          return failedResponse.status === 404 ? -{{{ cDefs.ENOENT }}} : -{{{ cDefs.EBADF }}};
        }
        var info = wasmFS$JSMemoryRanges[file];
        length = Math.min(length, info.size - offset);
        if (length <= 0) {
          return 0;
        }
        var chunkSize = info.chunkSize;
        var firstChunk = (offset / chunkSize) | 0;
        var lastChunk = ((offset + length - 1) / chunkSize) | 0;
        var readLength = 0;
        for (var i = firstChunk; i <= lastChunk; i++) {
          var chunkStart = i * chunkSize;
          var start = Math.max(chunkStart, offset);
          var end = Math.min(chunkStart + chunkSize, offset + length);
          HEAPU8.set(info.chunks[i].subarray(start - chunkStart, end - chunkStart), buffer + (start - offset));
          readLength = end - offset;
        }
        return readLength;
      },
      getSize: async (file) => {
        try {
          await getFileRange(file, 0, 0);
        } catch (failedResponse) {
          return 0;
        }
        return wasmFS$JSMemoryRanges[file].size;
      },
    };
  },
});
