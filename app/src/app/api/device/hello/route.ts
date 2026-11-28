// Rejestracja urządzenia przy pierwszym połączeniu (zaufanie przy pierwszym użyciu).
// Produkcja: sekret powinien być wgrywany fabrycznie, a serwer znać go z góry.
import { NextResponse } from "next/server";
import { z } from "zod";
import { ID_RE, isClaimed, safeEqualHex, sha256 } from "@/lib/deviceAuth";
import { admin } from "@/lib/supabase/admin";

const Body = z.object({
  id: z.string().regex(ID_RE),
  secret: z.string().regex(/^[0-9a-f]{64}$/),
  pair: z.string().regex(/^[A-Z2-9]{8}$/),
  variant: z.string().max(16).optional(),
  fw: z.string().max(32).optional(),
  build: z.number().int().min(0).optional(),
});

export async function POST(req: Request) {
  const parsed = Body.safeParse(await req.json().catch(() => null));
  if (!parsed.success) return NextResponse.json({ error: "bad request" }, { status: 400 });
  const { id, secret, pair, variant, fw, build } = parsed.data;
  const db = admin();
  const { data: existing } = await db.from("devices").select("id, secret_hash").eq("id", id).maybeSingle();
  const now = new Date().toISOString();
  // wersja ustawień, którą LUMI ma na dysku: po ponownym parowaniu numeracja idzie dalej od niej,
  // inaczej urządzenie uznałoby nowe ustawienia (od wersji 1) za starsze od swoich
  const has = Math.max(0, Math.min(2_000_000_000, Math.trunc(Number(req.headers.get("x-lumi-version")) || 0)));
  if (!existing) {
    const { error } = await db.from("devices").insert({ id, secret_hash: sha256(secret), pair_hash: sha256(pair), variant, fw, fw_build: build ?? 0, last_seen: now, applied_version: has });
    if (error) return NextResponse.json({ error: "db" }, { status: 500 });
    return NextResponse.json({ claimed: false });
  }
  if (!safeEqualHex((existing as { secret_hash: string }).secret_hash, sha256(secret)))
    return NextResponse.json({ error: "secret mismatch" }, { status: 403 });
  const claimed = await isClaimed(id);
  await db
    .from("devices")
    .update({ variant, fw, fw_build: build ?? 0, last_seen: now, ...(claimed ? {} : { pair_hash: sha256(pair), applied_version: has }) })
    .eq("id", id);
  return NextResponse.json({ claimed });
}
