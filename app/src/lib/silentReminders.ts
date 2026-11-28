// Przypomnienia bez żadnej wieści od LUMI (urządzenie wyłączone albo bez zasięgu): wywoływane
// z harmonogramu, zawiadamia rodzinę raz na przypomnienie.
import "server-only";
import { silentReminderAlert } from "./alerts";
import { notifyDevice } from "./push";
import { silentReminders, toMinute, type ReminderEvent } from "./reminders";
import { currentSettings } from "./store";
import { admin } from "./supabase/admin";

export async function alertSilentReminders(now = new Date()): Promise<number> {
  const db = admin();
  const { data } = await db.from("devices").select("id, name, settings, last_seen").gt("config_version", 0);
  const since = new Date(now.getTime() - 24 * 3600_000).toISOString();
  let alerted = 0;
  for (const d of (data ?? []) as { id: string; name: string | null; settings: unknown; last_seen: string | null }[]) {
    const reminders = currentSettings(d.settings).reminders;
    if (!reminders.length) continue;
    const { data: ev } = await db.from("reminder_events").select("reminder_id, status, at").eq("device_id", d.id).gte("at", since);
    const silent = silentReminders(reminders, (ev ?? []) as ReminderEvent[], d.last_seen, now);
    if (!silent.length) continue;
    // najpierw wpis (każde przypomnienie zgłaszamy raz, nawet gdy wysyłka się nie uda)
    await db.from("reminder_events").insert(
      silent.map((r) => ({ device_id: d.id, reminder_id: r.id, text: r.text, minute: toMinute(r.time), status: "nodata", at: now.toISOString() })),
    );
    await notifyDevice(d.id, silentReminderAlert(d.name ?? "LUMI", silent.map((r) => ({ text: r.text, time: r.time }))));
    alerted += silent.length;
  }
  return alerted;
}
