#include "modem/Modem.h"

#include <Arduino.h>
#include <sys/time.h>

#include "app/Events.h"
#include "core/AtParse.h"
#include "hw/Board.h"

namespace modem {

namespace {

HardwareSerial& uart = Serial1;

enum class CmdType : uint8_t { Dial, Answer, Hangup, Volume, Raw, Diag, PowerOff, Ringer, Tone };
struct Cmd {
  CmdType t;
  char arg[64];
  uint16_t v[4];
};

QueueHandle_t cmdQ;
SemaphoreHandle_t stMutex;
Status st;
volatile bool bridging = false;
int clvlMax = 5;  // nadpisywane odpowiedzią na AT+CLVL=?
bool timeSet = false;

// --- stan rozmowy ---
enum class CallState : uint8_t { Idle, Incoming, Dialing, Active };
CallState cs = CallState::Idle;
uint32_t csSince = 0;
bool incomingPosted = false;
bool wasActive = false;

// --- odczyt linii ---
char line[160];
size_t lineLen = 0;

void setStatus(void (*fn)(Status&)) {
  xSemaphoreTake(stMutex, portMAX_DELAY);
  fn(st);
  xSemaphoreGive(stMutex);
}

void setCall(CallState s) {
  cs = s;
  csSince = millis();
}

void onEnded() {
  if (cs == CallState::Idle) return;
  bus::post(Event(Ev::CallEnded, wasActive ? 1 : 0));
  setCall(CallState::Idle);
}

void onActive() {
  if (cs == CallState::Active) return;
  wasActive = true;
  setCall(CallState::Active);
  bus::post(Event(Ev::CallActive));
}

void onRing() {
  if (cs != CallState::Idle) return;
  setCall(CallState::Incoming);
  incomingPosted = false;
  wasActive = false;
}

// Zwraca true, gdy linia była komunikatem niezamówionym (URC) i została obsłużona.
bool handleUrc(const char* l) {
  char num[24];
  if (!strcmp(l, "RING") || !strncmp(l, "+CRING:", 7)) {
    onRing();
    return true;
  }
  if (at::parseClip(l, num, sizeof num)) {
    onRing();
    if (cs == CallState::Incoming && !incomingPosted) {
      incomingPosted = true;
      bus::post(Event(Ev::CallIncoming, 0, 0, num));
    }
    return true;
  }
  if (!strcmp(l, "NO CARRIER") || !strcmp(l, "BUSY") || !strcmp(l, "NO ANSWER") || !strcmp(l, "NO DIALTONE") ||
      !strncmp(l, "VOICE CALL: END", 15) || !strncmp(l, "MISSED_CALL", 11)) {
    onEnded();
    return true;
  }
  if (!strncmp(l, "VOICE CALL: BEGIN", 17)) {  // SIMCom
    onActive();
    return true;
  }
  if (!strncmp(l, "+CTZV", 5) || !strncmp(l, "+CTZE", 5) || !strncmp(l, "*PSUTTZ", 7) || !strncmp(l, "DST:", 4)) {
    timeSet = false;  // sieć przysłała czas - odczytaj ponownie
    return true;
  }
  // komunikaty startowe i SMS - niepotrzebne w wersji podstawowej
  static const char* const kIgnore[] = {"RDY", "PB DONE", "SMS DONE", "+QIND", "*ATREADY", "+CMTI", "+CGEV"};
  for (const char* p : kIgnore)
    if (!strncmp(l, p, strlen(p))) return true;
  return false;
}

// Czyta jedną linię (bez \r\n). false gdy minął czas.
bool readLine(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (uart.available()) {
      char c = uart.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line[lineLen] = 0;
        bool got = lineLen > 0;
        lineLen = 0;
        if (got) return true;
        continue;
      }
      if (lineLen + 1 < sizeof(line)) line[lineLen++] = c;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return false;
}

enum class Res : uint8_t { Ok, Error, Timeout };

// Wysyła komendę i zbiera linie odpowiedzi (bez URC) do resp, oddzielone '\n'.
Res cmd(const char* c, uint32_t timeoutMs = 2000, char* resp = nullptr, size_t n = 0, bool echoToUsb = false) {
  if (resp && n) resp[0] = 0;
  uart.print(c);
  uart.print("\r");
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (!readLine(timeoutMs - (millis() - start))) break;
    if (echoToUsb) Serial.printf("  < %s\n", line);
    if (!strcmp(line, "OK")) return Res::Ok;
    if (!strncmp(line, "ERROR", 5) || !strncmp(line, "+CME ERROR", 10)) return Res::Error;
    if (!strcmp(line, c)) continue;  // echo przed ATE0
    if (handleUrc(line)) continue;
    if (resp && n) {
      size_t used = strlen(resp);
      snprintf(resp + used, n - used, "%s%s", used ? "\n" : "", line);
    }
  }
  return Res::Timeout;
}

void pulsePwrkey(uint32_t ms) {
  if (board::MODEM_RESET_ONLY) {
    // płytka bez PWRKEY (np. SIM800L): modem działa od podania zasilania, a pin to RST (stan niski = reset)
    digitalWrite(pins::MODEM_PWRKEY, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));
    digitalWrite(pins::MODEM_PWRKEY, HIGH);
    return;
  }
  digitalWrite(pins::MODEM_PWRKEY, board::PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
  vTaskDelay(pdMS_TO_TICKS(ms));
  digitalWrite(pins::MODEM_PWRKEY, board::PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
}

bool probe() {
  for (int i = 0; i < 3; i++)
    if (cmd("AT", 500) == Res::Ok) return true;
  return false;
}

void readTime() {
  char r[64];
  int64_t utc;
  if (cmd("AT+CCLK?", 1000, r, sizeof r) == Res::Ok && at::parseCclk(r, utc)) {
    timeval tv{(time_t)utc, 0};
    settimeofday(&tv, nullptr);
    timeSet = true;
    bus::post(Event(Ev::NetTime, 0, 0, "modem"));
  }
}

// false = modem przestał odpowiadać
bool pollNetwork() {
  char r[96];
  int rssi = 99, stat = 0, act = -1;
  char op[20] = {0};
  Res csq = cmd("AT+CSQ", 1000, r, sizeof r);
  if (csq == Res::Timeout && cmd("AT", 1000) == Res::Timeout) return false;
  if (csq == Res::Ok) at::parseCsq(r, rssi);
  bool reg = false;
  if (cmd("AT+CEREG?", 1000, r, sizeof r) == Res::Ok && at::parseRegStat(r, stat)) reg = at::isRegistered(stat);
  if (!reg && cmd("AT+CREG?", 1000, r, sizeof r) == Res::Ok && at::parseRegStat(r, stat))
    reg = at::isRegistered(stat);
  if (cmd("AT+COPS?", 2000, r, sizeof r) == Res::Ok) at::parseCops(r, op, sizeof op, act);
  xSemaphoreTake(stMutex, portMAX_DELAY);
  st.bars = reg ? at::csqToBars(rssi) : 0;
  st.registered = reg;
  st.lte = (act == 7);
  strlcpy(st.op, op, sizeof st.op);
  Vendor v = st.vendor;
  xSemaphoreGive(stMutex);
  int mv;
  // SIM800L jest zasilany prosto z akumulatora i sam mierzy jego napięcie
  if (v == Vendor::Sim800 && cmd("AT+CBC", 1000, r, sizeof r) == Res::Ok && at::parseCbc(r, mv)) {
    xSemaphoreTake(stMutex, portMAX_DELAY);
    st.batteryMv = mv;
    xSemaphoreGive(stMutex);
  }
  if (reg && !timeSet) readTime();
  return true;
}

// Lista połączeń - jedyne wiarygodne źródło stanu u obu producentów
void pollCalls() {
  char r[200];
  if (cmd("AT+CLCC", 1000, r, sizeof r) != Res::Ok) return;
  bool any = false;
  char* save = nullptr;
  for (char* l = strtok_r(r, "\n", &save); l; l = strtok_r(nullptr, "\n", &save)) {
    at::Clcc c;
    if (!at::parseClcc(l, c) || c.mode != 0) continue;
    any = true;
    if (c.stat == 0) onActive();
    if (c.stat == 4 && cs == CallState::Incoming && !incomingPosted && millis() - csSince > 1500) {
      incomingPosted = true;
      bus::post(Event(Ev::CallIncoming, 0, 0, c.number));
    }
  }
  if (!any && millis() - csSince > 2500) onEnded();
}

bool startup() {
  while (uart.available()) uart.read();
  if (!probe()) {
    Serial.println("[modem] brak odpowiedzi, wlaczam PWRKEY");
    pulsePwrkey(1000);  // >=50 ms dla A7670E, >=500 ms dla EG915U
    uint32_t start = millis();
    bool ok = false;
    while (!ok && millis() - start < 20000) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ok = probe();
    }
    if (!ok) return false;
  }
  cmd("ATE0");
  char r[160];
  Vendor v = Vendor::Unknown;
  if (cmd("ATI", 1000, r, sizeof r) == Res::Ok) {
    if (strcasestr(r, "SIM800") || strcasestr(r, "SIM900")) v = Vendor::Sim800;
    else if (strcasestr(r, "SIMCOM") || strcasestr(r, "A76")) v = Vendor::Simcom;
    else if (strcasestr(r, "Quectel") || strcasestr(r, "EG915")) v = Vendor::Quectel;
  }
  char model[24] = {0}, fw[48] = {0};
  if (cmd("AT+CGMM", 1000, r, sizeof r) == Res::Ok) strlcpy(model, r, sizeof model);
  if (cmd("AT+GMR", 1000, r, sizeof r) == Res::Ok) {
    const char* p = strchr(r, ':');  // "+GMR: A011B07" / "Revision: EG915U..."
    p = p ? p + 1 : r;
    while (*p == ' ') p++;
    strlcpy(fw, p, sizeof fw);
  }
  Serial.printf("[modem] %s %s, oprogramowanie: %s\n", vendorName(v), model, fw);

  // Wspólna konfiguracja: błędy słowne, numer dzwoniącego, rejestracja przez zapytania,
  // czas z sieci, bez połączeń oczekujących (druga rozmowa myli seniora).
  static const char* const kInit[] = {"AT+CMEE=2", "AT+CLIP=1", "AT+CREG=0", "AT+CCWA=0"};
  for (const char* c : kInit)
    if (cmd(c) != Res::Ok) Serial.printf("[modem] %s nie przeszlo\n", c);
  // Różnice producentów: LTE (CEREG, CTZU) albo 2G (SIM800: czas z sieci przez CLTS, własny dzwonek)
  static const char* const kLte[] = {"AT+CEREG=0", "AT+CTZU=1"};
  static const char* const kSim800[] = {"AT+CLTS=1", "AT+CALM=0", "AT+CSCLK=0"};
  if (v == Vendor::Sim800) {
    for (const char* c : kSim800)
      if (cmd(c) != Res::Ok) Serial.printf("[modem] %s nie przeszlo\n", c);
  } else {
    for (const char* c : kLte)
      if (cmd(c) != Res::Ok) Serial.printf("[modem] %s nie przeszlo\n", c);
  }
  if (cmd("AT+CLVL=?", 1000, r, sizeof r) == Res::Ok) {
    int lo, hi;
    const char* p = strchr(r, '(');
    if (p && sscanf(p, "(%d-%d)", &lo, &hi) == 2 && hi > 0) clvlMax = hi;
  }

  bool sim = false;
  for (int i = 0; i < 10 && !sim; i++) {
    if (cmd("AT+CPIN?", 1000, r, sizeof r) == Res::Ok && strstr(r, "READY")) sim = true;
    else vTaskDelay(pdMS_TO_TICKS(1000));
  }

  xSemaphoreTake(stMutex, portMAX_DELAY);
  st.vendor = v;
  st.simOk = sim;
  strlcpy(st.model, model, sizeof st.model);
  strlcpy(st.fw, fw, sizeof st.fw);
  st.ready = true;
  xSemaphoreGive(stMutex);
  bus::post(Event(Ev::ModemReady, sim ? 1 : 0, 0, model));
  if (!sim) Serial.println("[modem] brak karty SIM lub PIN");
  return true;
}

bool validNumber(const char* s) {
  if (!*s || strlen(s) > 20) return false;
  for (; *s; s++)
    if (!isdigit((unsigned char)*s) && *s != '+' && *s != '*' && *s != '#') return false;
  return true;
}

void runDiag() {
  static const char* const kCommon[] = {"ATI",      "AT+CGMM",  "AT+GMR",   "AT+CPIN?",  "AT+CSQ",
                                        "AT+COPS?", "AT+CEREG?", "AT+CREG?", "AT+CLVL=?", "AT+CLCC"};
  // Komendy zależne od producenta - do potwierdzenia z dokumentacją AT konkretnej wersji oprogramowania
  static const char* const kQuectel[] = {"AT+QCFG=\"ims\"", "AT+QNWINFO", "AT+QENG=\"servingcell\""};
  static const char* const kSimcom[] = {"AT+CNMP?", "AT+CPSI?"};
  static const char* const kSim800[] = {"AT+CBC", "AT+CCID", "AT+CLVL?", "AT+CMIC?"};
  Serial.println("=== diagnostyka modemu ===");
  for (const char* c : kCommon) {
    Serial.printf("> %s\n", c);
    cmd(c, 3000, nullptr, 0, true);
  }
  Vendor v = status().vendor;
  const char* const* list = v == Vendor::Quectel ? kQuectel : v == Vendor::Sim800 ? kSim800 : kSimcom;
  size_t n = v == Vendor::Quectel ? 3 : v == Vendor::Sim800 ? 4 : 2;
  for (size_t i = 0; i < n; i++) {
    Serial.printf("> %s\n", list[i]);
    cmd(list[i], 3000, nullptr, 0, true);
  }
  if (v == Vendor::Sim800) {
    Serial.println("SIM800 dziala tylko w 2G (bez VoLTE): to test interfejsu i rozmow, nie test etapu 1.");
    Serial.println("Sprawdz: +CREG 0,1 lub 0,5, potem 'call <numer>'.");
    return;
  }
  Serial.println("Sprawdz: +CEREG stat 1/5, +COPS ...,7 (LTE). Potem zadzwon: 'call <numer>'");
  Serial.println("i w trakcie rozmowy 'at AT+COPS?' - wciaz 7 oznacza rozmowe przez VoLTE, bez spadku do 2G.");
}

void handle(const Cmd& c) {
  switch (c.t) {
    case CmdType::Dial:
      if (cs != CallState::Idle || !validNumber(c.arg)) break;
      {
        char b[80];
        snprintf(b, sizeof b, "ATD%s;", c.arg);
        wasActive = false;
        if (cmd(b, 5000) == Res::Ok) {
          setCall(CallState::Dialing);
          bus::post(Event(Ev::CallDialing));
        } else {
          bus::post(Event(Ev::CallEnded, 0, 0, "error"));
        }
      }
      break;
    case CmdType::Answer:
      if (cs == CallState::Incoming && cmd("ATA", 5000) == Res::Ok) onActive();
      break;
    case CmdType::Hangup:
      if (cmd("AT+CHUP", 3000) != Res::Ok) cmd("ATH", 3000);
      onEnded();
      break;
    case CmdType::Volume: {
      char b[24];
      int lvl = atoi(c.arg);
      snprintf(b, sizeof b, "AT+CLVL=%d", (lvl * clvlMax + 5) / 10);
      cmd(b);
      break;
    }
    case CmdType::Ringer:
      if (status().vendor == Vendor::Sim800) {
        char b[24];
        snprintf(b, sizeof b, "AT+CRSL=%u", c.v[0] * 10);  // 0..100
        cmd(b);
      }
      break;
    case CmdType::Tone:
      // SIM800: ton generowany przez modem na jego głośniku (prototyp bez DAC i wzmacniacza)
      if (status().vendor == Vendor::Sim800 && cs == CallState::Idle) {
        char b[56];
        snprintf(b, sizeof b, "AT+SIMTONE=1,%u,%u,%u,%u", c.v[0], c.v[1], c.v[2], c.v[3]);
        cmd(b, 1000);
      }
      break;
    case CmdType::Raw:
      Serial.printf("> %s\n", c.arg);
      Serial.println(cmd(c.arg, 10000, nullptr, 0, true) == Res::Ok ? "  OK" : "  (blad lub brak odpowiedzi)");
      break;
    case CmdType::Diag: runDiag(); break;
    case CmdType::PowerOff: {
      Vendor v = status().vendor;
      Res r = cmd(v == Vendor::Quectel ? "AT+QPOWD=1" : v == Vendor::Sim800 ? "AT+CPOWD=1" : "AT+CPOF", 3000);
      if (r != Res::Ok) pulsePwrkey(3000);
      setStatus([](Status& s) { s.ready = false; });
      break;
    }
  }
}

void task(void*) {
  uart.begin(115200, SERIAL_8N1, pins::MODEM_RX, pins::MODEM_TX);
  pinMode(pins::MODEM_PWRKEY, OUTPUT);
  digitalWrite(pins::MODEM_PWRKEY, board::MODEM_RESET_ONLY ? HIGH : (board::PWRKEY_ACTIVE_HIGH ? LOW : HIGH));
  pinMode(pins::MODEM_DTR, OUTPUT);
  digitalWrite(pins::MODEM_DTR, LOW);  // DTR nisko = modem nie śpi
  pinMode(pins::MODEM_RI, INPUT_PULLUP);

  while (!startup()) {
    Serial.println("[modem] start nieudany, ponowie za 30 s");
    bus::post(Event(Ev::ModemFail));
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
  pollNetwork();

  uint32_t lastNet = millis(), lastClcc = 0;
  uint8_t silentPolls = 0;
  for (;;) {
    if (bridging) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (readLine(20)) handleUrc(line);

    Cmd c;
    while (xQueueReceive(cmdQ, &c, 0) == pdTRUE) handle(c);

    uint32_t now = millis();
    if (cs == CallState::Incoming && !incomingPosted && now - csSince > 1500) {
      incomingPosted = true;  // RING bez +CLIP: numer zastrzeżony
      bus::post(Event(Ev::CallIncoming, 0, 0, ""));
    }
    if (cs != CallState::Idle && now - lastClcc > 1000) {
      lastClcc = now;
      pollCalls();
    }
    if (cs == CallState::Idle && now - lastNet > 30000) {
      lastNet = now;
      if (pollNetwork()) {
        silentPolls = 0;
      } else if (++silentPolls >= 3) {
        // Modem zawiesił się - restart, żeby nie przegapić rozmowy
        Serial.println("[modem] brak odpowiedzi od 90 s, restart modemu");
        setStatus([](Status& s) { s.ready = false; });
        bus::post(Event(Ev::ModemFail));
        pulsePwrkey(board::MODEM_RESET_ONLY ? 200 : 3000);  // wyłącz (lub reset)
        vTaskDelay(pdMS_TO_TICKS(3000));
        while (!startup()) vTaskDelay(pdMS_TO_TICKS(30000));
        silentPolls = 0;
      }
    }
  }
}

void send(CmdType t, const char* arg = "") {
  Cmd c;
  c.t = t;
  strlcpy(c.arg, arg, sizeof c.arg);
  xQueueSend(cmdQ, &c, pdMS_TO_TICKS(100));
}

}  // namespace

void begin() {
  cmdQ = xQueueCreate(8, sizeof(Cmd));
  stMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "modem", 6144, nullptr, 5, nullptr, 1);
}

