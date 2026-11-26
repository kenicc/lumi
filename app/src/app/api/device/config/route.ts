// Dokument konfiguracji dla urządzenia. 304 gdy urządzenie ma już tę wersję.
import { NextResponse } from "next/server";
import { authDevice, isClaimed } from "@/lib/deviceAuth";
import { admin } from "@/lib/supabase/admin";

export async function GET(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  await admin().from("devices").update({ last_seen: new Date().toISOString() }).eq("id", d.id);
  if (!(await isClaimed(d.id)) || !d.config) return NextResponse.json({ claimed: false }, { status: 409 });
  const have = Number(req.headers.get("x-lumi-version") ?? 0);
  if (have >= d.config_version) return new NextResponse(null, { status: 304 });
  return NextResponse.json(d.config, { headers: { "Cache-Control": "no-store" } });
}
