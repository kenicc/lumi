"use server";
import { revalidatePath } from "next/cache";
import { redirect } from "next/navigation";
import { requireMember } from "@/lib/access";
import { bool, move, newId, num, str } from "@/lib/form";
import { CONTACT_COLORS, type Media } from "@/lib/schema";
import { saveSettings } from "@/lib/store";
import { admin } from "@/lib/supabase/admin";

const p = (id: string, sub = "") => `/d/${id}${sub}`;

// ---------- kontakty ----------

function contactFrom(fd: FormData, guardian: boolean, prevAuto = false) {
  return {
    name: str(fd, "name"),
    number: str(fd, "number"),
    color: CONTACT_COLORS.includes(str(fd, "color")) ? str(fd, "color") : CONTACT_COLORS[0],
    // automatyczne odbieranie ustawia tylko główny opiekun (dokumentacja v2.0, "Role")
    autoAnswer: guardian ? bool(fd, "autoAnswer") : prevAuto,
  };
}

export async function addContact(id: string, fd: FormData) {
  return saveSettings(id, "contacts", (s, role) => {
    if (s.contacts.length >= 30) return "Najwięcej 30 kontaktów";
    const c = { id: newId(), ...contactFrom(fd, role === "guardian") };
    s.contacts.push(c);
    // pierwszy wolny przycisk ekranu głównego dostaje nową osobę
    if (bool(fd, "onHome")) {
      const i = s.home.slots.findIndex((x) => x === "");
      if (i >= 0) s.home.slots[i] = `contact:${c.id}`;
    }
  }, p(id, "/kontakty"));
}

export async function updateContact(id: string, cid: string, fd: FormData) {
  return saveSettings(id, "contacts", (s, role) => {
    const i = s.contacts.findIndex((c) => c.id === cid);
    if (i < 0) return "Nie ma takiego kontaktu";
    s.contacts[i] = { id: cid, ...contactFrom(fd, role === "guardian", s.contacts[i].autoAnswer) };
  }, p(id, "/kontakty"));
}

export async function deleteContact(id: string, cid: string) {
  return saveSettings(id, "contacts", (s) => {
    s.contacts = s.contacts.filter((c) => c.id !== cid);
    s.home.slots = s.home.slots.map((x) => (x === `contact:${cid}` ? "" : x)) as typeof s.home.slots;
  }, p(id, "/kontakty"));
}

export async function moveContact(id: string, cid: string, dir: -1 | 1) {
  return saveSettings(id, "contacts", (s) => move(s.contacts, s.contacts.findIndex((c) => c.id === cid), dir), p(id, "/kontakty"));
}

// ---------- ekran główny ----------

export async function saveHome(id: string, fd: FormData) {
  return saveSettings(id, "home", (s) => {
    s.home.slots = [str(fd, "slot0"), str(fd, "slot1"), str(fd, "slot2")];
    s.home.partOfDay = bool(fd, "partOfDay");
  }, p(id, "/ekran"));
}

// ---------- radio i muzyka ----------

export async function addMedia(id: string, fd: FormData) {
  return saveSettings(id, "media", (s) => {
    if (s.media.length >= 40) return "Najwięcej 40 pozycji";
    const type = str(fd, "type");
    const name = str(fd, "name");
    let m: Media;
    if (type === "fm") m = { type: "fm", name, freq: Math.round(num(fd, "freq") * 10) / 10 };
    else if (type === "web") m = { type: "web", name, url: str(fd, "url") };
    else return "Nieznany rodzaj";
    s.media.push(m);
  }, p(id, "/radio"));
}

export async function addPlaylist(id: string, name: string, urls: string[], from: string) {
  return saveSettings(id, "media", (s) => {
    if (!urls.length) return "Brak plików";
    s.media.push({ type: "playlist", name: name.trim() || "Muzyka", from: from.slice(0, 24) || undefined, urls });
  }, p(id, "/radio"));
}

export async function deleteMedia(id: string, idx: number) {
  return saveSettings(id, "media", (s) => {
    s.media.splice(idx, 1);
  }, p(id, "/radio"));
}

export async function moveMedia(id: string, idx: number, dir: -1 | 1) {
  return saveSettings(id, "media", (s) => move(s.media, idx, dir), p(id, "/radio"));
}

// Podpisany adres do wgrania pliku MP3 prosto z telefonu do magazynu (bez przechodzenia przez serwer)
export async function musicUploadUrl(id: string, fileName: string) {
  await requireMember(id, "media");
  const safe = fileName.toLowerCase().replace(/[^a-z0-9.]+/g, "-").slice(-60);
  const path = `${id}/${crypto.randomUUID()}-${safe.endsWith(".mp3") ? safe : safe + ".mp3"}`;
  const { data, error } = await admin().storage.from("music").createSignedUploadUrl(path);
  if (error || !data) throw new Error("Nie udało się przygotować wysyłania");
  const { data: pub } = admin().storage.from("music").getPublicUrl(path);
  return { path, token: data.token, url: pub.publicUrl };
}

// ---------- przypomnienia ----------

export async function addReminder(id: string, fd: FormData) {
  return saveSettings(id, "reminders", (s) => {
    if (s.reminders.length >= 20) return "Najwięcej 20 przypomnień";
    const days = [1, 2, 3, 4, 5, 6, 7].filter((d) => bool(fd, `d${d}`));
    if (!days.length) return "Zaznacz co najmniej jeden dzień";
    s.reminders.push({ id: newId(), text: str(fd, "text"), time: str(fd, "time"), days: days.length === 7 ? [] : days, confirm: bool(fd, "confirm") });
    s.reminders.sort((a, b) => a.time.localeCompare(b.time));
  }, p(id, "/przypomnienia"));
}

