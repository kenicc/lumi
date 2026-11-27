import { describe, expect, it } from "vitest";
import { isPublicHttpUrl, parseFeed } from "@/lib/podcastParse";
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
