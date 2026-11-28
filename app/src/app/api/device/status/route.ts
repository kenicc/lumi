// Stan urządzenia widoczny dla rodziny. Bez lokalizacji i bez historii słuchania (dokumentacja v2.0).
import { NextResponse } from "next/server";
import { z } from "zod";
import { backOnlineAlert, batteryAlert, statusAlerts } from "@/lib/alerts";
import { authDevice } from "@/lib/deviceAuth";
import { notifyDevice } from "@/lib/push";
import { admin } from "@/lib/supabase/admin";

const Body = z.object({
  version: z.number().int().min(0),
  variant: z.string().max(16).optional(),
  fw: z.string().max(32).optional(),
  battery: z.number().min(0).max(100).optional(),
  batteryMv: z.number().optional(),
  charging: z.boolean().optional(),
  signal: z.number().int().min(0).max(4).optional(),
  registered: z.boolean().optional(),
  lte: z.boolean().optional(),
  operator: z.string().max(24).optional(),
  modem: z.string().max(24).optional(),
  sim: z.boolean().optional(),
  wifiRssi: z.number().optional(),
  uptime: z.number().optional(),
  build: z.number().int().min(0).optional(),
  reset: z.string().max(24).optional(),
  boots: z.number().int().min(0).optional(),
  errors: z.array(z.string().max(120)).max(8).optional(),
});

export async function POST(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const parsed = Body.safeParse(await req.json().catch(() => null));
  if (!parsed.success) return NextResponse.json({ error: "bad request" }, { status: 400 });
  const s = parsed.data;
  // alarmy liczymy względem poprzedniego stanu, zanim zostanie nadpisany
  const name = d.name ?? "LUMI";
  const alerts = statusAlerts(d.status, s, name);
  if (d.offline_alerted) alerts.push(backOnlineAlert(name));
  await admin()
    .from("devices")
    .update({
      status: { ...s, alertLevel: batteryAlert(d.status, s).level },
      applied_version: Math.min(s.version, d.config_version),
      fw: s.fw ?? d.fw,
      fw_build: s.build ?? d.fw_build,
      variant: s.variant ?? d.variant,
      last_seen: new Date().toISOString(),
      offline_alerted: false,
    })
    .eq("id", d.id);
  for (const a of alerts) await notifyDevice(d.id, a);
  return NextResponse.json({ ok: true });
}
