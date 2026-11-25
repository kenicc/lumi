import "server-only";
import { redirect } from "next/navigation";
import { DEMO, DEMO_USER } from "./demo";
import { admin } from "./supabase/admin";
import { supabaseServer } from "./supabase/server";
import type { DeviceRow, Role } from "./types";

// Kto może zmieniać którą część ustawień (dokumentacja v2.0, "Role")
export const CAN = {
  contacts: ["guardian", "family"],
  home: ["guardian", "family"],
  media: ["guardian", "family", "friend"],
  reminders: ["guardian", "family"],
  messages: ["guardian", "family", "friend"],
  propose: ["friend"], // znajomi proponują kontakty; rodzina dodaje je od razu
  look: ["guardian", "family"],
  weather: ["guardian", "family"],
  wifi: ["guardian", "family"],
  access: ["guardian"],
  people: ["guardian"],
} as const satisfies Record<string, readonly Role[]>;
export type Section = keyof typeof CAN;

export function can(role: Role, section: Section) {
  return (CAN[section] as readonly Role[]).includes(role);
}

export async function currentUser() {
  if (DEMO) return DEMO_USER as unknown as Awaited<ReturnType<Awaited<ReturnType<typeof supabaseServer>>["auth"]["getUser"]>>["data"]["user"];
  const sb = await supabaseServer();
  const { data } = await sb.auth.getUser();
  return data.user;
}

export async function requireUser(next = "/") {
  const user = await currentUser();
  if (!user) redirect(`/login?next=${encodeURIComponent(next)}`);
  return user;
}

export async function requireMember(deviceId: string, section?: Section) {
  const user = await requireUser(`/d/${deviceId}`);
  const db = admin();
  const { data: m } = await db.from("members").select("role").eq("device_id", deviceId).eq("user_id", user.id).maybeSingle();
  if (!m) redirect("/");
  const role = (m as { role: Role }).role;
  if (section && !can(role, section)) redirect(`/d/${deviceId}?err=${encodeURIComponent("Brak uprawnień do tej części")}`);
  const { data: device } = await db.from("devices").select("*").eq("id", deviceId).single();
  if (!device) redirect("/");
  return { user, role, device: device as DeviceRow };
}

export async function getProfile(userId: string) {
  const { data } = await admin().from("profiles").select("display_name, form").eq("user_id", userId).maybeSingle();
  return data as { display_name: string; form: "f" | "m" | "n" } | null;
}

// Zaproszenia: gdy osoba zaproszona e-mailem pierwszy raz się zaloguje, dostaje dostęp
export async function acceptInvites(userId: string, email: string | undefined) {
  if (!email) return;
  const db = admin();
  const { data: inv } = await db.from("invites").select("id, device_id, role").ilike("email", email);
  for (const i of (inv ?? []) as { id: string; device_id: string; role: Role }[]) {
    await db.from("members").upsert({ device_id: i.device_id, user_id: userId, role: i.role }, { onConflict: "device_id,user_id", ignoreDuplicates: true });
    await db.from("invites").delete().eq("id", i.id);
  }
}
