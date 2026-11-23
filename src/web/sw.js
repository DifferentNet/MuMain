/*
 * The /play page's service worker (index.html registers it). It keeps the game's files on the device:
 * a URL with "?v=<hash>" names one version of a file (Main.js, Main.wasm, and the files the game
 * downloads one by one, fetch-backend.js; the hashes are in startup.json), so once downloaded it is
 * answered from the browser's storage, and a release that changes the file changes its URL. The
 * server answers those with "Cache-Control: no-cache", and the game asks for byte ranges, which the
 * browser's own cache doesn't keep: before this, a phone downloaded them again at every visit.
 * The packs aren't here: the page keeps them itself (index.html). Main.wasm is also kept unpacked
 * (unpackedCode below), so the browser can keep its compiled code. Everything else goes to the network
 * as it would without this file. With no connection, opening the page shows a short note instead of
 * the browser's error page.
 *
 * index.html?nosw removes it from the browser, with the files it kept.
 */
// The game's file system reads kept files from here itself (fetch-backend.js, readKept): keep the name.
const FILES = "mu-web/files";
// Main.wasm unpacked, the current release's only (unpackedCode).
const CODE = "mu-web/code";
// What this worker does that an older one didn't: the page asks before relying on it (a release's first
// visit is still answered by the worker before it).
const FEATURES = { unpackedCode: true };

self.addEventListener("install", () => self.skipWaiting());

self.addEventListener("activate", (event) => {
  event.waitUntil((async () => {
    // The page's own request starts while this worker wakes up, instead of after it.
    if (self.registration.navigationPreload) await self.registration.navigationPreload.enable().catch(() => {});
    await self.clients.claim();
  })());
});

self.addEventListener("fetch", (event) => {
  const request = event.request;
  if (request.method !== "GET" || new URL(request.url).origin !== self.location.origin) return;
  const url = new URL(request.url);
  if (request.mode === "navigate") {
    event.respondWith(openPage(event));
  } else if (url.pathname.endsWith("/sw-features")) {
    event.respondWith(new Response(JSON.stringify(FEATURES), { headers: { "Content-Type": "application/json" } }));
  } else if (url.pathname.endsWith("/Main.wasm") && url.searchParams.has("unpacked")) {
    event.respondWith(unpackedCode(event, url));
  } else if (url.searchParams.has("v")) {
    event.respondWith(versionedFile(event));
  }
});

async function openPage(event) {
  try {
    return (await event.preloadResponse) || (await fetch(event.request));
  } catch (e) {
    return offlinePage();
  }
}

// Files downloading now, by URL. The game reads a file range after range (fetch-backend.js): the
// first range downloads the whole file, and the next ones wait for it instead of downloading it
// again; so do the page's background downloads (index.html, "X-Mu-Background"). A file stays here
// until it is kept, so a range asked for in between doesn't download it once more.
const downloads = new Map();

// The file from the device, else from the server (the whole file, which is then kept). The game's
// file system asks for byte ranges: they are cut from the whole file here.
async function versionedFile(event) {
  const request = event.request;
  const range = request.headers.get("Range");
  const cache = await caches.open(FILES).catch(() => null);
  const kept = cache ? await cache.match(request.url).catch(() => undefined) : undefined;
  if (!range) {
    // A whole file from the device is passed on as kept, without X-Mu-Kept (byteRange): a response of
    // our own would lose the code the browser keeps compiled for a kept script.
    if (kept) return kept;
    if (request.destination === "worker") return threadScript(event, cache);
    const response = await fetch(request.url, { credentials: "same-origin" });
    if (response.status === 200 && cache) event.waitUntil(keep(cache, request.url, response.clone()));
    return response;
  }
  if (kept) return byteRange(await kept.blob(), kept.headers, range, true);
  let download = downloads.get(request.url);
  if (!download) {
    download = downloadWhole(request.url, request.headers.has("X-Mu-Background"));
    downloads.set(request.url, download);
    // The game gets its range as soon as the file is here; keeping it on the device takes a while
    // longer (11 ms a file, 2026-10-30), and the game waited for that too.
    event.waitUntil(download.then((whole) => whole.blob && cache && keep(cache, request.url, whole.response))
      .catch(() => {}).finally(() => downloads.delete(request.url)));
  }
  const whole = await download;
  // An error, or an answer that isn't the whole file: passed on as the server gave it.
  return whole.blob ? byteRange(whole.blob, whole.response.headers, range) : whole.response.clone();
}

