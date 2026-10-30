/*
 * The /play page's service worker (index.html registers it). It keeps the game's files on the device:
 * a URL with "?v=<hash>" names one version of a file (Main.js, Main.wasm, and the files the game
 * downloads one by one, fetch-backend.js; the hashes are in startup.json), so once downloaded it is
 * answered from the browser's storage, and a release that changes the file changes its URL. The
 * server answers those with "Cache-Control: no-cache", and the game asks for byte ranges, which the
 * browser's own cache doesn't keep: before this, a phone downloaded them again at every visit.
 * The packs aren't here: the page keeps them itself (index.html). Everything else goes to the network
 * as it would without this file. With no connection, opening the page shows a short note instead of
 * the browser's error page.
 *
 * index.html?nosw removes it from the browser, with the files it kept.
 */
// The game's file system reads kept files from here itself (fetch-backend.js, readKept): keep the name.
const FILES = "mu-web/files";

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
  if (request.mode === "navigate") {
    event.respondWith(openPage(event));
  } else if (new URL(request.url).searchParams.has("v")) {
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
    if (kept) return kept;
    const response = await fetch(request.url, { credentials: "same-origin" });
    if (response.status === 200 && cache) event.waitUntil(keep(cache, request.url, response.clone()));
    return response;
  }
  if (kept) return byteRange(await kept.blob(), kept.headers, range);
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

// Downloads a whole file: {blob, response (to keep, with the blob as its body)}, or {response}
// where the server's answer isn't the whole file. A background download gives way to the game's.
async function downloadWhole(url, background) {
  const response = await fetch(url, { credentials: "same-origin", priority: background ? "low" : "auto" });
  if (response.status !== 200) return { response };
  const blob = await response.blob();
  return { blob, response: new Response(blob, { status: 200, statusText: response.statusText, headers: response.headers }) };
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
// own bytes, so a range of a long sound kept on the device doesn't read the whole file.
function byteRange(blob, fileHeaders, range) {
  const match = /^bytes=(\d+)-(\d*)$/.exec(range.trim());
  const headers = new Headers(fileHeaders);
  headers.delete("ETag");
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
</style></head><body><div>
<p>No connection to the internet. The game is played online.<br>
Нет подключения к интернету. Игра работает только онлайн.</p>
<button type="button" onclick="location.reload()">Try again · Повторить</button>
</div></body></html>`;
  return new Response(html, { status: 503, headers: { "Content-Type": "text/html; charset=utf-8" } });
}
