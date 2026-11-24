// LUMI Pocket - oprogramowanie prototypu (ESP32-S3)
//
// Zadania:
//   rdzeń 0: pętla główna (interfejs LVGL, przyciski, logika), sieć (Wi-Fi, pogoda, konfiguracja)
//   rdzeń 1: modem (rozmowy), audio (radio, muzyka, dzwonek)
// Interfejs nigdy nie blokuje rozmowy ani dźwięku: modem i audio tylko wysyłają zdarzenia.
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_sleep.h>

#include "app/App.h"
#include "app/Console.h"
#include "app/Events.h"
#include "hw/Battery.h"
#include "hw/Board.h"
#include "hw/Display.h"
#include "hw/Input.h"

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
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  if (!LittleFS.begin(true)) Serial.println("[fs] LittleFS niedostepny");

  bus::begin();
  display::begin();
  input::begin();
  battery::begin();
  app::begin();
  Serial.printf("LUMI %s gotowe. Wpisz help.\n", LUMI_VARIANT);
}

void loop() {
  input::poll();
  battery::poll();
  console::poll();
  app::loop();
  display::tick();
  delay(5);
}
