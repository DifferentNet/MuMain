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
 * the server again only when a release changes the file, and so its hash. Without startup.json or a
 * hash, the plain URL.
 */
addToLibrary({
  _wasmfs_create_fetch_backend_js__deps: [
    '$wasmFS$backends',
    '$wasmFS$JSMemoryRanges',
    '$UTF8ToString',
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

    // The file's size and its first two chunks (or all of it), in one request.
    async function fetchStart(file, url, chunkSize) {
      var response = await fetch(url, {headers: {'Range': `bytes=0-${2 * chunkSize - 1}`}});
      if (response.status == 416) {
        // Nothing to give from byte 0 on: an empty file.
        wasmFS$JSMemoryRanges[file] = {size: 0, chunks: [], chunkSize: chunkSize};
        return;
      }
      if (!response.ok) {
        throw response;
      }
      var bytes = new Uint8Array(await response.arrayBuffer());
      // A 200 is the whole file (a server that ignores ranges); a 206 says the size after the slash.
      var total = /\/(\d+)$/.exec(response.headers.get('Content-Range') || '');
      var size = response.status == 206 && total ? parseInt(total[1], 10) : bytes.length;
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
      var response = await fetch(url, {headers: {'Range': `bytes=${start}-${end - 1}`}});
      if (!response.ok) {
        throw response;
      }
      var bytes = new Uint8Array(await response.arrayBuffer());
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