// The game's threads load Main.js?v= as workers (request.destination "worker"), and nothing asks for a
// refused one again, so the game wouldn't start: a busy server's answer (HTTP 429 or 503) for one is
// asked again here by the page's rule (index.html fetchWhenFree: after its Retry-After, and at least
// 1 s doubling up to 8 s, for up to 30 s). The page's own requests are left to the page and to
// fetch-backend.js, so nothing is asked again twice over. The threads start together: they share one
// asking (threadScripts) instead of each asking the busy server on its own.
const threadScripts = new Map();
function threadScript(event, cache) {
  const url = event.request.url;
  let asking = threadScripts.get(url);
  if (!asking) {
    asking = fetchWhenFree(url, { credentials: "same-origin" });
    threadScripts.set(url, asking);
    event.waitUntil(asking.then((response) => response.status === 200 && cache && keep(cache, url, response.clone()))
      .catch(() => {}).finally(() => threadScripts.delete(url)));
  }
  // Each thread gets a copy; the first answer itself is never read, so a copy can be made of it later.
  return asking.then((response) => response.clone());
}

async function fetchWhenFree(url, init) {
  const started = Date.now();
  for (let tries = 0; ; tries++) {
    const response = await fetch(url, init);
    if (response.status !== 429 && response.status !== 503) return response;
    const retryAfter = response.headers.get("Retry-After");
    const seconds = retryAfter && isFinite(retryAfter) ? Number(retryAfter) : 0;
    const wait = Math.max(seconds * 1000, Math.min(1000 * 2 ** tries, 8000)) * (1 + Math.random() / 4);
    if (Date.now() - started + wait > 30000) return response;
    if (response.body) response.body.cancel().catch(() => {});
    await new Promise((resume) => setTimeout(resume, wait));
  }
}

// Downloads a whole file: {blob, response (to keep, with the blob as its body)}, or {response}
// where the server's answer isn't the whole file. A background download gives way to the game's.
async function downloadWhole(url, background) {
  const response = await fetch(url, { credentials: "same-origin", priority: background ? "low" : "auto" });
  if (response.status !== 200) return { response };
  const blob = await response.blob();
  return { blob, response: new Response(blob, { status: 200, statusText: response.statusText, headers: response.headers }) };
}

// The game's code, Main.wasm?v=<hash>&unpacked: unpacked and kept whole on the device. It comes
// packed (Main.wasm.gz), and the page used to unpack it into a response of its own, whose compiled
// code the browser never keeps: every start compiled the 21 MB of code again, seconds of a phone's
// processor. A response from this store is one the browser keeps the compiled code of (Chrome, Edge),
// and the next start skips compiling. The first time it is unpacked here as it downloads, the page
// compiling it meanwhile. "&keep" only fills the store and answers 204 (the page's first visit, before
// this worker answered it). The earlier release's copy is dropped once this one is kept.
async function unpackedCode(event, url) {
  const version = url.searchParams.get("v") || "";
  const key = new URL(`Main.wasm?v=${version}&unpacked`, url).href;
  const fillOnly = url.searchParams.has("keep");
  const cache = await caches.open(CODE).catch(() => null);
  const kept = cache ? await cache.match(key).catch(() => undefined) : undefined;
  // The kept code itself is passed on as kept (see versionedFile); the 204 says it came from the device.
  if (kept) return fillOnly ? new Response(null, { status: 204, headers: { "X-Mu-Kept": "1" } }) : kept;
  let body = null;
  if (self.DecompressionStream) {
    const packed = await fetch(new URL(`Main.wasm.gz?v=${version}`, url), { credentials: "same-origin" }).catch(() => null);
    // A busy server (HTTP 429 or 503) is passed on, for the page to ask again (index.html fetchWhenFree):
    // Main.wasm itself would be refused too, and is four times as big.
    if (packed && (packed.status === 429 || packed.status === 503)) return packed;
    if (packed && packed.ok) {
      // A server that marks the copy as gzip has the browser unpack it already.
      body = /gzip/i.test(packed.headers.get("Content-Encoding") || "")
        ? packed.body : packed.body.pipeThrough(new DecompressionStream("gzip"));
    }
  }
  if (!body) {
    const plain = await fetch(new URL(`Main.wasm?v=${version}`, url), { credentials: "same-origin" });
    if (!plain.ok) return plain;
    body = plain.body;
  }
  const headers = { "Content-Type": "application/wasm" };
  if (!cache) return fillOnly ? new Response(null, { status: 204 }) : new Response(body, { headers });
  let forPage = null;
  if (!fillOnly) [body, forPage] = body.tee();
  const keeping = cache.put(key, new Response(body, { headers })).then(async () => {
    for (const request of await cache.keys()) {
      if (request.url !== key) await cache.delete(request);
    }
  }).catch(() => {});   // the device is full: unpacked by the page next time
  if (fillOnly) {
    await keeping;
    return new Response(null, { status: 204 });
  }
  event.waitUntil(keeping);
  return new Response(forPage, { headers });
}

