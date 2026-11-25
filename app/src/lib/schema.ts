// Ustawienia urządzenia edytowane w aplikacji. Kształt = dokument konfiguracji firmware
// (firmware/src/core/Config.cpp, opis: firmware/docs/config.md) bez pól version/changedBy/changeText.
// Granice liczb są takie same jak w firmware, żeby urządzenie niczego po cichu nie przycinało.
import { z } from "zod";

const hex = z.string().regex(/^#[0-9A-Fa-f]{6}$/);
const hm = z.string().regex(/^([01]\d|2[0-3]):[0-5]\d$/);
const httpUrl = z.string().trim().url().regex(/^https?:\/\//, "Adres musi zaczynać się od http:// lub https://");

export const ContactSchema = z.object({
  id: z.string().min(1).max(40),
  name: z.string().trim().min(1, "Podaj imię").max(24),
  number: z
    .string()
    .trim()
    .regex(/^\+?[0-9 ]{3,20}$/, "Numer może mieć tylko cyfry, spacje i + na początku"),
  color: hex.default("#3D8BFD"),
  autoAnswer: z.boolean().default(false),
  photo: z.string().regex(/^[0-9a-f]{16}$/).optional(), // skrót zdjęcia (pliki w prywatnym koszu "photos")
});

const name = z.string().trim().min(1, "Podaj nazwę").max(40);
export const MediaSchema = z.discriminatedUnion("type", [
  z.object({ type: z.literal("fm"), name, freq: z.number().min(76).max(108) }),
  z.object({ type: z.literal("web"), name, url: httpUrl }),
  z.object({ type: z.literal("playlist"), name, from: z.string().max(24).optional(), urls: z.array(httpUrl).min(1).max(100) }),
]);

export const ReminderSchema = z.object({
  id: z.string().min(1).max(40),
  text: z.string().trim().min(1, "Wpisz treść przypomnienia").max(60),
  time: hm,
  days: z.array(z.number().int().min(1).max(7)).max(7), // 1 = poniedziałek; pusta = codziennie
  confirm: z.boolean(),
});

export const SlotSchema = z.string().regex(/^(|media|weather|contact:.{1,40})$/);

export const SettingsSchema = z.object({
  features: z.object({ calls: z.boolean(), media: z.boolean(), weather: z.boolean() }),
  contacts: z.array(ContactSchema).max(30),
  calls: z.object({ onlyContacts: z.boolean() }),
  home: z.object({ slots: z.tuple([SlotSchema, SlotSchema, SlotSchema]), partOfDay: z.boolean() }),
  media: z.array(MediaSchema).max(40),
  weather: z.object({ place: z.string().max(60), lat: z.number().min(-90).max(90), lon: z.number().min(-180).max(180) }),
  display: z.object({
    textSize: z.number().int().min(0).max(2),
    highContrast: z.boolean(),
    brightness: z.number().int().min(10).max(100),
    screenOffSec: z.number().int().min(10).max(600),
  }),
  sound: z.object({
    ringVolume: z.number().int().min(1).max(10),
    maxVolume: z.number().int().min(1).max(10),
    startVolume: z.number().int().min(1).max(10),
    keyClick: z.boolean(),
    quiet: z.object({ from: hm, to: hm }).nullable(),
  }),
  access: z.object({
    autoAnswerSec: z.number().int().min(3).max(10),
    oneButton: z.boolean(),
    minPressMs: z.number().int().min(0).max(1500),
    repeatIgnoreMs: z.number().int().min(0).max(2000),
    locked: z.array(z.enum(["music", "call"])),
    allowPowerOff: z.boolean(),
  }),
  wifi: z.array(z.object({ ssid: z.string().trim().min(1).max(32), pass: z.string().max(64) })).max(5),
  sync: z.object({ intervalSec: z.number().int().min(10).max(3600) }),
  // pola dodane później mają wartość domyślną - stare zapisy w bazie nadal przechodzą walidację
  reminders: z.array(ReminderSchema).max(20).default([]),
});

export type Settings = z.infer<typeof SettingsSchema>;
export type Contact = z.infer<typeof ContactSchema>;
export type Media = z.infer<typeof MediaSchema>;
export type Reminder = z.infer<typeof ReminderSchema>;

export function defaultSettings(): Settings {
  return {
    features: { calls: true, media: true, weather: true },
    contacts: [],
    calls: { onlyContacts: false },
    home: { slots: ["", "media", "weather"], partOfDay: true },
    media: [],
    weather: { place: "Warszawa", lat: 52.23, lon: 21.01 },
    display: { textSize: 0, highContrast: false, brightness: 80, screenOffSec: 60 },
    sound: { ringVolume: 8, maxVolume: 10, startVolume: 5, keyClick: true, quiet: { from: "22:00", to: "07:00" } },
    access: { autoAnswerSec: 5, oneButton: false, minPressMs: 0, repeatIgnoreMs: 250, locked: [], allowPowerOff: true },
    wifi: [],
    sync: { intervalSec: 20 },
    reminders: [],
  };
}

// Kolory kółek z inicjałem - dobrze widoczne na ciemnym ekranie LUMI, z białą literą
export const CONTACT_COLORS = ["#3D8BFD", "#C2185B", "#2E7D32", "#6D4C41", "#7B1FA2", "#E65100", "#00838F", "#5D4037"];
