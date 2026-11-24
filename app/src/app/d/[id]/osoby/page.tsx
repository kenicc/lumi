import { cancelInvite, deleteDevice, invite, removeMember, renameDevice } from "../actions";
import { requireMember } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";
import { ROLE_NAMES, type Role } from "@/lib/types";
import { Flash, Top, type SP } from "@/components/ui";

export default async function People({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { user, device } = await requireMember(id, "people");
  const db = admin();
  const { data: mem } = await db.from("members").select("user_id, role").eq("device_id", id);
  const members = (mem ?? []) as { user_id: string; role: Role }[];
  const { data: prof } = members.length
    ? await db.from("profiles").select("user_id, display_name").in("user_id", members.map((m) => m.user_id))
    : { data: [] };
  const names = new Map(((prof ?? []) as { user_id: string; display_name: string }[]).map((p) => [p.user_id, p.display_name]));
  const { data: inv } = await db.from("invites").select("id, email, role").eq("device_id", id);
  const invites = (inv ?? []) as { id: string; email: string; role: Role }[];

  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Osoby</h1>
      <Flash ok={ok} err={err} />
      <ul className="list card">
        {members.map((m) => (
          <li key={m.user_id} className="row" style={{ padding: "10px 0" }}>
            <div className="grow">
              <b>{names.get(m.user_id) ?? "?"}</b>{m.user_id === user.id && " (Ty)"}
              <div className="dim small">{ROLE_NAMES[m.role]}</div>
            </div>
            {m.user_id !== user.id && (
              <form action={removeMember.bind(null, id, m.user_id)}><button className="btn small danger">Usuń</button></form>
            )}
          </li>
        ))}
        {invites.map((i) => (
          <li key={i.id} className="row" style={{ padding: "10px 0" }}>
            <div className="grow">
              {i.email}
              <div className="dim small">{ROLE_NAMES[i.role]} · zaproszenie czeka na pierwsze logowanie</div>
            </div>
            <form action={cancelInvite.bind(null, id, i.id)}><button className="btn small">Anuluj</button></form>
          </li>
        ))}
      </ul>

      <form action={invite.bind(null, id)} className="card">
        <h2 style={{ marginTop: 0 }}>Zaproś</h2>
        <label htmlFor="email">E-mail</label>
        <input id="email" name="email" type="email" required />
        <label htmlFor="role">Rola</label>
        <select id="role" name="role" defaultValue="family">
          <option value="family">Rodzina - kontakty, ekran, radio, wygląd</option>
          <option value="friend">Znajomi - tylko radio i muzyka</option>
        </select>
        <p className="dim small">Osoba loguje się w tej aplikacji tym e-mailem i od razu widzi LUMI.</p>
        <button className="btn primary block">Zaproś</button>
      </form>

      <form action={renameDevice.bind(null, id)} className="card">
        <label htmlFor="name">Nazwa LUMI w aplikacji</label>
        <input id="name" name="name" type="text" maxLength={40} defaultValue={device.name ?? ""} />
        <button className="btn block" style={{ marginTop: 12 }}>Zmień nazwę</button>
      </form>

      <details className="card">
        <summary>Usuń LUMI z aplikacji</summary>
        <p className="dim small">
          Wszystkie osoby stracą dostęp. LUMI działa dalej na ostatnich ustawieniach. Żeby dodać je ponownie, przytrzymaj na
          urządzeniu przycisk Home przez 5 sekund i zeskanuj kod.
        </p>
        <form action={deleteDevice.bind(null, id)}>
          <label htmlFor="confirm">Wpisz {id}, żeby potwierdzić</label>
          <input id="confirm" name="confirm" type="text" autoCapitalize="characters" />
          <button className="btn danger block" style={{ marginTop: 12 }}>Usuń na stałe</button>
        </form>
      </details>
    </main>
  );
}
