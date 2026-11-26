import { addWifi, deleteWifi } from "../actions";
import { requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, Top, type SP } from "@/components/ui";

export default async function Wifi({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "wifi");
  const s = currentSettings(device.settings);
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Wi-Fi</h1>
      <p className="dim">
        Dodaj sieci, z którymi LUMI ma się łączyć, np. u Ciebie w domu przed oddaniem urządzenia. Sieć podana przy pierwszym
        uruchomieniu jest zapisana w samym LUMI i tu jej nie widać.
      </p>
      <Flash ok={ok} err={err} />
      <ul className="list card">
        {s.wifi.length === 0 && <li className="dim" style={{ padding: 8 }}>Brak dodatkowych sieci.</li>}
        {s.wifi.map((w) => (
          <li key={w.ssid} className="row" style={{ padding: "10px 0" }}>
            <b className="grow">{w.ssid}</b>
            <form action={deleteWifi.bind(null, id, w.ssid)}><button className="btn small danger">Usuń</button></form>
          </li>
        ))}
      </ul>
      <form action={addWifi.bind(null, id)} className="card">
        <label htmlFor="ssid">Nazwa sieci</label>
        <input id="ssid" name="ssid" type="text" required maxLength={32} autoCapitalize="none" />
        <label htmlFor="pass">Hasło</label>
        <input id="pass" name="pass" type="password" maxLength={64} />
        <button className="btn primary block" style={{ marginTop: 12 }}>Dodaj sieć</button>
      </form>
    </main>
  );
}
