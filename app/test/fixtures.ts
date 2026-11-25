import { defaultSettings, type Settings } from "@/lib/schema";

export function sample(): Settings {
  const s = defaultSettings();
  s.contacts = [
    { id: "ania", name: "Ania", number: "+48 600 100 200", color: "#3D8BFD", autoAnswer: true },
    { id: "basia", name: "Basia", number: "601200300", color: "#C2185B", autoAnswer: false },
  ];
  s.home.slots = ["contact:ania", "media", "weather"];
  s.media = [
    { type: "fm", name: "Jedynka", freq: 99.4 },
    { type: "web", name: "Polskie Radio Chopin", url: "http://stream.polskieradio.pl/chopin" },
    { type: "playlist", name: "Piosenki od Ani", from: "Ania", urls: ["https://example.com/a.mp3"] },
  ];
  s.weather = { place: "Kraków", lat: 50.06, lon: 19.94 };
  s.access.locked = ["music"];
  s.wifi = [{ ssid: "Dom", pass: "haslo123" }];
  s.reminders = [
    { id: "leki", text: "Weź leki", time: "08:00", days: [1, 2, 3, 4, 5], confirm: true },
    { id: "woda", text: "Napij się wody", time: "14:30", days: [], confirm: false },
  ];
  return s;
}
