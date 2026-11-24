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
        Zapisano. LUMI pobierze zmianę w ciągu kilkunastu sekund i pokaże, kto ją wprowadził.
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

export function Avatar({ name, color }: { name: string; color: string }) {
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
  return `${Math.round(h / 24)} dni temu`;
}

export type SP = Promise<{ ok?: string; err?: string }>;
