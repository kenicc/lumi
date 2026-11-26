#include "hw/Display.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "hw/Board.h"

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
};

Lgfx tft;
lv_display_t* disp = nullptr;
bool on = false;

constexpr int BL_CH = 0;
constexpr int BL_BITS = 8;
constexpr int BL_FREQ = 25000;  // powyżej pasma słyszalnego

constexpr int BUF_LINES = 40;

void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  uint32_t w = lv_area_get_width(a), h = lv_area_get_height(a);
  tft.startWrite();
  tft.setAddrWindow(a->x1, a->y1, w, h);
  tft.writePixels(reinterpret_cast<uint16_t*>(px), w * h, true);
  tft.endWrite();
  lv_display_flush_ready(d);
}

uint32_t tickMs() { return millis(); }

}  // namespace

namespace display {

void begin() {
  ledcSetup(BL_CH, BL_FREQ, BL_BITS);
  ledcAttachPin(pins::LCD_BL, BL_CH);
  ledcWrite(BL_CH, 0);

  tft.init();
  tft.setRotation(LUMI_ROTATION);
  tft.fillScreen(0);

  lv_init();
  lv_tick_set_cb(tickMs);
  disp = lv_display_create(board::SCREEN_W, board::SCREEN_H);
  lv_display_set_flush_cb(disp, flush);
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

}  // namespace display
