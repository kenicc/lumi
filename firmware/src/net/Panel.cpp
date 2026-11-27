#include "../BuildConfig.h"
#include "Panel.h"

#ifdef LUMI_WEB_PANEL

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../app/Events.h"
#include "../hw/Input.h"

namespace panel {

namespace {

constexpr uint16_t PORT = 8080;  // 80 zajmuje strona konfiguracji Wi-Fi
WebServer* srv = nullptr;
IPAddress shownIp;

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
.scr{border:1px dashed #333;border-radius:12px;display:grid;place-items:center;color:#555;font-size:13px;min-height:220px;text-align:center;padding:8px}
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
<div class="dev">
 <div class="scr">tu jest ekran LUMI<br>patrz na urządzenie</div>
 <div class="rail">
  <button data-k="top">Górny<small>•</small></button>
  <button data-k="mid">Środek<small>&nbsp;</small></button>
  <button data-k="bottom">Dolny<small>–</small></button>
 </div>
</div>
<div class="row">
 <button data-k="music">Muzyka</button><button data-k="home">Dom</button>
 <button data-k="call">Dzwoń</button><button data-k="power">Zasil.</button>
</div>
<div class="wheel"><button data-w="-1">Rolka −<small>ciszej</small></button><button data-w="1">Rolka +<small>głośniej</small></button></div>
<div id="log"></div>
<script>
const log=t=>document.getElementById('log').textContent=t;
const send=(u,t)=>fetch(u,{method:'POST'}).then(r=>log(r.ok?t:'błąd '+r.status)).catch(()=>log('brak połączenia z LUMI'));
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

void task(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (!srv) {
      srv = new WebServer(PORT);
      srv->on("/", HTTP_GET, [] { srv->send_P(200, "text/html; charset=utf-8", PAGE); });
      srv->on("/key", HTTP_POST, onKey);
      srv->on("/wheel", HTTP_POST, onWheel);
      srv->begin();
    }
    if (WiFi.localIP() != shownIp) {
      shownIp = WiFi.localIP();
      Serial.printf("[panel] przyciski w przegladarce: %s\n", address().c_str());
    }
    srv->handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void begin() { xTaskCreatePinnedToCore(task, "panel", 4096, nullptr, 1, nullptr, 0); }

std::string address() {
  if (WiFi.status() != WL_CONNECTED) return "";
  return std::string("http://") + WiFi.localIP().toString().c_str() + ":" + std::to_string(PORT);
}

}  // namespace panel

#else

namespace panel {
void begin() {}
std::string address() { return ""; }
}  // namespace panel

#endif
