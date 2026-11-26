// Najnowsze wydanie oprogramowania dla wariantu urządzenia. 204 = brak nowszego.
// Podpis sprawdza samo urządzenie (klucz publiczny w firmware) - serwer tylko wskazuje plik.
import { NextResponse } from "next/server";
import { authDevice } from "@/lib/deviceAuth";
import { admin } from "@/lib/supabase/admin";

export async function GET(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  if (!d.variant) return new NextResponse(null, { status: 204 });
  const { data } = await admin()
    .from("firmware_releases")
    .select("build, version, url, size, sha256, signature")
    .eq("variant", d.variant)
    .eq("active", true)
    .gt("build", d.fw_build ?? 0)
    .order("build", { ascending: false })
    .limit(1);
  const r = data?.[0];
  if (!r) return new NextResponse(null, { status: 204 });
  return NextResponse.json({ build: r.build, version: r.version, url: r.url, size: r.size, sha256: r.sha256, sig: r.signature });
}
