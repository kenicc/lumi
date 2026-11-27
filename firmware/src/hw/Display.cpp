#include "../BuildConfig.h"
#include "Display.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "Board.h"

namespace {

class Lgfx : public lgfx::LGFX_Device {
#if defined(LUMI_PANEL_ST7789)
  lgfx::Panel_ST7789 panel_;
#else
  lgfx::Panel_ILI9341 panel_;
#endif
  lgfx::Bus_SPI bus_;

 public:
  Lgfx() {
    auto b = bus_.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 40000000;
    b.freq_read = 16000000;
    b.spi_3wire = true;
    b.use_lock = true;
    b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk = pins::LCD_SCK;
    b.pin_mosi = pins::LCD_MOSI;
    b.pin_miso = -1;
    b.pin_dc = pins::LCD_DC;
    bus_.config(b);
    panel_.setBus(&bus_);

    auto p = panel_.config();
    p.pin_cs = pins::LCD_CS;
    p.pin_rst = -1;  // reset ekranu podłączony do EN
    p.pin_busy = -1;
    p.panel_width = 240;
    p.panel_height = 320;
    p.readable = false;
    p.bus_shared = false;
    p.invert = LUMI_PANEL_INVERT;
    panel_.config(p);
    setPanel(&panel_);
  }

  // Poprawki dla konkretnej sztuki ekranu (moduły z Allegro różnią się między partiami)
  void applyTweaks(const display::Tweaks& t) {
    auto p = panel_.config();
    p.invert = t.invert ? !LUMI_PANEL_INVERT : LUMI_PANEL_INVERT;
    p.rgb_order = t.bgr;
    panel_.config(p);
  }
};

Lgfx tft;
lv_display_t* disp = nullptr;
bool on = false;
display::Tweaks tweaks;

void loadTweaks() {
  Preferences p;
  if (!p.begin("lcd", true)) return;  // brak przestrzeni = ustawienia domyślne
  tweaks.invert = p.getBool("inv", false);
  tweaks.flip = p.getBool("flip", false);
  tweaks.bgr = p.getBool("bgr", false);
  p.end();
}

int rotation() { return tweaks.flip ? (LUMI_ROTATION + 2) % 4 : LUMI_ROTATION; }

constexpr int BL_CH = 0;
constexpr int BL_BITS = 8;
constexpr int BL_FREQ = 25000;  // powyżej pasma słyszalnego

constexpr int BUF_LINES = 40;

// Przesyłanie przez DMA w tle: gdy jeden bufor leci do ekranu, LVGL rysuje następny kawałek w drugim.
// Pełne przerysowanie ekranu jest wtedy wyraźnie krótsze, a procesor nie czeka bezczynnie na SPI.
bool dmaPending = false;

void finishDma() {
  if (!dmaPending) return;
  tft.waitDMA();
  tft.endWrite();
  dmaPending = false;
}

void flush(lv_display_t*, const lv_area_t* a, uint8_t* px) {
  uint32_t w = lv_area_get_width(a), h = lv_area_get_height(a);
  finishDma();
  lv_draw_sw_rgb565_swap(px, w * h);  // ekran oczekuje starszego bajtu najpierw; zamiana w miejscu, przed DMA
  tft.startWrite();
  tft.setAddrWindow(a->x1, a->y1, w, h);
  tft.writePixelsDMA(reinterpret_cast<uint16_t*>(px), w * h, false);
  dmaPending = true;
}

// LVGL woła to przed ponownym użyciem bufora; zamiast lv_display_flush_ready
void flushWait(lv_display_t*) { finishDma(); }

uint32_t tickMs() { return millis(); }

}  // namespace

namespace display {

void begin() {
  ledcSetup(BL_CH, BL_FREQ, BL_BITS);
  ledcAttachPin(pins::LCD_BL, BL_CH);
  ledcWrite(BL_CH, 0);

  loadTweaks();
  tft.applyTweaks(tweaks);
  tft.init();
  tft.setRotation(rotation());
  tft.fillScreen(0);
  Serial.printf("[lcd] ekran %dx%d, obrot %d%s%s\n", tft.width(), tft.height(), rotation(),
                tweaks.invert ? ", odwrocone kolory" : "", tweaks.bgr ? ", BGR" : "");

  lv_init();
  lv_tick_set_cb(tickMs);
  disp = lv_display_create(board::SCREEN_W, board::SCREEN_H);
  lv_display_set_flush_cb(disp, flush);
  lv_display_set_flush_wait_cb(disp, flushWait);
  const size_t bytes = board::SCREEN_W * BUF_LINES * 2;
  void* b1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* b2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void tick() { lv_timer_handler(); }

void setBacklight(uint8_t percent, bool steady) {
  if (percent > 100) percent = 100;
  uint32_t duty = (steady || percent == 100) ? (1u << BL_BITS) : (percent * ((1u << BL_BITS) - 1)) / 100;
  ledcWrite(BL_CH, duty);
  on = percent > 0;
}

void off() {
  ledcWrite(BL_CH, 0);
  on = false;
}

bool isOn() { return on; }

Tweaks currentTweaks() { return tweaks; }

void setTweaks(const Tweaks& t) {
  finishDma();
  Preferences p;
  p.begin("lcd", false);
  p.putBool("inv", t.invert);
  p.putBool("flip", t.flip);
  p.putBool("bgr", t.bgr);
  p.end();
  tweaks = t;
  // od razu na ekranie, bez restartu
  tft.applyTweaks(t);
  tft.invertDisplay(t.invert ? !LUMI_PANEL_INVERT : LUMI_PANEL_INVERT);
  tft.setRotation(rotation());
  if (disp) lv_obj_invalidate(lv_screen_active());
}

void testPattern() {
  // Pasy rysowane prosto przez sterownik, z pominięciem interfejsu: sprawdzenie przewodów i kolorów
  static const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
  const int n = sizeof colors / sizeof colors[0];
  const int w = tft.width() / n;
  finishDma();
  tft.startWrite();
  for (int i = 0; i < n; i++) tft.fillRect(i * w, 0, i == n - 1 ? tft.width() - i * w : w, tft.height(), colors[i]);
  tft.endWrite();
  ledcWrite(BL_CH, 1u << BL_BITS);
  on = true;
}

}  // namespace display
