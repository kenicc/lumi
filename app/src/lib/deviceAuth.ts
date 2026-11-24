import "server-only";
import { createHash, timingSafeEqual } from "node:crypto";
import { admin } from "./supabase/admin";
import type { DeviceRow } from "./types";

export const sha256 = (s: string) => createHash("sha256").update(s).digest("hex");

export function safeEqualHex(a: string, b: string) {
  const x = Buffer.from(a, "hex");
  const y = Buffer.from(b, "hex");
  return x.length === y.length && timingSafeEqual(x, y);
}

export const ID_RE = /^LUMI-[0-9A-F]{6}$/;

// Urządzenie podpisuje zapytania sekretem z NVS: Authorization: Bearer <sekret>, X-Lumi-Device: <id>
export async function authDevice(req: Request): Promise<DeviceRow | null> {
  const id = req.headers.get("x-lumi-device") ?? "";
  const auth = req.headers.get("authorization") ?? "";
  const secret = auth.startsWith("Bearer ") ? auth.slice(7) : "";
  if (!ID_RE.test(id) || !/^[0-9a-f]{64}$/.test(secret)) return null;
  const { data } = await admin().from("devices").select("*").eq("id", id).maybeSingle();
  const d = data as DeviceRow | null;
  if (!d || !safeEqualHex(d.secret_hash, sha256(secret))) return null;
  return d;
}

export async function isClaimed(deviceId: string) {
  const { count } = await admin()
    .from("members")
    .select("user_id", { count: "exact", head: true })
    .eq("device_id", deviceId)
    .eq("role", "guardian");
  return (count ?? 0) > 0;
}