export async function deleteReminder(id: string, rid: string) {
  return saveSettings(id, "reminders", (s) => {
    s.reminders = s.reminders.filter((r) => r.id !== rid);
  }, p(id, "/przypomnienia"));
}

// ---------- wygląd i dźwięk ----------

export async function saveLook(id: string, fd: FormData) {
  return saveSettings(id, "look", (s) => {
    s.display.textSize = num(fd, "textSize");
    s.display.highContrast = bool(fd, "highContrast");
    s.display.brightness = num(fd, "brightness", 80);
    s.display.screenOffSec = num(fd, "screenOffSec", 60);
    s.sound.ringVolume = num(fd, "ringVolume", 8);
    s.sound.maxVolume = num(fd, "maxVolume", 10);
    s.sound.startVolume = Math.min(num(fd, "startVolume", 5), s.sound.maxVolume);
    s.sound.keyClick = bool(fd, "keyClick");
    s.sound.quiet = bool(fd, "quietOn") ? { from: str(fd, "quietFrom"), to: str(fd, "quietTo") } : null;
  }, p(id, "/wyglad"));
}

// ---------- dostępność (tylko główny opiekun) ----------

export async function saveAccess(id: string, fd: FormData) {
  return saveSettings(id, "access", (s) => {
    s.features = { calls: bool(fd, "fCalls"), media: bool(fd, "fMedia"), weather: bool(fd, "fWeather") };
    s.calls.onlyContacts = bool(fd, "onlyContacts");
    s.access.autoAnswerSec = num(fd, "autoAnswerSec", 5);
    s.access.oneButton = bool(fd, "oneButton");
    s.access.minPressMs = num(fd, "minPressMs");
    s.access.repeatIgnoreMs = num(fd, "repeatIgnoreMs", 250);
    s.access.locked = [...(bool(fd, "lockMusic") ? ["music" as const] : []), ...(bool(fd, "lockCall") ? ["call" as const] : [])];
    s.access.allowPowerOff = !bool(fd, "blockPowerOff");
  }, p(id, "/dostepnosc"));
}

// ---------- pogoda ----------

export async function saveWeather(id: string, fd: FormData) {
  return saveSettings(id, "weather", (s) => {
    const lat = num(fd, "lat", NaN), lon = num(fd, "lon", NaN);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return "Wybierz miejscowość z listy";
    s.weather = { place: str(fd, "place").slice(0, 60), lat: Math.round(lat * 1000) / 1000, lon: Math.round(lon * 1000) / 1000 };
  }, p(id, "/pogoda"));
}

// ---------- Wi-Fi ----------

export async function addWifi(id: string, fd: FormData) {
  return saveSettings(id, "wifi", (s) => {
    const ssid = str(fd, "ssid");
    s.wifi = s.wifi.filter((w) => w.ssid !== ssid);
    s.wifi.unshift({ ssid, pass: String(fd.get("pass") ?? "") });
    if (s.wifi.length > 5) return "Najwięcej 5 sieci";
  }, p(id, "/wifi"));
}

export async function deleteWifi(id: string, ssid: string) {
  return saveSettings(id, "wifi", (s) => {
    s.wifi = s.wifi.filter((w) => w.ssid !== ssid);
  }, p(id, "/wifi"));
}

// ---------- osoby (tylko główny opiekun) ----------

export async function invite(id: string, fd: FormData) {
  const { user } = await requireMember(id, "people");
  const email = str(fd, "email").toLowerCase();
  const role = str(fd, "role") === "friend" ? "friend" : "family";
  if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) redirect(p(id, `/osoby?err=${encodeURIComponent("Nieprawidłowy e-mail")}`));
  await admin().from("invites").upsert({ device_id: id, email, role, created_by: user.id }, { onConflict: "device_id,email" });
  revalidatePath(p(id, "/osoby"));
  redirect(p(id, "/osoby?ok=1"));
}

export async function cancelInvite(id: string, inviteId: string) {
  await requireMember(id, "people");
  await admin().from("invites").delete().eq("id", inviteId).eq("device_id", id);
  redirect(p(id, "/osoby"));
}

export async function removeMember(id: string, userId: string) {
  const { user } = await requireMember(id, "people");
  if (userId === user.id) redirect(p(id, `/osoby?err=${encodeURIComponent("Nie możesz usunąć siebie jako głównego opiekuna")}`));
  await admin().from("members").delete().eq("device_id", id).eq("user_id", userId);
  redirect(p(id, "/osoby"));
}

export async function renameDevice(id: string, fd: FormData) {
  await requireMember(id, "people");
  await admin().from("devices").update({ name: str(fd, "name").slice(0, 40) || null }).eq("id", id);
  revalidatePath("/");
  redirect(p(id, "/osoby?ok=1"));
}

// Usunięcie z aplikacji: LUMI wraca do ekranu parowania przy następnym połączeniu
export async function deleteDevice(id: string, fd: FormData) {
  await requireMember(id, "people");
  if (str(fd, "confirm") !== id) redirect(p(id, `/osoby?err=${encodeURIComponent("Wpisz numer urządzenia, żeby potwierdzić")}`));
  await admin().from("devices").delete().eq("id", id);
  revalidatePath("/");
  redirect("/");
}
