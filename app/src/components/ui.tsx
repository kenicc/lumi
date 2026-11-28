import Link from "next/link";

export function Top({ back, title }: { back?: { href: string; label: string }; title?: string }) {
  return (
    <div className="top">
      {back && <Link href={back.href}>‹ {back.label}</Link>}
      {title && <span className="dim">{title}</span>}
    </div>
  );
}

export function Flash({ ok, err }: { ok?: string; err?: string }) {
  if (err) return <div className="flash err" role="alert">{err}</div>;
  if (ok === "1")
    return (
      <div className="flash ok" role="status">
        Zapisano. LUMI pobierze zmianę zwykle w kilkanaście sekund (gdy śpi na baterii - do minuty) i pokaże, kto ją
        wprowadził.
      </div>
    );
  if (ok === "0") return <div className="flash ok" role="status">Bez zmian.</div>;
  return null;
}

export function SaveBar({ label = "Zapisz" }: { label?: string }) {
  return (
    <div className="bar">
      <div>
        <button className="btn primary block" type="submit">{label}</button>
      </div>
    </div>
  );
}

export function Avatar({ name, color, photo, size = 44 }: { name: string; color: string; photo?: string; size?: number }) {
  if (photo)
    // eslint-disable-next-line @next/next/no-img-element
    return <img className="avatar" src={photo} alt="" width={size} height={size} style={{ objectFit: "cover", width: size, height: size }} />;
  return (
    <span className="avatar" style={{ background: color }} aria-hidden>
      {Array.from(name)[0]?.toUpperCase() ?? "?"}
    </span>
  );
}

export function ago(iso: string | null): string {
  if (!iso) return "nigdy";
  const s = Math.max(0, Math.round((Date.now() - new Date(iso).getTime()) / 1000));
  if (s < 60) return "przed chwilą";
  const m = Math.round(s / 60);
  if (m < 60) return `${m} min temu`;
  const h = Math.round(m / 60);
  if (h < 24) return `${h} godz. temu`;
  const d = Math.round(h / 24);
  if (d === 1) return "wczoraj";
  if (d < 7) return `${d} dni temu`;
  const w = Math.round(d / 7);
  if (d < 31) return w === 1 ? "tydzień temu" : `${w} tyg. temu`;
  const mo = Math.round(d / 30);
  return mo === 1 ? "miesiąc temu" : `${mo} mies. temu`;
}

// Polska odmiana: 1 nowa propozycja, 2-4 nowe propozycje, 5+ nowych propozycji (ale 22 nowe)
export function plural(n: number, one: string, few: string, many: string) {
  if (n === 1) return one;
  const d = n % 10, t = n % 100;
  return d >= 2 && d <= 4 && (t < 12 || t > 14) ? few : many;
}

export type SP = Promise<{ ok?: string; err?: string }>;
