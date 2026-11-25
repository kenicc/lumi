import Link from "next/link";
import { redirect } from "next/navigation";
import { signOut } from "./actions";
import { acceptInvites, getProfile, requireUser } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";
import { batteryText, isOnline } from "@/lib/status";
import { ROLE_NAMES, type DeviceRow, type Role } from "@/lib/types";
import { ago } from "@/components/ui";
import { PushToggle } from "@/components/PushToggle";

export default async function Home() {
  const user = await requireUser("/");
  await acceptInvites(user.id, user.email);
  const profile = await getProfile(user.id);
  if (!profile) redirect("/profil?next=/");
  const db = admin();
  const { data: mem } = await db.from("members").select("device_id, role").eq("user_id", user.id);
  const rows = (mem ?? []) as { device_id: string; role: Role }[];
  const { data: devs } = rows.length
    ? await db.from("devices").select("*").in("id", rows.map((r) => r.device_id))
    : { data: [] };
  const devices = (devs ?? []) as DeviceRow[];

  return (
    <main>
      <div className="row">
        <h1 className="grow">Cześć, {profile!.display_name}</h1>
        <Link href="/profil" className="btn small">Profil</Link>
      </div>
      {devices.length === 0 && (
        <div className="card stack">
          <h2 style={{ marginTop: 0 }}>Dodaj pierwsze LUMI</h2>
          <p>Włącz LUMI i postępuj według ekranu:</p>
          <ol>
            <li>Zeskanuj aparatem telefonu pierwszy kod i podaj domowe Wi-Fi.</li>
            <li>Zeskanuj drugi kod - otworzy się ta aplikacja z gotowym parowaniem.</li>
          </ol>
          <p className="dim small">Ktoś inny już dodał LUMI? Poproś go o zaproszenie na Twój e-mail ({user.email}).</p>
        </div>
      )}
      {devices.map((d) => {
        const role = rows.find((r) => r.device_id === d.id)!.role;
        const online = isOnline(d);
        return (
          <Link key={d.id} href={`/d/${d.id}`} className="card row" style={{ textDecoration: "none" }}>
            <div className="grow">
              <b style={{ fontSize: 21 }}>{d.name ?? d.id}</b>
              <div className="dim small">{ROLE_NAMES[role]} · ostatni kontakt {ago(d.last_seen)}</div>
            </div>
            <div style={{ textAlign: "right" }}>
              <span className={`badge ${online ? "ok" : "bad"}`}>{online ? "w sieci" : "poza siecią"}</span>
              <div className="small dim">bateria {batteryText(d)}</div>
            </div>
          </Link>
        );
      })}
      {devices.length > 0 && <PushToggle compact />}
      <form action={signOut} style={{ marginTop: 32 }}>
        <button className="btn block">Wyloguj</button>
      </form>
    </main>
  );
}
