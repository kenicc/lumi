import { proposeContact } from "../actions";
import { requireMember } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";
import { Flash, Top, type SP } from "@/components/ui";

const STATUS: Record<string, { text: string; cls: string }> = {
  pending: { text: "czeka na decyzję opiekuna", cls: "badge" },
  accepted: { text: "dodany do LUMI", cls: "badge ok" },
  rejected: { text: "nie dodany", cls: "badge bad" },
};

// Dla znajomych: zaproponuj kontakt (lista kontaktów seniora jest dla nich niewidoczna)
export default async function Propose({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { user, device } = await requireMember(id, "propose");
  const { data } = await admin()
    .from("contact_proposals")
    .select("id, name, status, created_at")
    .eq("device_id", id)
    .eq("user_id", user.id)
    .order("id", { ascending: false })
    .limit(10);
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Zaproponuj kontakt</h1>
      <p className="dim">Opiekun zobaczy propozycję i zdecyduje, czy dodać ją do LUMI.</p>
      {ok === "1" ? <div className="flash ok" role="status">Wysłano propozycję do opiekuna.</div> : <Flash err={err} />}
      <form action={proposeContact.bind(null, id)} className="card">
        <label htmlFor="name">Imię na przycisku</label>
        <input id="name" name="name" type="text" required maxLength={24} placeholder="np. Kasia" />
        <label htmlFor="number">Numer telefonu</label>
        <input id="number" name="number" type="tel" required placeholder="+48 600 100 200" />
        <button className="btn primary block" style={{ marginTop: 12 }}>Wyślij propozycję</button>
      </form>
      {(data ?? []).length > 0 && (
        <ul className="list card">
          {(data ?? []).map((p) => (
            <li key={p.id} className="row" style={{ padding: "8px 0" }}>
              <b className="grow">{p.name}</b>
              <span className={STATUS[p.status]?.cls}>{STATUS[p.status]?.text}</span>
            </li>
          ))}
        </ul>
      )}
    </main>
  );
}
