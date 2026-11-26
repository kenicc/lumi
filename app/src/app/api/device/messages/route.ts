// Nieprzeczytane wiadomości od rodziny dla urządzenia (najstarsze najpierw)
import { NextResponse } from "next/server";
import { authDevice } from "@/lib/deviceAuth";
import { admin } from "@/lib/supabase/admin";

export async function GET(req: Request) {
  const d = await authDevice(req);
  if (!d) return NextResponse.json({ error: "unauthorized" }, { status: 401 });
  const { data } = await admin()
    .from("messages")
    .select("id, author, text, created_at")
    .eq("device_id", d.id)
    .is("read_at", null)
    .order("id", { ascending: true })
    .limit(10);
  return NextResponse.json(
    (data ?? []).map((m) => ({ id: m.id, author: m.author, text: m.text, at: Math.floor(new Date(m.created_at).getTime() / 1000) })),
    { headers: { "Cache-Control": "no-store" } },
  );
}
