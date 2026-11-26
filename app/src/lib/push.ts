import "server-only";
import webpush from "web-push";
import type { Alert } from "./alerts";
import { admin } from "./supabase/admin";

let ready = false;
function setup() {
  if (ready) return true;
  const pub = process.env.NEXT_PUBLIC_VAPID_PUBLIC_KEY, priv = process.env.VAPID_PRIVATE_KEY;
  if (!pub || !priv) return false;
  webpush.setVapidDetails(process.env.VAPID_SUBJECT ?? "mailto:lumi@example.invalid", pub, priv);
  return (ready = true);
}

// Wysyła powiadomienie do wszystkich osób z dostępem do urządzenia. Wygasłe subskrypcje usuwa.
export async function notifyDevice(deviceId: string, alert: Alert) {
  if (!setup()) return 0;
  const db = admin();
  const { data: mem } = await db.from("members").select("user_id").eq("device_id", deviceId);
  const users = (mem ?? []).map((m) => m.user_id);
  if (!users.length) return 0;
  const { data: subs } = await db.from("push_subscriptions").select("endpoint, p256dh, auth").in("user_id", users);
  const payload = JSON.stringify({ ...alert, url: `/d/${deviceId}` });
  let sent = 0;
  await Promise.all(
    (subs ?? []).map(async (s) => {
      try {
        await webpush.sendNotification({ endpoint: s.endpoint, keys: { p256dh: s.p256dh, auth: s.auth } }, payload, { TTL: 6 * 3600 });
        sent++;
      } catch (e) {
        const code = (e as { statusCode?: number }).statusCode;
        if (code === 404 || code === 410) await db.from("push_subscriptions").delete().eq("endpoint", s.endpoint);
      }
    }),
  );
  return sent;
}
