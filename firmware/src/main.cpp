// LUMI Pocket - oprogramowanie prototypu (ESP32-S3)
//
// Zadania:
//   rdzeń 0: pętla główna (interfejs LVGL, przyciski, logika), sieć (Wi-Fi, pogoda, konfiguracja)
//   rdzeń 1: modem (rozmowy), audio (radio, muzyka, dzwonek)
// Interfejs nigdy nie blokuje rozmowy ani dźwięku: modem i audio tylko wysyłają zdarzenia.
#include "BuildConfig.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#include "app/App.h"
#include "app/Console.h"
#include "app/Diag.h"
#include "app/Events.h"
#include "hw/Battery.h"
#include "hw/Board.h"
#include "hw/Display.h"
#include "hw/Input.h"
#include "hw/Rtc.h"
#include "net/Ota.h"
#include "net/Panel.h"

// Ślad pętli głównej w pamięci RTC (przetrwa restart przez strażnika): po zawieszeniu wiadomo,
// w którym kroku pętla utknęła - bez tego raport strażnika pokazuje tylko "IDLE".
RTC_NOINIT_ATTR static volatile uint32_t loopStep;
static const char* const kStepNames[] = {"-", "przyciski", "bateria", "konsola", "logika", "ekran (przed oszczedzaniem)",
                                         "zmiana taktowania", "ekran", "przerwa"};
static void step(uint32_t s) { loopStep = 0x4C550000u | s; }  // "LU" + numer: odróżnia ślad od śmieci po zasilaniu

static void reportHang() {
  if (esp_reset_reason() != ESP_RST_TASK_WDT && esp_reset_reason() != ESP_RST_WDT) return;
  uint32_t s = loopStep;
  if ((s & 0xFFFF0000u) != 0x4C550000u) return;
  s &= 0xFFFF;
  Serial.printf("[diag] petla glowna zawisla w kroku: %s\n", s < sizeof kStepNames / sizeof kStepNames[0] ? kStepNames[s] : "?");
  diag::error("[diag] zawieszenie w kroku %u (%s)", (unsigned)s,
              s < sizeof kStepNames / sizeof kStepNames[0] ? kStepNames[s] : "?");
}

// Po wybudzeniu przyciskiem: włącz dopiero po przytrzymaniu (przypadkowe naciśnięcie w torbie nie włącza)
static void requireHoldToPowerOn() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
  pinMode(pins::BTN_POWER, INPUT_PULLUP);
  uint32_t start = millis();
  while (digitalRead(pins::BTN_POWER) == LOW) {
    if (millis() - start > 1500) return;
    delay(10);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pins::BTN_POWER, 0);
  esp_deep_sleep_start();
}

void setup() {
  requireHoldToPowerOn();
  Serial.begin(115200);
  ota::begin();  // przed resztą: jeśli nowa wersja nie wstaje, wróć do poprzedniej
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  // Etykieta musi zgadzać się z partitions.csv - domyślnie biblioteka szuka "spiffs" i nie montuje nic
  // Bez formatowania na ślepo: jeśli montowanie zawiedzie, powiedz o tym, zanim pamięć zostanie wyczyszczona
  if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("[fs] pamiec ustawien pusta lub uszkodzona - formatuje (wgraj ustawienia: Upload Filesystem Image)");
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) Serial.println("[fs] LittleFS niedostepny");
  }

  diag::begin();
  reportHang();
  bus::begin();
  rtc::begin();  // godzina z zegara z baterią (jeśli jest) - zanim cokolwiek pokaże czas
  display::begin();
  input::begin();
  battery::begin();
  app::begin();
  panel::begin();
  // Strażnik: jeśli pętla główna zawiśnie na 15 s, urządzenie samo się zrestartuje
  // (u seniora nikt nie naciśnie RESET, a zawieszone LUMI nie odbierze rozmowy)
  esp_task_wdt_init(15, true);
  esp_task_wdt_add(nullptr);
  Serial.printf("LUMI %s gotowe. Wpisz help.\n", LUMI_VARIANT);
}

// Oszczędzanie energii: gdy LUMI nic nie robi (ekran zgaszony, bez rozmowy i muzyki), procesor
// zwalnia do 80 MHz, a ekran nie jest odświeżany. Przyciski, rolka i modem działają normalnie -
// pierwsze naciśnięcie budzi urządzenie w ułamku sekundy.
static bool lowPower = false;

void loop() {
  step(1);
  input::poll();
  step(2);
  battery::poll();
  step(3);
  console::poll();
  step(4);
  app::loop();

  bool idle = app::canSlowDown();
  if (idle != lowPower) {
    lowPower = idle;
    step(5);
    if (lowPower) display::tick();  // dokończ ostatnią klatkę, zanim przestaniemy odświeżać
    step(6);
    setCpuFrequencyMhz(lowPower ? 80 : 240);
  }
  step(7);
  if (!lowPower) display::tick();
  esp_task_wdt_reset();
  step(8);
  delay(lowPower ? 20 : 5);
}
