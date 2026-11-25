// Zdarzenia z urządzenia. Na razie: przypomnienia (potwierdzone / odłożone / bez potwierdzenia).
import { NextResponse } from "next/server";
import { z } from "zod";
import { authDevice } from "@/lib/deviceAuth";
import { notifyDevice } from "@/lib/push";
import { currentSettings } from "@/lib/store";
import { admin } from "@/lib/supabase/admin";

const Body = z.object({
  type: z.literal("reminder"),
  id: z.string().min(1).max(40),
  text: z.string().max(60),
  minute: z.number().int().min(0).max(1439),
  status: z.enum(["done", "snoozed", "missed", "shown"]),
  at: z.number().int().optional(), // czas z urządzenia (zdarzenie mogło czekać w kolejce bez internetu)
});

export async function POST(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const parsed = Body.safeParse(await req.json().catch(() => null));
  if (!parsed.success) return NextResponse.json({ error: "bad request" }, { status: 400 });
  const e = parsed.data;
  const nowS = Date.now() / 1000;
  // czas z urządzenia tylko gdy wiarygodny (nie z przyszłości, nie sprzed tygodnia)
  const at = e.at && e.at <= nowS + 300 && e.at > nowS - 7 * 86400 ? new Date(e.at * 1000) : new Date();
  await admin().from("reminder_events").insert({ device_id: d.id, reminder_id: e.id, text: e.text, minute: e.minute, status: e.status, at: at.toISOString() });

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
