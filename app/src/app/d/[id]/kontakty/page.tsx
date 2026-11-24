import { addContact, deleteContact, moveContact, updateContact } from "../actions";
import { requireMember } from "@/lib/access";
import { CONTACT_COLORS, type Contact } from "@/lib/schema";
import { currentSettings } from "@/lib/store";
import { Avatar, Flash, Top, type SP } from "@/components/ui";

function ContactFields({ c, guardian, autoSec }: { c?: Contact; guardian: boolean; autoSec: number }) {
  return (
    <>
      <label htmlFor="name">Imię na przycisku</label>
      <input id="name" name="name" type="text" required maxLength={24} defaultValue={c?.name} placeholder="np. Ania" />
      <p className="dim small">Krótko: do 9 liter mieści się w całości na przycisku LUMI.</p>
      <label htmlFor="number">Numer telefonu</label>
      <input id="number" name="number" type="tel" required defaultValue={c?.number} placeholder="+48 600 100 200" />
      <label>Kolor kółka</label>
      <div className="row" style={{ flexWrap: "wrap" }}>
        {CONTACT_COLORS.map((col, i) => (
          <label key={col} className="check" style={{ margin: 0, minHeight: 0 }}>
            <input type="radio" name="color" value={col} defaultChecked={(c?.color ?? CONTACT_COLORS[0]) === col || (!c && i === 0)} />
            <span className="avatar" style={{ background: col, width: 32, height: 32 }} />
          </label>
        ))}
      </div>
      {guardian && (
        <label className="check">
          <input type="checkbox" name="autoAnswer" defaultChecked={c?.autoAnswer} />
          <span>
            Odbieraj automatycznie po {autoSec} s
            <br />
            <span className="dim small">Dla osób, które nie mogą nacisnąć przycisku. LUMI da wyraźny sygnał.</span>
          </span>
        </label>
      )}
    </>
  );
}

export default async function Contacts({
  params,
  searchParams,
}: {
  params: Promise<{ id: string }>;
  searchParams: Promise<{ ok?: string; err?: string; edit?: string }>;
}) {
  const { id } = await params;
  const { ok, err, edit } = await searchParams;
  const { role, device } = await requireMember(id, "contacts");
  const s = currentSettings(device.settings);
  const guardian = role === "guardian";
  const editing = s.contacts.find((c) => c.id === edit);

  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Kontakty</h1>
      <p className="dim">Ta kolejność jest pod przyciskiem Dzwoń. Najważniejsze osoby na górze.</p>
      <Flash ok={ok} err={err} />

      {editing ? (
        <form action={updateContact.bind(null, id, editing.id)} className="card">
          <h2 style={{ marginTop: 0 }}>Zmień: {editing.name}</h2>
          <ContactFields c={editing} guardian={guardian} autoSec={s.access.autoAnswerSec} />
          <div className="row" style={{ marginTop: 16 }}>
            <a className="btn grow" href={`/d/${id}/kontakty`}>Anuluj</a>
            <button className="btn primary grow">Zapisz</button>
          </div>
        </form>
      ) : (
        <>
          <ul className="list card">
            {s.contacts.length === 0 && <li className="dim" style={{ padding: 8 }}>Brak kontaktów.</li>}
            {s.contacts.map((c, i) => (
              <li key={c.id} className="row" style={{ padding: "10px 0" }}>
                <Avatar name={c.name} color={c.color} />
                <div className="grow">
                  <b>{c.name}</b> {c.autoAnswer && <span className="badge">auto</span>}{" "}
                  {s.home.slots.includes(`contact:${c.id}`) && <span className="badge ok">ekran główny</span>}
                  <div className="dim small">{c.number}</div>
                </div>
                <form action={moveContact.bind(null, id, c.id, -1)}>
                  <button className="btn small" disabled={i === 0} aria-label={`Przesuń ${c.name} wyżej`}>↑</button>
                </form>
                <form action={moveContact.bind(null, id, c.id, 1)}>
                  <button className="btn small" disabled={i === s.contacts.length - 1} aria-label={`Przesuń ${c.name} niżej`}>↓</button>
                </form>
                <a className="btn small" href={`?edit=${c.id}`}>Zmień</a>
              </li>
            ))}
          </ul>
          <details className="card">
            <summary>Dodaj kontakt</summary>
            <form action={addContact.bind(null, id)}>
              <ContactFields guardian={guardian} autoSec={s.access.autoAnswerSec} />
              {s.home.slots.includes("") && (
                <label className="check">
                  <input type="checkbox" name="onHome" defaultChecked /> Dodaj też przycisk na ekranie głównym
                </label>
              )}
              <button className="btn primary block" style={{ marginTop: 12 }}>Dodaj</button>
            </form>
          </details>
          {s.contacts.length > 0 && (
            <details className="card">
              <summary>Usuń kontakt</summary>
              {s.contacts.map((c) => (
                <form key={c.id} action={deleteContact.bind(null, id, c.id)} className="row" style={{ padding: "6px 0" }}>
                  <span className="grow">{c.name}</span>
                  <button className="btn small danger">Usuń</button>
                </form>
              ))}
            </details>
          )}
        </>
      )}
    </main>
  );
}
