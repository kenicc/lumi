import { deleteAccount, saveProfile } from "../actions";
import { getProfile, requireUser } from "@/lib/access";
import { Flash, Top } from "@/components/ui";
import { PushToggle } from "@/components/PushToggle";

export default async function Profil({ searchParams }: { searchParams: Promise<{ next?: string; err?: string }> }) {
  const { next = "/", err } = await searchParams;
  const user = await requireUser("/profil");
  const p = await getProfile(user.id);
  return (
    <main>
      <Top back={{ href: "/", label: "Urządzenia" }} />
      <h1>O Tobie</h1>
      <p className="dim">LUMI pokazuje użytkownikowi, kto zmienił ustawienia, np. „Ania dodała kontakt Basia”.</p>
      <Flash err={err} />
      <form action={saveProfile} className="card">
        <input type="hidden" name="next" value={next} />
        <label htmlFor="name">Jak Cię nazywa osoba, która używa LUMI?</label>
        <input id="name" name="name" type="text" required maxLength={24} defaultValue={p?.display_name ?? ""} placeholder="np. Ania, Wnuczek Marek" />
        <label>Jak ma brzmieć komunikat?</label>
        {[
          ["f", "Ania dodała kontakt"],
          ["m", "Marek dodał kontakt"],
          ["n", "Ania dodaje kontakt"],
        ].map(([v, t]) => (
          <label className="check" key={v}>
            <input type="radio" name="form" value={v} defaultChecked={(p?.form ?? "n") === v} /> {t}
          </label>
        ))}
        <button className="btn primary block">Zapisz</button>
      </form>
      <section className="card">
        <h2 style={{ marginTop: 0 }}>Powiadomienia</h2>
        <p className="dim small">Słaba bateria, brak karty SIM lub zasięgu, LUMI bez połączenia od 2 godzin.</p>
        <PushToggle />
      </section>
      <details className="card">
        <summary>Usuń konto</summary>
        <p className="dim small">
          Usuniemy Twoje konto i dostęp do wszystkich LUMI. Jeśli jesteś głównym opiekunem, rola przejdzie na osobę z rodziny;
          gdy nikogo innego nie ma, LUMI i jego dane też zostaną usunięte. <a href="/prywatnosc">Co przechowujemy</a>
        </p>
        <form action={deleteAccount}>
          <label htmlFor="confirm">Wpisz USUŃ, żeby potwierdzić</label>
          <input id="confirm" name="confirm" type="text" autoComplete="off" />
          <button className="btn danger block" style={{ marginTop: 12 }}>Usuń konto na stałe</button>
        </form>
      </details>
    </main>
  );
}
