// Minimalny service worker: wymagany do instalacji PWA. Nie zapisuje stron w pamięci podręcznej,
// bo ustawienia muszą być zawsze aktualne - bez internetu pokazuje krótką informację.
self.addEventListener("install", () => self.skipWaiting());
self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));
self.addEventListener("fetch", (e) => {
  if (e.request.mode !== "navigate") return;
  e.respondWith(
    fetch(e.request).catch(
      () =>
        new Response(
          '<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width"><body style="font:20px system-ui;padding:24px">Brak internetu. LUMI działa dalej na ostatnich ustawieniach - spróbuj, gdy telefon połączy się z siecią.</body>',
          { headers: { "Content-Type": "text/html; charset=utf-8" } },
        ),
    ),
  );
});
