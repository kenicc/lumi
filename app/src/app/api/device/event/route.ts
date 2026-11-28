// Zdarzenia z urządzenia. Na razie: przypomnienia (potwierdzone / odłożone / bez potwierdzenia).
import { NextResponse } from "next/server";
import { z } from "zod";
import { authDevice } from "@/lib/deviceAuth";
import { notifyDevice } from "@/lib/push";
import { currentSettings } from "@/lib/store";
import { admin } from "@/lib/supabase/admin";

const Reminder = z.object({
  type: z.literal("reminder"),
  id: z.string().min(1).max(40),
  text: z.string().max(60),
  minute: z.number().int().min(0).max(1439),
  status: z.enum(["done", "snoozed", "missed", "shown"]),
  at: z.number().int().optional(), // czas z urządzenia (zdarzenie mogło czekać w kolejce bez internetu)
});
const MessageRead = z.object({
  type: z.literal("message"),
  id: z.number().int().positive(),
  status: z.literal("read"),
  at: z.number().int().optional(),
});
const Body = z.discriminatedUnion("type", [Reminder, MessageRead]);

// czas z urządzenia tylko gdy wiarygodny (nie z przyszłości, nie sprzed tygodnia)
function eventTime(at?: number) {
  const nowS = Date.now() / 1000;
  return at && at <= nowS + 300 && at > nowS - 7 * 86400 ? new Date(at * 1000) : new Date();
}

export async function POST(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const parsed = Body.safeParse(await req.json().catch(() => null));
  if (!parsed.success) return NextResponse.json({ error: "bad request" }, { status: 400 });
  const e = parsed.data;
  const at = eventTime(e.at);
  if (e.type === "message") {
    // tylko wiadomości tego urządzenia; pierwsze potwierdzenie wygrywa
    await admin().from("messages").update({ read_at: at.toISOString() }).eq("id", e.id).eq("device_id", d.id).is("read_at", null);
    return NextResponse.json({ ok: true });
  }
  // LUMI ponawia zdarzenie, dopóki nie dostanie odpowiedzi. Gdy odpowiedź zginęła po zapisie, ten sam
  // wpis (ten sam czas z urządzenia co do sekundy) przyjdzie drugi raz - bez drugiego zapisu i powiadomienia.
  const row = { device_id: d.id, reminder_id: e.id, status: e.status, at: at.toISOString() };
  const { data: dup } = await admin()
    .from("reminder_events")
    .select("id")
    .eq("device_id", row.device_id)
    .eq("reminder_id", row.reminder_id)
    .eq("status", row.status)
    .eq("at", row.at)
    .limit(1);
  if (dup?.length) return NextResponse.json({ ok: true, duplicate: true });
  await admin().from("reminder_events").insert({ ...row, text: e.text, minute: e.minute });

  if (e.status === "missed") {
    const r = currentSettings(d.settings).reminders.find((x) => x.id === e.id);
    if (r?.confirm !== false) {
      const hm = `${Math.floor(e.minute / 60)}:${String(e.minute % 60).padStart(2, "0")}`;
      await notifyDevice(d.id, {
        title: `${d.name ?? "LUMI"}: przypomnienie bez potwierdzenia`,
        body: `„${e.text}” (${hm}) - nikt nie nacisnął „Zrobione”. Może warto zadzwonić.`,
      });
    }
  }
  return NextResponse.json({ ok: true });
}
