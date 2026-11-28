#include "../BuildConfig.h"
#include "Panel.h"

#ifdef LUMI_WEB_PANEL

// Panel nie ma logowania: każdy w sieci domowej mógłby dzwonić z LUMI i czytać ekran (kontakty,
// wiadomości). Do urządzenia dla seniora nie może trafić.
#ifdef LUMI_VARIANT_SERIES
#error "LUMI_WEB_PANEL to narzędzie prototypu - nie włączaj go w wersji series"
#endif

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../app/Events.h"
#include "../hw/Board.h"
#include "../hw/Display.h"
#include "../hw/Input.h"
#include "../net/Identity.h"
#include "../net/Net.h"

namespace panel {

namespace {

constexpr uint16_t PORT = 8080;  // 80 zajmuje strona konfiguracji Wi-Fi
WebServer* srv = nullptr;
IPAddress shownIp;
bool wasUp = false;  // czy w poprzednim obiegu była jakakolwiek sieć

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>LUMI - przyciski</title>
<style>
:root{--bg:#0b0b0d;--card:#1c1d21;--text:#f4f1ea;--dim:#a39e94;--acc:#f2b04b}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif;display:flex;justify-content:center}
main{width:100%;max-width:420px;padding:16px}
h1{font-size:20px;margin:4px 0 2px}p{color:var(--dim);margin:0 0 16px;font-size:14px}
.dev{display:grid;grid-template-columns:1fr 92px;gap:10px;background:#000;border-radius:22px;padding:12px}
.scr{border-radius:12px;overflow:hidden;background:#111;display:grid;place-items:center;color:#555;font-size:13px;aspect-ratio:4/3;align-self:start}
.scr img{width:100%;height:100%;display:block}
.dev{grid-template-columns:1fr}
.shot{display:grid;grid-template-columns:1fr 92px;gap:10px}
.rail{display:grid;gap:10px}
button{font:inherit;font-weight:600;color:var(--text);background:var(--card);border:0;border-radius:14px;min-height:64px;touch-action:none;user-select:none}
button.on{background:var(--acc);color:#000}
.row{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:10px}
.wheel{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px}
small{display:block;font-weight:400;color:var(--dim);font-size:12px}
button.on small{color:#000}
#log{margin-top:14px;color:var(--dim);font-size:13px;min-height:20px}
</style></head><body><main>
<h1>LUMI - przyciski</h1>
<p>Krótko = naciśnięcie. Przytrzymaj ponad 1 s = długie naciśnięcie.</p>
<div class="dev"><div class="shot">
 <div class="scr"><img id="scr" alt="ekran LUMI"></div>
 <div class="rail">
  <button data-k="top">Górny<small>•</small></button>
  <button data-k="mid">Środek<small>&nbsp;</small></button>
  <button data-k="bottom">Dolny<small>–</small></button>
 </div>
</div></div>
<div class="row">
 <button data-k="music">Muzyka</button><button data-k="home">Dom</button>
 <button data-k="call">Dzwoń</button><button data-k="power">Zasil.</button>
</div>
<div class="wheel"><button data-w="-1">Rolka −<small>ciszej</small></button><button data-w="1">Rolka +<small>głośniej</small></button></div>
<div id="log"></div>
<script>
const log=t=>document.getElementById('log').textContent=t;
// podgląd ekranu: obraz pobierany tylko wtedy, gdy na LUMI coś się zmieniło
const img=document.getElementById('scr');let ver=-1,busy=false;
async function poll(){if(busy)return;busy=true;try{const v=await (await fetch('/v')).text();
 if(v!==ver){ver=v;await new Promise(r=>{img.onload=img.onerror=r;img.src='/screen.bmp?'+v})}}catch(e){}busy=false}
setInterval(poll,400);poll();
const send=(u,t)=>fetch(u,{method:'POST'}).then(r=>{log(r.ok?t:'błąd '+r.status);setTimeout(poll,150)}).catch(()=>log('brak połączenia z LUMI'));
document.querySelectorAll('[data-k]').forEach(b=>{
 let t0=0;
 b.addEventListener('pointerdown',e=>{t0=Date.now();b.classList.add('on');b.setPointerCapture(e.pointerId)});
 const up=()=>{if(!t0)return;b.classList.remove('on');const l=Date.now()-t0>=1000;t0=0;
  send('/key?k='+b.dataset.k+'&e='+(l?'long':'short'),b.firstChild.textContent+(l?' (długo)':''))};
 b.addEventListener('pointerup',up);b.addEventListener('pointercancel',up);
});
document.querySelectorAll('[data-w]').forEach(b=>b.addEventListener('click',()=>send('/wheel?n='+b.dataset.w,'rolka '+(b.dataset.w>0?'+':'−'))));
</script></main></body></html>)HTML";

void onKey() {
  Key k;
  if (!input::keyFromName(srv->arg("k").c_str(), k)) {
    srv->send(400, "text/plain", "nieznany przycisk");
    return;
  }
  Serial.printf("[panel] %s %s\n", srv->arg("k").c_str(), srv->arg("e") == "long" ? "(dlugo)" : "");
  if (srv->arg("e") == "long") {
    input::inject(k, BtnEvent::HoldStart);
    input::inject(k, BtnEvent::Long);
  } else {
    input::inject(k, BtnEvent::Short);
  }
  srv->send(200, "text/plain", "ok");
}

void onWheel() {
  int n = srv->arg("n").toInt();
  if (n < -5 || n > 5 || n == 0) {
    srv->send(400, "text/plain", "n: -5..5");
    return;
  }
  bus::post(Event(Ev::Wheel, n));
  srv->send(200, "text/plain", "ok");
}

// Ekran jako BMP 16-bitowy (RGB565, BI_BITFIELDS, wiersze od góry). Bez kompresji: prosty i szybki
// do przygotowania na ESP32; ok. 150 KB na klatkę, a strona pobiera ją tylko po zmianie.
void onScreen() {
  const uint16_t* fb = display::mirror();
  if (!fb) {
    srv->send(503, "text/plain", "podglad niedostepny");
    return;
  }
  constexpr uint32_t W = board::SCREEN_W, H = board::SCREEN_H;
  constexpr uint32_t HDR = 14 + 40 + 12, DATA = W * H * 2;
  uint8_t h[HDR] = {0};
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  h[0] = 'B';
  h[1] = 'M';
  put32(2, HDR + DATA);
  put32(10, HDR);
  put32(14, 40);
  put32(18, W);
  put32(22, (uint32_t)(-(int32_t)H));  // ujemna wysokość = wiersze od góry
  h[26] = 1;
  h[28] = 16;
  put32(30, 3);  // BI_BITFIELDS
  put32(34, DATA);
  put32(54, 0xF800);
  put32(58, 0x07E0);
  put32(62, 0x001F);
  srv->setContentLength(HDR + DATA);
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(200, "image/bmp", "");
  srv->sendContent(reinterpret_cast<const char*>(h), HDR);
  // po 16 wierszy (20 KB) - mniej wywołań sieci niż wiersz po wierszu
  for (uint32_t y = 0; y < H; y += 16) {
    uint32_t rows = (H - y) < 16 ? (H - y) : 16;
    srv->sendContent(reinterpret_cast<const char*>(fb + y * W), rows * W * 2);
  }
}

void task(void*) {
  for (;;) {
    bool sta = WiFi.status() == WL_CONNECTED;
    bool ap = WiFi.getMode() & WIFI_MODE_AP;
    if (!sta && !ap) {
      wasUp = false;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    // Sieć zniknęła i wróciła (np. restart radia): gniazdo nasłuchu trzeba otworzyć od nowa,
    // inaczej panel milczy do restartu urządzenia
    if (srv && !wasUp) {
      srv->stop();
      srv->begin();
    }
    wasUp = true;
    if (!srv) {
      srv = new WebServer(PORT);
      srv->on("/", HTTP_GET, [] { srv->send_P(200, "text/html; charset=utf-8", PAGE); });
      srv->on("/key", HTTP_POST, onKey);
      srv->on("/wheel", HTTP_POST, onWheel);
      srv->on("/screen.bmp", HTTP_GET, onScreen);
      srv->on("/v", HTTP_GET, [] { srv->send(200, "text/plain", String(display::mirrorVersion())); });
      srv->begin();
    }
    if (sta && WiFi.localIP() != shownIp) {
      shownIp = WiFi.localIP();
      Serial.printf("[panel] przyciski w przegladarce: %s\n", address().c_str());
    }
    srv->handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));  // 100 razy na sekundę wystarczy na kliknięcia, a rdzeń 0 częściej śpi
  }
}

}  // namespace

void begin() {
  Preferences p;
  p.begin("panel", false);
  net::setPanelAp(p.getBool("ap", true));  // domyślnie włączona: działa w każdej sieci
  p.end();
  xTaskCreatePinnedToCore(task, "panel", 6144, nullptr, 1, nullptr, 0);  // serwer WWW + String: 4 KB to za mało na zapas
}

void setOwnNetwork(bool on) {
  Preferences p;
  p.begin("panel", false);
  p.putBool("ap", on);
  p.end();
  net::setPanelAp(on);
}

std::string address() {
  std::string r;
  if (WiFi.status() == WL_CONNECTED)
    r = std::string("w sieci domowej: http://") + WiFi.localIP().toString().c_str() + ":" + std::to_string(PORT);
  if (net::panelAp()) {
    if (!r.empty()) r += "\n";
    r += std::string("albo polacz telefon z siecia LUMI-PANEL (haslo ") + identity::apPassword() +
         ") i otworz http://192.168.4.1:" + std::to_string(PORT);
  }
  return r;
}

}  // namespace panel

#else

namespace panel {
void begin() {}
void setOwnNetwork(bool) {}
std::string address() { return ""; }
}  // namespace panel

#endif
