// Minimalny service worker: wymagany do instalacji PWA. Nie zapisuje stron w pamięci podręcznej,
// bo ustawienia muszą być zawsze aktualne - bez internetu pokazuje krótką informację.
self.addEventListener("install", () => self.skipWaiting());
self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));
// Powiadomienia od serwera (bateria, brak połączenia)
self.addEventListener("push", (e) => {
  let d = { title: "LUMI", body: "", url: "/" };
  try {
    d = { ...d, ...e.data.json() };
  } catch {}
  e.waitUntil(self.registration.showNotification(d.title, { body: d.body, icon: "/icons/icon-192.png", badge: "/icons/icon-192.png", data: { url: d.url } }));
});
self.addEventListener("notificationclick", (e) => {
  e.notification.close();
  const url = (e.notification.data && e.notification.data.url) || "/";
  e.waitUntil(
    self.clients.matchAll({ type: "window" }).then((list) => {
      for (const c of list) if ("focus" in c) return c.navigate(url).then((w) => w && w.focus());
      return self.clients.openWindow(url);
    }),
  );
});
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
