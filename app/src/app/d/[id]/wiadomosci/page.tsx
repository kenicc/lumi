import { sendMessage } from "../actions";
import { requireMember } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";
import { AutoRefresh } from "@/components/AutoRefresh";
import { Top, ago, type SP } from "@/components/ui";

const hm = (iso: string) => new Date(iso).toLocaleTimeString("pl-PL", { hour: "2-digit", minute: "2-digit", timeZone: "Europe/Warsaw" });

export default async function Messages({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "messages");
  const { data } = await admin()
    .from("messages")
    .select("id, author, text, created_at, read_at")
    .eq("device_id", id)
    .order("id", { ascending: false })
    .limit(20);
  const list = data ?? [];
  return (
    <main>
      <AutoRefresh />
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Wiadomości</h1>
      <p className="dim">Krótki tekst pojawi się na LUMI na cały ekran. Gdy ktoś naciśnie „Dobrze”, zobaczysz tu godzinę przeczytania. Jeśli jesteś w kontaktach, LUMI pozwoli od razu do Ciebie oddzwonić.</p>
      {ok === "1" && <div className="flash ok" role="status">Wysłano. LUMI pokaże wiadomość zwykle w kilkanaście sekund, najpóźniej w ciągu minuty.</div>}
      {err && <div className="flash err" role="alert">{err}</div>}
      <form action={sendMessage.bind(null, id)} className="card">
        <label htmlFor="text">Wiadomość</label>
        <input id="text" name="text" type="text" required maxLength={120} placeholder="np. Przyjadę o 15, nie gotuj obiadu" autoComplete="off" />
        <p className="dim small">Najwyżej 120 znaków - krótko i dużymi literami czyta się najłatwiej.</p>
        <button className="btn primary block">Wyślij na LUMI</button>
      </form>
      <ul className="list card">
        {list.length === 0 && <li className="dim" style={{ padding: 8 }}>Jeszcze nie było wiadomości.</li>}
        {list.map((m) => (
          <li key={m.id} style={{ padding: "10px 0" }}>
            <b>{m.author}:</b> {m.text}
            <div className="small">
              <span className="dim">{ago(m.created_at)} · </span>
              {m.read_at ? <span className="badge ok">przeczytane {hm(m.read_at)}</span> : <span className="badge">czeka na przeczytanie</span>}
            </div>
          </li>
        ))}
      </ul>
    </main>
  );
}
