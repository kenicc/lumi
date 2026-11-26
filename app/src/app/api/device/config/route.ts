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
  // X-Lumi-Inbox: id najnowszej nieprzeczytanej wiadomości (0 = brak). Urządzenie pyta o wiadomości
  // tylko, gdy ten numer jest większy niż ostatnio widziany - zwykle jedno zapytanie na cykl zamiast dwóch.
  const { data: top } = await admin()
    .from("messages")
    .select("id")
    .eq("device_id", d.id)
    .is("read_at", null)
    .order("id", { ascending: false })
    .limit(1)
    .maybeSingle();
  const headers = { "Cache-Control": "no-store", "X-Lumi-Inbox": String(top?.id ?? 0) };
  if (have >= d.config_version) return new NextResponse(null, { status: 304, headers });
  return NextResponse.json(d.config, { headers });
}