// Keeps a file. The page drops its earlier versions, once a release (index.html, dropOldVersions):
// looking for them here read through the whole store after every file (1.5 s with 3,359 files kept),
// and the game's reads waited behind that, 25-40 s on a window's first open (2026-10-30).
async function keep(cache, url, response) {
  try {
    await cache.put(url, response);
  } catch (e) {
    // The device is full or keeps nothing for this site: the file comes from the server next time.
  }
}

// "Range: bytes=a-b" (or "bytes=a-") as the web server answers it (server/web/serve-web.py),
// keeping the file's headers, cross-origin isolation among them. A slice of a Blob reads only its
// own bytes, so a range of a long sound kept on the device doesn't read the whole file. `fromDevice`
// adds "X-Mu-Kept: 1": the server wasn't asked, so the game's file system doesn't take the answer
// as a sign that a busy server answers again (fetch-backend.js, heard).
function byteRange(blob, fileHeaders, range, fromDevice) {
  const match = /^bytes=(\d+)-(\d*)$/.exec(range.trim());
  const headers = new Headers(fileHeaders);
  headers.delete("ETag");
  if (fromDevice) headers.set("X-Mu-Kept", "1");
  if (!match) return new Response(blob, { status: 200, headers });
  const size = blob.size;
  const start = Number(match[1]);
  if (start >= size) {
    headers.set("Content-Range", `bytes */${size}`);
    headers.set("Content-Length", "0");
    return new Response(null, { status: 416, statusText: "Range Not Satisfiable", headers });
  }
  const end = match[2] ? Math.min(Number(match[2]), size - 1) : size - 1;
  headers.set("Content-Range", `bytes ${start}-${end}/${size}`);
  headers.set("Content-Length", String(end - start + 1));
  return new Response(blob.slice(start, end + 1), { status: 206, statusText: "Partial Content", headers });
}

// The note in the page's language, picked as index.html picks it (pageLang: ?lang=, the one the page
// kept, the browser's languages), by its own few lines: English where they don't run.
function offlinePage() {
  const html = `<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MU Skynet</title>
<style>
  html, body { height: 100%; margin: 0; }
  body { display: flex; align-items: center; justify-content: center; padding: 1rem; box-sizing: border-box;
         background: #0d0e12; color: #f2eee6; font: 16px/1.5 "Segoe UI", system-ui, sans-serif; text-align: center; }
  button { margin-top: 1rem; font: inherit; font-weight: 600; color: #1b1406; background: #e5b95c; border: 0;
           border-radius: 6px; padding: .5rem 1.75rem; cursor: pointer; }
</style></head><body>
<div lang="en"><p>No connection to the internet. The game is played online.</p>
<button type="button" onclick="location.reload()">Try again</button></div>
<div lang="ru" hidden><p>Нет подключения к интернету. Игра работает только онлайн.</p>
<button type="button" onclick="location.reload()">Повторить</button></div>
<div lang="ro" hidden><p>Nu ai conexiune la internet. Jocul merge doar online.</p>
<button type="button" onclick="location.reload()">Încearcă din nou</button></div>
<div lang="pt" hidden><p>Sem conexão com a internet. O jogo é jogado online.</p>
<button type="button" onclick="location.reload()">Tentar de novo</button></div>
<div lang="es" hidden><p>No hay conexión a internet. El juego se juega en línea.</p>
<button type="button" onclick="location.reload()">Intentar de nuevo</button></div>
<script>
(function () {
  var langs = ["en", "ru", "ro", "pt", "es"], key = "mu-page-lang", lang = new URLSearchParams(location.search).get("lang");
  if (langs.indexOf(lang) >= 0) {
    try { localStorage.setItem(key, lang); } catch (e) {}
  } else {
    lang = null;
    try { lang = localStorage.getItem(key); } catch (e) {}
    if (langs.indexOf(lang) < 0) {
      lang = "en";
      var tags = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language];
      for (var i = 0; i < tags.length; i++) {
        var primary = String(tags[i] || "").toLowerCase().split(/[-_;]/)[0];
        if (primary === "mo") primary = "ro";
        if (langs.indexOf(primary) >= 0) { lang = primary; break; }
      }
    }
  }
  document.documentElement.lang = lang === "pt" ? "pt-BR" : lang === "es" ? "es-419" : lang;
  var notes = document.querySelectorAll("body > div[lang]");
  for (var j = 0; j < notes.length; j++) notes[j].hidden = notes[j].lang !== lang;
})();
</script>
</body></html>`;
  return new Response(html, { status: 503, headers: { "Content-Type": "text/html; charset=utf-8" } });
}
