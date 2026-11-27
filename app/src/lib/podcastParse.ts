// Parsowanie kanału RSS podcastu bez zależności (kanały to prosty XML; potrzebujemy tylko tytułu
// i pliku audio każdego odcinka). Osobny plik bez "server-only", żeby dało się go testować.

export type Episode = { title: string; url: string };

const ENTITIES: Record<string, string> = { amp: "&", lt: "<", gt: ">", quot: '"', apos: "'" };

function decode(s: string): string {
  return s
    .replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, "$1")
    .replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/gi, (all, e: string) => {
      if (e[0] === "#") {
        const code = e[1] === "x" || e[1] === "X" ? parseInt(e.slice(2), 16) : parseInt(e.slice(1), 10);
        return Number.isFinite(code) ? String.fromCodePoint(code) : all;
      }
      return ENTITIES[e.toLowerCase()] ?? all;
    })
    .replace(/\s+/g, " ")
    .trim();
}

function attr(tag: string, name: string): string | null {
  const m = tag.match(new RegExp(`\\s${name}\\s*=\\s*("([^"]*)"|'([^']*)')`, "i"));
  return m ? decode(m[2] ?? m[3] ?? "") : null;
}

/** Najnowsze odcinki (kolejność z kanału - zwykle od najnowszego) z plikiem audio http(s). */
export function parseFeed(xml: string, n: number): Episode[] {
  const out: Episode[] = [];
  const items = xml.match(/<item[\s>][\s\S]*?<\/item>/gi) ?? xml.match(/<entry[\s>][\s\S]*?<\/entry>/gi) ?? [];
  for (const item of items) {
    if (out.length >= n) break;
    const enc = item.match(/<enclosure\b[^>]*>/i)?.[0] ?? item.match(/<link\b[^>]*rel=["']enclosure["'][^>]*>/i)?.[0];
    if (!enc) continue;
    const url = attr(enc, "url") ?? attr(enc, "href");
    const type = attr(enc, "type") ?? "";
    if (!url || !/^https?:\/\//i.test(url)) continue;
    if (type && !/^audio\//i.test(type)) continue;  // wideo pomijamy - LUMI gra tylko dźwięk
    const titleRaw = item.match(/<title\b[^>]*>([\s\S]*?)<\/title>/i)?.[1] ?? "";
    const title = decode(titleRaw).slice(0, 80) || "Odcinek";
    out.push({ title, url });
  }
  return out;
}

/**
 * Tylko publiczne adresy http(s): serwer pobiera kanał podany przez użytkownika, więc nie może
 * dać się namówić na zajrzenie do sieci wewnętrznej (localhost, 10.x, 192.168.x, metadane chmury).
 */
export function isPublicHttpUrl(raw: string): boolean {
  let u: URL;
  try {
    u = new URL(raw);
  } catch {
    return false;
  }
  if (u.protocol !== "http:" && u.protocol !== "https:") return false;
  if (u.username || u.password) return false;
  const h = u.hostname.toLowerCase().replace(/^\[|\]$/g, "");
  if (!h || h === "localhost" || h.endsWith(".localhost") || h.endsWith(".local") || h.endsWith(".internal")) return false;
  if ((/^\d+\.\d+\.\d+\.\d+$/.test(h) || h.includes(":")) && !isPublicIp(h)) return false;
  return true;
}

/** Adres IP spoza sieci prywatnych, pętli, lokalnych łączy i metadanych chmury. */
export function isPublicIp(ip: string): boolean {
  const h = ip.toLowerCase();
  if (/^\d+\.\d+\.\d+\.\d+$/.test(h)) {
    const [a, b] = h.split(".").map(Number);
    return !(a === 10 || a === 127 || a === 0 || (a === 169 && b === 254) || (a === 172 && b >= 16 && b <= 31) ||
      (a === 192 && b === 168) || a >= 224 || (a === 100 && b >= 64 && b <= 127));
  }
  if (h.includes(":")) {
    if (h.startsWith("::ffff:")) {  // IPv4 zapisany jako IPv6: "::ffff:10.0.0.1" albo (po URL) "::ffff:a00:1"
      const rest = h.slice(7);
      if (rest.includes(".")) return isPublicIp(rest);
      const g = rest.split(":").map((x) => parseInt(x, 16));
      if (g.length !== 2 || g.some((x) => !Number.isFinite(x))) return false;
      return isPublicIp(`${g[0] >> 8}.${g[0] & 255}.${g[1] >> 8}.${g[1] & 255}`);
    }
    return !(h === "::1" || h === "::" || h.startsWith("fe80") || h.startsWith("fc") || h.startsWith("fd"));
  }
  return false;
}
