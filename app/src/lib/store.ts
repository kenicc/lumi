import "server-only";
import { revalidatePath } from "next/cache";
import { redirect } from "next/navigation";
import { getProfile, requireMember, type Section } from "./access";
import { buildDeviceConfig } from "./config";
import { SettingsSchema, defaultSettings, type Settings } from "./schema";
import { summarize, type Form } from "./summary";
import { admin } from "./supabase/admin";
import type { Role } from "./types";

/**
 * Ustawienia z bazy. Brakujące sekcje (np. dodane w nowszej wersji aplikacji) dostają wartości
 * domyślne. Uszkodzonych danych NIE zastępujemy domyślnymi po cichu - następny zapis
 * skasowałby rodzinie kontakty. Zamiast tego błąd, który widać i da się naprawić.
 */
export function currentSettings(raw: unknown): Settings {
  if (raw == null) return defaultSettings();
  const merged = { ...defaultSettings(), ...(raw as object) };
  const r = SettingsSchema.safeParse(merged);
  if (!r.success) throw new Error(`Ustawienia urządzenia w bazie są nieprawidłowe: ${r.error.issues[0]?.path.join(".")}`);
  return r.data;
}

/**
 * Jedyna droga zmiany ustawień: sprawdza rolę, waliduje tym samym schematem co urządzenie,
 * podbija wersję, składa zdanie "Ania dodała kontakt Basia" i zapisuje dokument dla LUMI.
 * mutate może zwrócić tekst błędu - wtedy nic nie jest zapisywane.
 */
export async function saveSettings(
  deviceId: string,
  section: Section,
  mutate: (s: Settings, role: Role) => string | void,
  back: string,
): Promise<never> {
  const { user, role, device } = await requireMember(deviceId, section);
  const prev = currentSettings(device.settings);
  const next = structuredClone(prev);
  const userError = mutate(next, role);
  if (userError) redirect(`${back}?err=${encodeURIComponent(userError)}`);

  const parsed = SettingsSchema.safeParse(next);
  if (!parsed.success) {
    const msg = parsed.error.issues[0]?.message ?? "Nieprawidłowe dane";
    redirect(`${back}?err=${encodeURIComponent(msg)}`);
  }
  if (JSON.stringify(parsed.data) === JSON.stringify(prev)) redirect(`${back}?ok=0`);

  const profile = await getProfile(user.id);
  const author = profile?.display_name ?? "Rodzina";
  const form: Form = profile?.form ?? "n";
  const summary = summarize(prev, parsed.data, form);

  const db = admin();
  // optymistyczna blokada: dwie osoby zapisujące naraz nie nadpiszą sobie zmian po cichu
  for (let attempt = 0; attempt < 2; attempt++) {
    const { data: cur } = await db.from("devices").select("config_version").eq("id", deviceId).single();
    const version = ((cur as { config_version: number } | null)?.config_version ?? 0) + 1;
    const config = buildDeviceConfig(parsed.data, version, author, summary);
    const { data: upd } = await db
      .from("devices")
      .update({ settings: parsed.data, config, config_version: version })
      .eq("id", deviceId)
      .eq("config_version", version - 1)
      .select("id");
    if (upd && upd.length) {
      await db.from("changes").insert({ device_id: deviceId, user_id: user.id, author, summary, version });
      revalidatePath(`/d/${deviceId}`, "layout");
      redirect(`${back}?ok=1`);
    }
  }
  redirect(`${back}?err=${encodeURIComponent("Ktoś właśnie zmienił ustawienia. Spróbuj ponownie.")}`);
}
