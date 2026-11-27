// Dokument dla urządzenia = ustawienia + numer wersji + kto i co zmienił.
import type { Media, Settings } from "./schema";

// Urządzenie nie potrzebuje adresu kanału podcastu - dostaje gotową listę odcinków (jak playlistę)
export type DeviceMedia =
  | Exclude<Media, { type: "podcast" }>
  | { type: "podcast"; name: string; urls: string[]; titles: string[] };

export function deviceMedia(m: Media): DeviceMedia {
  if (m.type !== "podcast") return m;
  return { type: "podcast", name: m.name, urls: m.episodes.map((e) => e.url), titles: m.episodes.map((e) => e.title) };
}

export type DeviceConfig = Omit<Settings, "sound" | "media"> & {
  media: DeviceMedia[];
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
    media: s.media.map(deviceMedia),
    sound: quiet ? { ...sound, quiet } : sound,
  };
}
