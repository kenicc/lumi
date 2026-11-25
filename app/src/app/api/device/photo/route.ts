// Zdjęcie kontaktu dla urządzenia: 96x96 RGB565, tylko dla zdjęć z jego własnych kontaktów
import { NextResponse } from "next/server";
import { authDevice } from "@/lib/deviceAuth";
import { currentSettings } from "@/lib/store";
import { admin } from "@/lib/supabase/admin";

export async function GET(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const h = new URL(req.url).searchParams.get("h") ?? "";
  if (!/^[0-9a-f]{16}$/.test(h) || !currentSettings(d.settings).contacts.some((c) => c.photo === h))
    return NextResponse.json({ error: "not found" }, { status: 404 });
  const { data } = await admin().storage.from("photos").download(`${d.id}/${h}.565`);
  if (!data) return NextResponse.json({ error: "not found" }, { status: 404 });
  return new NextResponse(Buffer.from(await data.arrayBuffer()), {
    headers: { "Content-Type": "application/octet-stream", "Cache-Control": "private, max-age=31536000, immutable" },
  });
}
