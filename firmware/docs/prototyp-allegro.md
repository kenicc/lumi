# Prototyp z koszyka Allegro

Środowisko `allegro` w `platformio.ini`. Wgrywanie: `pio run -e allegro -t upload` i `pio run -e allegro -t uploadfs`.

## Czego brakuje w koszyku

Z samym koszykiem LUMI **nie zadzwoni i nie zagra**. Trzeba dokupić:

| Część | Po co | Ok. cena |
| --- | --- | --- |
| Głośnik 8 Ω 0,5-1 W (np. 28-40 mm) | dzwonek, głos rozmówcy, sygnały przycisków (podłączany do SIM800L) | 4-8 zł |
| Mikrofon elektretowy 6-9 mm | rozmowa (podłączany do SIM800L) | 1-2 zł |
| Ogniwo 18650 z zabezpieczeniem | zasilanie SIM800L (koszyczek jest, ogniwa nie ma) | 15-25 zł |
| Adapter nano → micro SIM | SIM800L ma gniazdo micro SIM | 2 zł |
| Karta SIM z 2G | Orange lub T-Mobile (2G zostaje tam najdłużej, ok. 2028-2030) | - |

Do radia i muzyki (później, wersja z dokumentacji): RDA5807M, PCM5102A, PAM8302. Proponowany w "Dorzuć do przesyłki" PAM8403 to wzmacniacz stereo na 5 V z potencjometrem, więc do LUMI nie pasuje.

## Co ten prototyp sprawdza, a czego nie

- **Sprawdza:** cały interfejs na docelowym ekranie 2,8", przyciski, rolkę, rozmowy (wybieranie, odbieranie, nieodebrane, automatyczne odbieranie), parowanie z aplikacją rodziny, zdalną konfigurację, pogodę i zegar.
- **Nie sprawdza VoLTE.** SIM800L działa tylko w 2G. Etap 1 z dokumentacji (największe ryzyko projektu: czy modem rozmawia przez LTE) **zostaje otwarty**. Najtańsze domknięcie to płytka z A7670E (KAmod) albo EG915U. Firmware obsługuje oba modemy bez zmian w kodzie, a modem rozpoznaje się sam.
- **Nie gra radia ani muzyki.** ESP32-S3 nie ma przetwornika DAC, a w koszyku nie ma ani DAC, ani tunera. Przycisk Muzyka pokazuje komunikat, rolka poza rozmową ustawia głośność dzwonka.

## Połączenia

ESP32-S3 DevKitC-1 zasilany na razie z USB komputera. Stabilizator na DevKicie potrzebuje ok. 4,4 V, więc z ogniwa 18650 (3,0-4,2 V) nie ruszy. Do noszenia trzeba dodać stabilizator LDO 3,3 V (HT7333 / ME6211) na pin 3V3 albo przetwornicę 5 V na pin 5V.

| Moduł | Pin modułu | Gdzie |
| --- | --- | --- |
| **SIM800L** | VCC | + ogniwa (wyjście OUT+ z TP4056), **krótki gruby przewód** |
| | GND | - ogniwa i GND ESP32 (wspólna masa!) |
| | VCC-GND | 2 x 1000 µF równolegle, jak najbliżej modułu |
| | RXD | GPIO47 przez dzielnik: GPIO47 - 10 kΩ - RXD - 20 kΩ - GND (SIM800L ma logikę 2,8 V) |
| | TXD | GPIO48 (DevKit v1.1) albo GPIO38 (v1.0, flaga `LUMI_DEVKIT_V10`) |
| | RST | GPIO42 |
| | RING | GPIO3 |
| | DTR | GPIO38 (v1.1) albo GPIO48 (v1.0) |
| | SPK+ / SPK- | głośnik 8 Ω |
| | MIC+ / MIC- | mikrofon elektretowy (+ do MIC+) |
| **Ekran 2,8" ST7789** | VCC / GND | 3V3 / GND |
| | CS / DC / RESET | GPIO10 / GPIO9 / EN (pin RST na DevKicie) |
| | SDI (MOSI) / SCK | GPIO11 / GPIO12 |
| | LED | GPIO13 |
| | SDO | niepodłączony |
| **Przyciski 6x6** (drugą nóżką do GND) | prawy górny / środkowy / dolny | GPIO15 / 16 / 17 |
| | Muzyka / Home / Dzwoń | GPIO18 / 21 / 39 |
| | włącznik | GPIO2 |
| **Enkoder** | CLK / DT | GPIO40 / GPIO41 |
| | + / GND | **3V3** (nie 5 V - moduł ma rezystory podciągające do swojego zasilania) / GND |
| **TP4056** | B+ / B- | koszyczek 18650 |
| | OUT+ / OUT- | SIM800L VCC / GND |

Poziom baterii odczytuje SIM800L (`AT+CBC`), więc dzielnik na GPIO1 nie jest potrzebny.

## Pułapki

- **SIM800L resetuje się przy logowaniu do sieci.** Pobiera wtedy impulsy do 2 A. Cienkie przewody i styki płytki stykowej powodują spadek napięcia, a dioda NET mruga szybko w kółko. Kondensatory trzeba przylutować przy module, a VCC i GND poprowadzić krótkim grubszym przewodem prosto z ogniwa, z pominięciem płytki stykowej.
- **ESP32-S3 DevKitC na płytce 830 pól** zostawia tylko jeden rząd wolnych otworów z każdej strony. Wygodniej użyć dwóch płytek stykowych obok siebie.
- **Ekran pokazuje negatyw.** Zmień `-DLUMI_PANEL_INVERT=0` na `1` w środowisku `allegro`.
- **Obraz jest obrócony.** Ustaw `LUMI_ROTATION` (0-3).
- **Dzwonek SIM800L** gra z głośnika modemu. Głośność ustawia aplikacja (`ringVolume`) albo rolka, a "Cisza" go wycisza.

## Kolejność uruchamiania

1. Najpierw sam SIM800L, ogniwo i ESP32: `pio device monitor`, potem `status` i `diag`. Modem ma zgłosić się jako "SIM800 (2G)", a `+CREG: 0,1` oznacza, że jest w sieci.
2. `call <twój numer>`: rozmowa z głośnika i mikrofonu.
3. Ekran, przyciski i enkoder. Bez przycisków interfejs da się klikać z konsoli: `key call`, `key mid`, `wheel 1`.
4. Wi-Fi i aplikacja: `server https://<adres aplikacji>`, potem kroki z ekranu LUMI (kod Wi-Fi, następnie kod parowania).
