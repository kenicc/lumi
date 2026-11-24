# LUMI - aplikacja rodziny

Aplikacja webowa instalowana na telefonie jak zwykła aplikacja (PWA): jedna dla Androida, iPhone'a i komputera, bez sklepów. Next.js 15 + Supabase (logowanie, baza, pliki muzyki).

## Co umie

- **Logowanie kodem z e-maila**, bez hasła. Kod zamiast linku, bo link na iPhonie otwiera Safari, a nie zainstalowaną aplikację.
- **Parowanie**: telefon skanuje zwykłym aparatem kod QR z ekranu LUMI i od razu otwiera `/pair`. Pierwsza osoba zostaje głównym opiekunem, a jej numer może od razu trafić na przycisk ekranu głównego.
- **Ustawienia**: kontakty z kolejnością i auto-odbieraniem, trzy przyciski ekranu głównego (z podglądem), radio FM i internetowe, wgrywanie MP3 z telefonu, wielkość tekstu, kontrast, głośność, godziny ciszy, dostępność (drżenie dłoni, tryb jednego przycisku, blokady, odbieranie tylko od kontaktów), pogoda z wyszukiwarką miejscowości, sieci Wi-Fi.
- **Role**: główny opiekun ma dostęp do wszystkiego; rodzina - do kontaktów, ekranu, radia, wyglądu, pogody i Wi-Fi; znajomi - tylko do radia i muzyki. Zaprasza się e-mailem.
- **Stan urządzenia**: bateria, zasięg, ostatni kontakt, czy ostatnia zmiana już dotarła. Bez lokalizacji i historii słuchania.
- **Historia zmian**: każde zapisanie składa zdanie, które LUMI pokaże użytkownikowi, np. „Ania dodała kontakt Basia”. Formę czasownika każdy wybiera w profilu.

## Jak to działa

```
telefon (PWA) --zapis--> serwer Next.js --> Supabase (ustawienia, wersja, historia)
LUMI --co 20 s, HTTPS + sekret urządzenia--> /api/device/config  (304 albo nowy dokument)
LUMI --co 5 min i po zmianie-------------> /api/device/status
```

- Każdy zapis przechodzi przez `src/lib/store.ts`. Ten moduł sprawdza rolę, waliduje dane tym samym zakresem co firmware (`src/lib/schema.ts`), podbija wersję i chroni przed nadpisaniem cudzej zmiany.
- Kontrakt z firmware sprawdzają testy: `shared/config-contract.json` generuje test aplikacji, a parsuje test firmware (`pio test -e native`).
- RLS jest włączone bez żadnych zasad: przeglądarka nie ma dostępu do tabel, wszystko idzie przez serwer z kluczem serwisowym.

## Uruchomienie

1. **Supabase**: utwórz projekt i w SQL Editor uruchom `supabase/schema.sql`.
2. **Logowanie kodem**: Authentication -> Email Templates -> "Magic Link". W treści szablonu musi być `{{ .Token }}`, np. „Twój kod do LUMI: {{ .Token }}”. Do testów wystarczy wbudowana poczta Supabase, w produkcji trzeba podpiąć własny SMTP.
3. **Zmienne**: skopiuj `.env.example` do `.env.local` i uzupełnij wartości z Project Settings -> API.
4. **Lokalnie**: `npm install && npm run dev`.
5. **Wdrożenie**: Vercel -> import repozytorium, katalog `app`, te same zmienne środowiskowe.
6. **Urządzenie**: wpisz adres aplikacji w `firmware/platformio.ini` (`-DLUMI_SERVER=\"https://...\"`). Można go też podać na stronie 192.168.4.1 przy konfiguracji Wi-Fi albo w konsoli: `server https://...`.

Na telefonie: otwórz adres aplikacji, potem "Dodaj do ekranu początkowego" (iPhone: Udostępnij -> Do ekranu początkowego).

```bash
npm test          # schemat, kontrakt z firmware, zdania o zmianach, protokół urządzenia
npm run build
```

## Świadomie na później

- **Powiadomienie urządzenia w kilka sekund** (MQTT albo Supabase Realtime). Teraz LUMI pyta co 20 s.
- **Propozycje kontaktów od znajomych** do akceptacji opiekuna.
- **Fabryczne klucze urządzeń.** Teraz urządzenie rejestruje swój sekret przy pierwszym połączeniu (zaufanie przy pierwszym użyciu).
- **Hasła Wi-Fi w bazie** leżą jawnie w ustawieniach, bo urządzenie musi je dostać. Dostęp do nich ma tylko serwer, ale w produkcji warto je szyfrować.
- **Ograniczanie liczby zapytań** do API urządzenia.
