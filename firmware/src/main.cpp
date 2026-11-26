// LUMI Pocket - oprogramowanie prototypu (ESP32-S3)
//
// Zadania:
//   rdzeń 0: pętla główna (interfejs LVGL, przyciski, logika), sieć (Wi-Fi, pogoda, konfiguracja)
//   rdzeń 1: modem (rozmowy), audio (radio, muzyka, dzwonek)
// Interfejs nigdy nie blokuje rozmowy ani dźwięku: modem i audio tylko wysyłają zdarzenia.
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
#include "net/Ota.h"

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
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) Serial.println("[fs] LittleFS niedostepny");

  diag::begin();
  bus::begin();
  display::begin();
  input::begin();
  battery::begin();
  app::begin();
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
  input::poll();
  battery::poll();
  console::poll();
  app::loop();

  bool idle = app::canSlowDown();
  if (idle != lowPower) {
    lowPower = idle;
    if (lowPower) display::tick();  // dokończ ostatnią klatkę, zanim przestaniemy odświeżać
    setCpuFrequencyMhz(lowPower ? 80 : 240);
  }
  if (!lowPower) display::tick();
  esp_task_wdt_reset();
  delay(lowPower ? 20 : 5);
}
