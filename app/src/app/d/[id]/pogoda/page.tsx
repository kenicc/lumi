import { saveWeather } from "../actions";
import { PlaceSearch } from "./PlaceSearch";
import { requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, Top, type SP } from "@/components/ui";

export default async function Weather({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "weather");
  const s = currentSettings(device.settings);
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Pogoda</h1>
      <p className="dim">LUMI pokazuje temperaturę na dziś i jedną radę na jutro, np. „Weź parasol”. Wybierz miejscowość z listy - zapisze się od razu.</p>
      <Flash ok={ok} err={err} />
      <form action={saveWeather.bind(null, id)} className="card">
        <PlaceSearch place={s.weather.place} lat={s.weather.lat} lon={s.weather.lon} />
      </form>
    </main>
  );
}
