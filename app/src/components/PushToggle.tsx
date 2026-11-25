"use client";
import { useEffect, useState } from "react";
import { removePush, savePush } from "@/app/actions";

function keyBytes(b64: string) {
  const s = atob((b64 + "=".repeat((4 - (b64.length % 4)) % 4)).replace(/-/g, "+").replace(/_/g, "/"));
  return Uint8Array.from(s, (c) => c.charCodeAt(0));
}

type State = "loading" | "unsupported" | "ios-install" | "off" | "on" | "denied";

// Włączanie powiadomień o baterii i braku połączenia. Na iPhonie działa tylko w aplikacji
// dodanej do ekranu początkowego (iOS 16.4+).
export function PushToggle({ compact = false }: { compact?: boolean }) {
  const [state, setState] = useState<State>("loading");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    (async () => {
      const ios = /iphone|ipad|ipod/i.test(navigator.userAgent);
      const standalone = window.matchMedia("(display-mode: standalone)").matches;
      if (!("serviceWorker" in navigator) || !("PushManager" in window)) return setState(ios && !standalone ? "ios-install" : "unsupported");
      if (Notification.permission === "denied") return setState("denied");
      const reg = await navigator.serviceWorker.ready;
      setState((await reg.pushManager.getSubscription()) ? "on" : "off");
    })();
  }, []);

  async function enable() {
    setBusy(true);
    try {
      if ((await Notification.requestPermission()) !== "granted") return setState("denied");
      const reg = await navigator.serviceWorker.ready;
      const sub = await reg.pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: keyBytes(process.env.NEXT_PUBLIC_VAPID_PUBLIC_KEY ?? ""),
      });
      const j = sub.toJSON();
      await savePush(j.endpoint!, j.keys!.p256dh, j.keys!.auth);
      setState("on");
    } finally {
      setBusy(false);
    }
  }

  async function disable() {
    setBusy(true);
    const reg = await navigator.serviceWorker.ready;
    const sub = await reg.pushManager.getSubscription();
    if (sub) {
      await removePush(sub.endpoint);
      await sub.unsubscribe();
    }
    setState("off");
    setBusy(false);
  }

  if (state === "loading" || (compact && state !== "off")) return null;
  if (state === "unsupported") return <p className="dim small">Ta przeglądarka nie obsługuje powiadomień.</p>;
  if (state === "ios-install")
    return <p className="dim small">Na iPhonie powiadomienia działają po dodaniu aplikacji do ekranu początkowego (Udostępnij → Do ekranu początkowego).</p>;
  if (state === "denied") return <p className="dim small">Powiadomienia są zablokowane w ustawieniach telefonu dla tej strony.</p>;
  return (
    <div className={compact ? "card row" : undefined}>
      {compact && <span className="grow">Dostawaj powiadomienie, gdy LUMI ma słabą baterię albo traci połączenie.</span>}
      {state === "off" ? (
        <button className="btn primary" onClick={enable} disabled={busy}>Włącz powiadomienia</button>
      ) : (
        <button className="btn" onClick={disable} disabled={busy}>Wyłącz powiadomienia na tym telefonie</button>
      )}
    </div>
  );
}
