# LUMI Pocket

[![CI](https://github.com/kenicc/lumi/actions/workflows/ci.yml/badge.svg?branch=claude/prototyp-allegro)](https://github.com/kenicc/lumi/actions/workflows/ci.yml)

Kieszonkowe urządzenie dla seniora i osoby z niepełnosprawnością: rozmowy przez kartę SIM, radio FM i internetowe, muzyka od rodziny, zegar i pogoda. Rodzina ustawia wszystko zdalnie z telefonu, a użytkownik niczego nie konfiguruje.

| Katalog | Co to jest |
| --- | --- |
| [`firmware/`](firmware/) | oprogramowanie urządzenia: ESP32-S3, PlatformIO, LVGL 9; prototyp z części z Allegro: [`firmware/docs/prototyp-allegro.md`](firmware/docs/prototyp-allegro.md) |
| [`app/`](app/) | aplikacja rodziny: PWA, Next.js + Supabase; da się ją wypróbować bez serwera w trybie demo |
| [`shared/config-contract.json`](shared/config-contract.json) | wspólny dokument konfiguracji, sprawdzany testami po obu stronach |

## Co już działa

| Dla użytkownika (LUMI) | Dla rodziny (aplikacja) |
| --- | --- |
| rozmowy: SIM800L (2G) w prototypie, A7670E i EG915U (VoLTE) docelowo | parowanie kodem QR z ekranu LUMI |
| kontakty ze zdjęciami, szybkie wybieranie z potwierdzeniem | kontakty, zdjęcia, ekran główny, radio, wygląd, dostępność |
| przypomnienia (leki, posiłki) z "Zrobione" i "Później" | przypomnienia i sekcja "Dziś" z potwierdzeniami |
| wiadomości od rodziny z oddzwonieniem | wiadomości z godziną przeczytania |
| zegar z porą dnia, pogoda z jedną radą | powiadomienia push: bateria, brak zasięgu, brak połączenia, niepotwierdzone przypomnienie |
| duży tekst, wysoki kontrast, tryb jednego przycisku, filtr drżenia dłoni | role: opiekun, rodzina, znajomi; historia zmian ("Ania dodała kontakt Basia") |
| aktualizacje przez Wi-Fi z podpisem i powrotem do poprzedniej wersji | stan urządzenia i diagnostyka zdalna |

## Jak sprawdzamy zmiany

CI (`.github/workflows/ci.yml`) przy każdej zmianie:
- uruchamia testy logiki firmware, w tym kontrakt z aplikacją;
- buduje trzy warianty urządzenia;
- robi zrzuty ekranów LUMI w symulatorze (artefakt `ekrany-lumi`);
- sprawdza typy, testy i build aplikacji.

Na sprzęcie dotąd nic nie było uruchomione: kolejny krok to złożenie prototypu z koszyka i test rozmów.
