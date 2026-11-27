// Podcasty: rodzina podaje adres kanału RSS, serwer wyciąga najnowsze odcinki (plik MP3 + tytuł).
// LUMI dostaje gotową listę adresów jak playlistę - nie musi rozumieć RSS ani XML.
import "server-only";
import { lookup } from "node:dns/promises";
import { buildDeviceConfig } from "./config";
import { SettingsSchema, type Settings } from "./schema";
import { admin } from "./supabase/admin";
import { isPublicHttpUrl, isPublicIp, parseFeed, type Episode } from "./podcastParse";

export { parseFeed, isPublicHttpUrl, type Episode };

// Nazwa domeny też nie może prowadzić do sieci wewnętrznej (np. wpis DNS wskazujący na 10.0.0.1)
async function resolvesPublic(url: string): Promise<boolean> {
  try {
    const host = new URL(url).hostname.replace(/^\[|\]$/g, "");
    const addrs = await lookup(host, { all: true });
    return addrs.length > 0 && addrs.every((a) => isPublicIp(a.address));
  } catch {
    return false;
  }
}

const MAX_BYTES = 3 * 1024 * 1024;  // kanały z setkami odcinków mają kilka MB - nowe są na początku
const TIMEOUT_MS = 10_000;
export const EPISODES = 5;

/** Pobiera kanał i zwraca najnowsze odcinki albo tekst błędu dla rodziny. */
export async function fetchEpisodes(feed: string, n = EPISODES): Promise<Episode[] | string> {
  // przekierowania obsługujemy sami: każdy kolejny adres też musi być publiczny
  let url = feed;
  let res: Response | null = null;
  for (let hop = 0; hop < 4; hop++) {
    if (!isPublicHttpUrl(url) || !(await resolvesPublic(url))) return "To nie wygląda na publiczny adres podcastu";
    try {
      res = await fetch(url, {
        signal: AbortSignal.timeout(TIMEOUT_MS),
        redirect: "manual",
        headers: { "User-Agent": "LUMI-podcast/1.0", Accept: "application/rss+xml, application/xml, text/xml" },
      });
    } catch {
      return "Nie udało się pobrać podcastu - sprawdź adres";
    }
    const loc = res.status >= 300 && res.status < 400 ? res.headers.get("location") : null;
    if (!loc) break;
    url = new URL(loc, url).toString();
    res = null;
  }
  if (!res) return "Za dużo przekierowań pod tym adresem";
  if (!res.ok || !res.body) return `Serwer podcastu odpowiedział błędem (${res.status})`;
  // czytamy najwyżej MAX_BYTES - najnowsze odcinki są na początku kanału
  const reader = res.body.getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  while (total < MAX_BYTES) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    total += value.length;
  }
  await reader.cancel().catch(() => {});
  const xml = new TextDecoder().decode(Buffer.concat(chunks));
  const eps = parseFeed(xml, n);
  return eps.length ? eps : "Nie znalazłem odcinków z plikiem audio pod tym adresem";
}

/**
 * Odświeżenie podcastów wszystkich urządzeń (z harmonogramu). Zmienia ustawienia tylko, gdy
 * pojawił się nowszy odcinek. Bez autora i opisu zmiany - LUMI nie pokaże seniorowi komunikatu
 * "zmiana od rodziny" przy każdym nowym odcinku.
 */
export async function refreshPodcasts(): Promise<number> {
  const db = admin();
  const { data } = await db.from("devices").select("id, settings").gt("config_version", 0);
  let updated = 0;
  for (const d of (data ?? []) as { id: string; settings: Settings | null }[]) {
    const parsed = SettingsSchema.safeParse(d.settings);
    if (!parsed.success || !parsed.data.media.some((m) => m.type === "podcast")) continue;
    const next = structuredClone(parsed.data);
    let changed = false;
    for (const m of next.media) {
      if (m.type !== "podcast") continue;
      const eps = await fetchEpisodes(m.feed);
      if (typeof eps === "string") continue;  // chwilowy błąd kanału: zostają poprzednie odcinki
      if (eps[0].url !== m.episodes[0]?.url || eps.length !== m.episodes.length) {
        m.episodes = eps;
        changed = true;
      }
    }
    if (!changed) continue;
    for (let attempt = 0; attempt < 2; attempt++) {  // ta sama blokada co przy zapisie przez rodzinę
      const { data: cur } = await db.from("devices").select("config_version").eq("id", d.id).single();
      const version = ((cur as { config_version: number } | null)?.config_version ?? 0) + 1;
      const { data: upd } = await db
        .from("devices")
        .update({ settings: next, config: buildDeviceConfig(next, version, "", ""), config_version: version })
        .eq("id", d.id)
        .eq("config_version", version - 1)
        .select("id");
      if (upd && upd.length) {
        updated++;
        break;
      }
    }
  }
  return updated;
}
