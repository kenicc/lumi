# LUMI Pocket

Kieszonkowe urządzenie dla seniora i osoby z niepełnosprawnością: rozmowy przez kartę SIM, radio FM i internetowe, muzyka od rodziny, zegar i pogoda. Rodzina ustawia wszystko zdalnie, użytkownik niczego nie konfiguruje.

- [`firmware/`](firmware/) - oprogramowanie urządzenia (ESP32-S3, PlatformIO, LVGL 9); prototyp z części z Allegro: [`firmware/docs/prototyp-allegro.md`](firmware/docs/prototyp-allegro.md)
- [`app/`](app/) - aplikacja rodziny (PWA, Next.js + Supabase)
- [`shared/config-contract.json`](shared/config-contract.json) - wspólny dokument konfiguracji, sprawdzany testami po obu stronach