void dial(const char* number) {
  char clean[24];
  size_t k = 0;
  for (const char* p = number; *p && k + 1 < sizeof clean; p++)
    if (isdigit((unsigned char)*p) || *p == '+' || *p == '*' || *p == '#') clean[k++] = *p;
  clean[k] = 0;
  send(CmdType::Dial, clean);
}
void answer() { send(CmdType::Answer); }
void hangup() { send(CmdType::Hangup); }
void setCallVolume(uint8_t level) {
  char b[4];
  snprintf(b, sizeof b, "%u", level > 10 ? 10 : level);
  send(CmdType::Volume, b);
}
void powerOff() { send(CmdType::PowerOff); }
void ringer(uint8_t level) {
  Cmd c{};
  c.t = CmdType::Ringer;
  c.v[0] = level > 10 ? 10 : level;
  xQueueSend(cmdQ, &c, pdMS_TO_TICKS(100));
}
void tone(uint16_t hz, uint16_t onMs, uint16_t offMs, uint16_t totalMs) {
  Cmd c{};
  c.t = CmdType::Tone;
  c.v[0] = hz;
  c.v[1] = onMs;
  c.v[2] = offMs;
  c.v[3] = totalMs;
  xQueueSend(cmdQ, &c, 0);  // dźwięk klawisza nie może blokować interfejsu
}
void raw(const char* c) { send(CmdType::Raw, c); }
void diag() { send(CmdType::Diag); }

void bridge(Stream& usb) {
  bridging = true;
  vTaskDelay(pdMS_TO_TICKS(100));
  usb.println("Most USB <-> modem. Wpisz +++ i Enter, aby wyjsc.");
  char last[4] = {0};
  for (;;) {
    while (uart.available()) usb.write(uart.read());
    while (usb.available()) {
      char c = usb.read();
      last[0] = last[1];
      last[1] = last[2];
      last[2] = c;
      if (last[0] == '+' && last[1] == '+' && last[2] == '+') {
        uart.print("\r");
        bridging = false;
        usb.println("\nKoniec mostu.");
        return;
      }
      uart.write(c == '\n' ? '\r' : c);
    }
    vTaskDelay(1);
  }
}

Status status() {
  xSemaphoreTake(stMutex, portMAX_DELAY);
  Status s = st;
  xSemaphoreGive(stMutex);
  return s;
}

const char* vendorName(Vendor v) {
  switch (v) {
    case Vendor::Simcom: return "SIMCom";
    case Vendor::Quectel: return "Quectel";
    case Vendor::Sim800: return "SIM800 (2G)";
    default: return "nieznany";
  }
}

}  // namespace modem
