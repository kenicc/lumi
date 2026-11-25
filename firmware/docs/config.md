# Konfiguracja urządzenia (JSON)

Jeden dokument z numerem wersji. Rodzina zmienia go w aplikacji, urządzenie pobiera nową wersję i pokazuje, kto co zmienił. Dokument przygotowuje aplikacja rodziny (`app/`), a urządzenie pobiera go z `/api/device/config`. Bez aplikacji można go wgrać przez `pio run -t uploadfs` albo wkleić w konsoli komendą `config`.

Przykład: [`data/config.json`](../data/config.json). Brakujące pola dostają wartości domyślne, a błędny kontakt czy stacja są pomijane, nie psują reszty. Nieczytelny dokument jest odrzucany i zostaje poprzednia wersja (`/config.bak`).

| Pole | Domyślnie | Znaczenie |
| --- | --- | --- |
| `version` | 0 | rośnie przy każdej zmianie; urządzenie bierze tylko wyższą |
| `changedBy`, `changeText` | - | komunikat "Ania dodała kontakt Basia". Zdanie składa aplikacja, bo zna rodzaj gramatyczny; bez `changeText` urządzenie pisze "Ustawienia zmienione przez: Ania" |
| `features.calls / media / weather` | true | widoczne funkcje |
| `contacts[]` | - | `id`, `name`, `number`, `color` (#RRGGBB kółka z inicjałem), `autoAnswer`, `photo` (skrót zdjęcia; LUMI pobiera je z `/api/device/photo`) |
| `calls.onlyContacts` | false | odrzucaj numery spoza listy (ochrona przed oszustami "na wnuczka") |
| `home.slots[3]` | - | co jest przy 3 prawych przyciskach na ekranie głównym: `contact:<id>`, `media`, `weather` albo pusty |
| `home.partOfDay` | true | "Wtorek wieczorem" zamiast samego "Wtorek" |
| `media[]` | - | `{type:"fm", name, freq}`, `{type:"web", name, url}`, `{type:"playlist", name, from, urls[]}` |
| `weather.place / lat / lon` | Warszawa | współrzędne podaje aplikacja |
| `display.textSize` | 0 | 0 normalny, 1 duży, 2 bardzo duży |
| `display.highContrast` | false | biel na czerni, grube obramowania |
| `display.brightness` | 80 | % (w trybie FM podświetlenie świeci bez PWM) |
| `display.screenOffSec` | 60 | wygaszanie ekranu |
| `sound.ringVolume / maxVolume / startVolume` | 8 / 10 / 5 | 0-10; rolka nie przekroczy `maxVolume` |
| `sound.keyClick` | true | dźwięk przy naciśnięciu |
| `sound.quiet.from / to` | - | godziny ciszy "22:00"-"07:00": dzwonek cichy, ale nigdy wyłączony |
| `access.autoAnswerSec` | 5 | 3-10 s, dla kontaktów z `autoAnswer` |
| `access.oneButton` | false | tryb jednego przycisku: środkowy przechodzi po opcjach, przytrzymanie wybiera |
| `access.minPressMs` | 0 | drżenie dłoni: krótsze dotknięcia są ignorowane (do 1500) |
| `access.repeatIgnoreMs` | 250 | drżenie dłoni: powtórzenia tuż po naciśnięciu są ignorowane |
| `access.locked[]` | [] | `"music"`, `"call"` - wyłączone przyciski |
| `access.allowPowerOff` | true | false = włącznik nie wyłącza (osoba, która przypadkiem wyłącza telefon) |
| `wifi[]` | - | `ssid`, `pass` |
| `reminders[]` | [] | do 20 przypomnień: `id`, `text` (do 60 znaków), `time` "08:00", `days` [1-7, 1 = pn; pusta = codziennie], `confirm` (czekaj na "Zrobione"; bez potwierdzenia w 10 min rodzina dostaje powiadomienie) |
| `sync.intervalSec` | 20 | jak często LUMI pyta serwer aplikacji o zmiany (10-3600 s) |
