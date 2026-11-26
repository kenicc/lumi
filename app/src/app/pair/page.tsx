import { claim } from "../actions";
import { getProfile, requireUser } from "@/lib/access";
import { Flash } from "@/components/ui";
import { redirect } from "next/navigation";

// Otwierane aparatem telefonu z kodu QR na ekranie LUMI: /pair?d=LUMI-XXXXXX&c=KOD
export default async function Pair({ searchParams }: { searchParams: Promise<{ d?: string; c?: string; err?: string }> }) {
  const { d = "", c = "", err } = await searchParams;
  const back = `/pair?d=${encodeURIComponent(d)}&c=${encodeURIComponent(c)}`;
  const user = await requireUser(back);
  const profile = await getProfile(user.id);
  if (!profile) redirect(`/profil?next=${encodeURIComponent(back)}`);
  return (
    <main>
      <h1>Dodaj LUMI</h1>
      <Flash err={err} />
      <form action={claim} className="card">
        <label htmlFor="d">Numer urządzenia</label>
        <input id="d" name="d" type="text" defaultValue={d} placeholder="LUMI-XXXXXX" required />
        <label htmlFor="c">Kod z ekranu LUMI</label>
        <input id="c" name="c" type="text" defaultValue={c} autoCapitalize="characters" required />
        <label htmlFor="name">Kto będzie używać LUMI?</label>
        <input id="name" name="name" type="text" maxLength={40} placeholder="np. Mama, Dziadek Józef" />
        <label htmlFor="phone">Twój numer telefonu (opcjonalnie)</label>
        <input id="phone" name="phone" type="tel" placeholder="+48 600 100 200" />
        <p className="dim small">
          Dodamy Cię jako pierwszy kontakt i przycisk „{profile!.display_name}” na ekranie głównym LUMI.
        </p>
        <button className="btn primary block">Dodaj jako główny opiekun</button>
      </form>
    </main>
  );
}
