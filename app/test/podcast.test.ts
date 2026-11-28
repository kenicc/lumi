import { describe, expect, it } from "vitest";
import { isPublicHttpUrl, mapLimit, parseFeed, withEpisodes } from "@/lib/podcastParse";
import { buildDeviceConfig } from "@/lib/config";
import { SettingsSchema } from "@/lib/schema";
import { sample } from "./fixtures";

const RSS = `<?xml version="1.0"?><rss><channel><title>Audycje</title>
<item><title><![CDATA[Odcinek 3: Wspomnienia & muzyka]]></title>
  <enclosure length="1" type="audio/mpeg" url="https://cdn.example.com/e3.mp3?a=1&amp;b=2"/></item>
<item><title>Odcinek 2 - wideo</title><enclosure url="https://cdn.example.com/e2.mp4" type="video/mp4"/></item>
<item><title>Odcinek 1 &#8211; początek</title><enclosure type='audio/mpeg' url='http://cdn.example.com/e1.mp3'/></item>
<item><title>Bez pliku</title></item>
</channel></rss>`;

describe("podcast", () => {
  it("wyciąga odcinki audio od najnowszego, dekoduje tytuły i adresy", () => {
    const eps = parseFeed(RSS, 5);
    expect(eps).toEqual([
      { title: "Odcinek 3: Wspomnienia & muzyka", url: "https://cdn.example.com/e3.mp3?a=1&b=2" },
      { title: "Odcinek 1 – początek", url: "http://cdn.example.com/e1.mp3" },
    ]);
    expect(parseFeed(RSS, 1)).toHaveLength(1);
    expect(parseFeed("<html>to nie kanał</html>", 5)).toEqual([]);
  });

  it("przyjmuje tylko publiczne adresy http(s)", () => {
    expect(isPublicHttpUrl("https://feeds.example.com/rss")).toBe(true);
    for (const bad of [
      "ftp://example.com/rss",
      "http://localhost/rss",
      "http://127.0.0.1/rss",
      "http://10.1.2.3/rss",
      "http://192.168.1.1/rss",
      "http://172.20.0.1/rss",
      "http://169.254.169.254/latest/meta-data",
      "http://[::1]/rss",
      "http://[::ffff:10.0.0.1]/rss",
      "http://user:pass@example.com/rss",
      "nie adres",
    ])
      expect(isPublicHttpUrl(bad), bad).toBe(false);
  });

  it("urządzenie dostaje listę odcinków, bez adresu kanału", () => {
    const s = sample();
    s.media.push({
      type: "podcast",
      name: "Audycje",
      feed: "https://feeds.example.com/rss",
      episodes: [{ title: "Odcinek 3", url: "https://cdn.example.com/e3.mp3" }],
    });
    expect(SettingsSchema.safeParse(s).success).toBe(true);
    const doc = buildDeviceConfig(s, 1, "", "");
    expect(doc.media[4]).toEqual({ type: "podcast", name: "Audycje", urls: ["https://cdn.example.com/e3.mp3"], titles: ["Odcinek 3"] });
    expect(JSON.stringify(doc)).not.toContain("feeds.example.com");
  });
});

describe("mapLimit", () => {
  it("zachowuje kolejność i nie przekracza limitu jednoczesnych zadań", async () => {
    let running = 0, peak = 0;
    const out = await mapLimit([30, 5, 20, 1, 10, 2, 15], 3, async (ms) => {
      running++;
      peak = Math.max(peak, running);
      await new Promise((r) => setTimeout(r, ms));
      running--;
      return ms * 2;
    });
    expect(out).toEqual([60, 10, 40, 2, 20, 4, 30]);
    expect(peak).toBe(3);
    expect(await mapLimit([], 3, async (x) => x)).toEqual([]);
  });
});

describe("withEpisodes", () => {
  const feedOf = () => {
    const m = sample().media.find((x) => x.type === "podcast");
    if (!m || m.type !== "podcast") throw new Error("fixture bez podcastu");
    return m;
  };
  it("podmienia odcinki tylko przy nowym odcinku", () => {
    const pod = feedOf();
    const fresh = [{ title: "Nowy", url: "https://cdn.example.com/new.mp3" }, ...pod.episodes].slice(0, pod.episodes.length);
    const next = withEpisodes(sample(), new Map([[pod.feed, fresh]]));
    const m = next?.media.find((x) => x.type === "podcast");
    expect(m && m.type === "podcast" && m.episodes[0].title).toBe("Nowy");
    expect(withEpisodes(sample(), new Map([[pod.feed, pod.episodes]]))).toBeNull();
  });
  it("błąd kanału zostawia poprzednie odcinki", () => {
    expect(withEpisodes(sample(), new Map([[feedOf().feed, "Serwer podcastu odpowiedział błędem (500)"]]))).toBeNull();
    expect(withEpisodes(sample(), new Map())).toBeNull();
  });
});
