import "server-only";
import { RETENTION_DAYS } from "./account";
import { admin } from "./supabase/admin";

// Pliki urządzenia w magazynie (zdjęcia kontaktów, muzyka) - przy usuwaniu LUMI z aplikacji
export async function deleteDeviceFiles(deviceId: string) {
  const st = admin().storage;
  for (const bucket of ["photos", "music"]) {
    const { data } = await st.from(bucket).list(deviceId, { limit: 1000 });
    const paths = (data ?? []).map((f) => `${deviceId}/${f.name}`);
    if (paths.length) await st.from(bucket).remove(paths);
  }
}

// Usuwa stare dane zgodnie z RETENTION_DAYS (wywoływane z harmonogramu)
export async function purgeOldData(now = Date.now()) {
  const db = admin();
  const before = (days: number) => new Date(now - days * 86400_000).toISOString();
  await db.from("reminder_events").delete().lt("at", before(RETENTION_DAYS.reminderEvents));
  await db.from("messages").delete().lt("created_at", before(RETENTION_DAYS.messages));
  await db.from("changes").delete().lt("created_at", before(RETENTION_DAYS.changes));
}
