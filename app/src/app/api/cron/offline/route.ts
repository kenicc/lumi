// Wywoływane z harmonogramu (vercel.json albo pg_cron): powiadamia rodzinę, gdy LUMI
// nie łączy się od 2 godzin. Raz na przerwę - flaga offline_alerted zeruje się przy następnym raporcie.
import { NextResponse } from "next/server";
import { OFFLINE_AFTER_MS, offlineAlert } from "@/lib/alerts";
import { purgeOldData } from "@/lib/cleanup";
import { refreshPodcasts } from "@/lib/podcast";
import { alertSilentReminders } from "@/lib/silentReminders";
import { notifyDevice } from "@/lib/push";
import { admin } from "@/lib/supabase/admin";

// podcasty: do 10 s na kanał, 6 naraz - zapas na wiele kanałów
export const maxDuration = 60;

export async function GET(req: Request) {
  const secret = process.env.CRON_SECRET;
  if (!secret || req.headers.get("authorization") !== `Bearer ${secret}`)
    return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const db = admin();
  const cutoff = new Date(Date.now() - OFFLINE_AFTER_MS).toISOString();
  const { data } = await db
    .from("devices")
    .select("id, name")
    .eq("offline_alerted", false)
    .gt("config_version", 0) // tylko sparowane i skonfigurowane
    .lt("last_seen", cutoff);
  for (const d of data ?? []) {
    await notifyDevice(d.id, offlineAlert(d.name ?? "LUMI"));
    await db.from("devices").update({ offline_alerted: true }).eq("id", d.id);
  }
  const reminders = await alertSilentReminders();  // przypomnienia, o których LUMI nie dało znać
  await purgeOldData(); // RODO: stare zdarzenia, wiadomości i historia nie leżą w bazie w nieskończoność
  // nowe odcinki podcastów co 6 godzin (harmonogram woła co godzinę)
  const podcasts = new Date().getUTCHours() % 6 === 0 ? await refreshPodcasts() : 0;
  return NextResponse.json({ alerted: (data ?? []).length, reminders, podcasts });
}
