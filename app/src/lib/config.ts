// Dokument dla urządzenia = ustawienia + numer wersji + kto i co zmienił.
import type { Settings } from "./schema";

export type DeviceConfig = Omit<Settings, "sound"> & {
  version: number;
  changedBy: string;
  changeText: string;
  sound: Omit<Settings["sound"], "quiet"> & { quiet?: { from: string; to: string } };
};

export function buildDeviceConfig(s: Settings, version: number, changedBy: string, changeText: string): DeviceConfig {
  const { quiet, ...sound } = s.sound;
  return {
    version,
    changedBy,
    changeText,
    ...s,
    contacts: s.contacts.map((c) => ({ ...c, number: c.number.replace(/\s+/g, "") })),
    sound: quiet ? { ...sound, quiet } : sound,
  };
}
