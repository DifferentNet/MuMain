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
 * read from there directly (readKept). Without startup.json or a hash, the plain URL.
 *
 * The downloads run in the mount's worker, except where the page decides (index.html,
 * MuWeb_FilesOnPageThread in WebFileSystem.cpp) that the page's thread runs them: WebKit (Safari,
 * every iPhone browser) starts a worker's download on the page's thread, and the game, which runs
 * there, holds it while it waits for this worker, so the download never started (Safari 27,
 * 2026-10-29). While it waits, the page's thread runs work other threads send it (Emscripten's
 * emscripten_futex_wait.c), and a synchronous request needs no event loop: the worker sends it
 * downloadOnPageThread.
 */
addToLibrary({
  // On the page's thread: bytes start..end of the file at `url` (a C string) go to `buffer` when they
  // fit in `capacity`; the answer's status and the file's size (from Content-Range, -1 without) go to
  // `info` (two doubles). Returns how many bytes the answer has, -1 when none came.
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
    {{{ makeSetValue('info', 0, 'request.status', 'double') }}};
    {{{ makeSetValue('info', 8, 'total ? parseInt(total[1], 10) : -1', 'double') }}};
    if (text.length <= capacity) {
      for (var i = 0; i < text.length; i++) {
        HEAPU8[buffer + i] = text.charCodeAt(i) & 0xff;
      }
    }
    return text.length;
  },

  _wasmfs_create_fetch_backend_js__deps: [
    '$wasmFS$backends',
    '$wasmFS$JSMemoryRanges',
    '$UTF8ToString',
    '$stringToNewUTF8',
    '$downloadOnPageThread',
    'malloc',
    'free',
    'MuWeb_FilesOnPageThread',
    '_wasmfs_fetch_get_file_url',
    '_wasmfs_fetch_get_chunk_size',
  ],
  _wasmfs_create_fetch_backend_js: async function(backend) {
    // This runs in a worker, whose location is Main.js's: the page's folder.
    var pageFolder = new URL('.', self.location.href).href;
    var versions = null;
    function fileVersions() {
      if (!versions) {
        versions = fetch(new URL('startup.json', pageFolder), {cache: 'no-cache'})
          .then((response) => response.ok ? response.json() : {})
          .then((index) => index.versions || {}, () => ({}));
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

    // See downloadOnPageThread: {status, size, bytes}, or {status: 0} without an answer.
    function askPageThread(url, start, end) {
      var urlText = stringToNewUTF8(url);
      var info = _malloc(16);
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
    // bytes (where the status is a success)}.
    async function download(url, start, end) {
      if (_MuWeb_FilesOnPageThread()) {
        return askPageThread(url, start, end);
      }
      var kept = await readKept(url, start, end);
      if (kept) {
        return kept;
      }
      var response = await fetch(url, {headers: {'Range': `bytes=${start}-${end}`}});
      var total = /\/(\d+)$/.exec(response.headers.get('Content-Range') || '');
      return {
        status: response.status,
        size: total ? parseInt(total[1], 10) : -1,
        bytes: response.ok ? new Uint8Array(await response.arrayBuffer()) : null,
      };
    }

    function succeeded(answer) {
      return answer.status >= 200 && answer.status < 300;
    }

    // The file's size and its first two chunks (or all of it), in one request.
    async function fetchStart(file, url, chunkSize) {
      var answer = await download(url, 0, 2 * chunkSize - 1);
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
