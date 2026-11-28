// Zapis zmiany zrobionej na samym LUMI (zdarzenie "setting"): ta sama blokada wersji co zapis
// rodziny (lib/store.ts) - przy kolizji operacja nakłada się na nowsze ustawienia.
import "server-only";
import { buildDeviceConfig } from "./config";
import { applySettingOp, type SettingOp } from "./deviceSettings";
import { currentSettings } from "./store";
import { summarize } from "./summary";
import { admin } from "./supabase/admin";

/** true = zapisano; false = nic się nie zmieniło (np. ponowione zdarzenie) albo kolizja nie ustąpiła. */
export async function saveDeviceOp(deviceId: string, deviceName: string | null, op: SettingOp): Promise<boolean> {
  const db = admin();
  for (let attempt = 0; attempt < 3; attempt++) {
    const { data } = await db.from("devices").select("settings, config_version").eq("id", deviceId).single();
    const row = data as { settings: unknown; config_version: number } | null;
    if (!row) return false;
    const prev = currentSettings(row.settings);
    const next = applySettingOp(prev, op);
    if (!next) return false;
    const version = row.config_version + 1;
    // bez autora w dokumencie: LUMI nie pokaże seniorowi komunikatu o jego własnej zmianie
    const config = buildDeviceConfig(next, version, "", "");
    const { data: upd } = await db
      .from("devices")
      .update({ settings: next, config, config_version: version })
      .eq("id", deviceId)
      .eq("config_version", row.config_version)
      .select("id");
    if (upd?.length) {
      await db.from("changes").insert({ device_id: deviceId, user_id: null, author: `${deviceName ?? "LUMI"} (na urządzeniu)`, summary: summarize(prev, next, "n"), version });
      return true;
    }
  }
  return false;
}
