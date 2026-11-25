"use server";
import { redirect } from "next/navigation";
import { requireUser, getProfile } from "@/lib/access";
import { buildDeviceConfig } from "@/lib/config";
import { safeEqualHex, sha256, ID_RE, isClaimed } from "@/lib/deviceAuth";
import { str, newId } from "@/lib/form";
import { CONTACT_COLORS, defaultSettings } from "@/lib/schema";
import { pairedText } from "@/lib/summary";
import { admin } from "@/lib/supabase/admin";
import { supabaseServer } from "@/lib/supabase/server";

export async function saveProfile(fd: FormData) {
  const user = await requireUser();
  const name = str(fd, "name").slice(0, 24);
  const form = ["f", "m", "n"].includes(str(fd, "form")) ? str(fd, "form") : "n";
  const next = str(fd, "next") || "/";
  if (!name) redirect(`/profil?next=${encodeURIComponent(next)}&err=${encodeURIComponent("Podaj imię")}`);
  await admin().from("profiles").upsert({ user_id: user.id, display_name: name, form });
  redirect(next.startsWith("/") ? next : "/");
}

export async function savePush(endpoint: string, p256dh: string, auth: string) {
  const user = await requireUser();
  if (!/^https:\/\//.test(endpoint) || endpoint.length > 1000) throw new Error("zły adres");
  await admin().from("push_subscriptions").upsert({ endpoint, user_id: user.id, p256dh, auth });
}

export async function removePush(endpoint: string) {
  const user = await requireUser();
  await admin().from("push_subscriptions").delete().eq("endpoint", endpoint).eq("user_id", user.id);
}

export async function signOut() {
  const sb = await supabaseServer();
  await sb.auth.signOut();
  redirect("/login");
}

// Parowanie: kod z ekranu LUMI dowodzi, że ktoś ma urządzenie w ręku
export async function claim(fd: FormData) {
  const deviceId = str(fd, "d").toUpperCase();
  const code = str(fd, "c").toUpperCase().replace(/\s+/g, "");
  const back = `/pair?d=${encodeURIComponent(deviceId)}&c=${encodeURIComponent(code)}`;
  const user = await requireUser(back);
  const fail = (msg: string) => redirect(`${back}&err=${encodeURIComponent(msg)}`);
  if (!ID_RE.test(deviceId) || !/^[A-Z2-9]{8}$/.test(code)) fail("Nieprawidłowy kod. Zeskanuj go jeszcze raz z ekranu LUMI.");

  const db = admin();
  const { data: dev } = await db.from("devices").select("id, pair_hash").eq("id", deviceId).maybeSingle();
  if (!dev) fail("LUMI jeszcze nie połączyło się z serwerem. Sprawdź, czy ma Wi-Fi, i spróbuj za chwilę.");
  const { data: mine } = await db.from("members").select("role").eq("device_id", deviceId).eq("user_id", user.id).maybeSingle();
  if (mine) redirect(`/d/${deviceId}`);
  if (await isClaimed(deviceId)) fail("To LUMI ma już opiekuna. Poproś go o zaproszenie.");
  const pairHash = (dev as { pair_hash: string | null }).pair_hash;
  if (!pairHash || !safeEqualHex(pairHash, sha256(code))) fail("Kod się nie zgadza. Zeskanuj go jeszcze raz z ekranu LUMI.");

  const profile = await getProfile(user.id);
  if (!profile) redirect(`/profil?next=${encodeURIComponent(back)}`);

  // kod jednorazowy: wygrywa tylko jedno parowanie, nawet gdy dwie osoby skanują naraz
  const { data: won } = await db.from("devices").update({ pair_hash: null }).eq("id", deviceId).eq("pair_hash", pairHash!).select("id");
  if (!won?.length) fail("Ktoś właśnie dodał to LUMI. Poproś go o zaproszenie.");

  const settings = defaultSettings();
  // numer opiekuna od razu jako pierwszy kontakt i przycisk na ekranie głównym
  const phone = str(fd, "phone");
  if (/^\+?[0-9 ]{9,20}$/.test(phone)) {
    const id = newId();
    settings.contacts.push({ id, name: profile.display_name, number: phone, color: CONTACT_COLORS[0], autoAnswer: false });
    settings.home.slots[0] = `contact:${id}`;
  }
  const summary = pairedText(profile.form);
  const config = buildDeviceConfig(settings, 1, profile.display_name, summary);
  const name = str(fd, "name").slice(0, 40) || "LUMI";
  await db.from("members").insert({ device_id: deviceId, user_id: user.id, role: "guardian" });
  await db.from("devices").update({ name, settings, config, config_version: 1, applied_version: 0 }).eq("id", deviceId);
  await db.from("changes").insert({ device_id: deviceId, user_id: user.id, author: profile.display_name, summary, version: 1 });
  redirect(`/d/${deviceId}?ok=1`);
}
