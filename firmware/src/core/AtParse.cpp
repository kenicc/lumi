#include "core/AtParse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace at {

static const char* afterPrefix(const char* line, const char* prefix) {
  size_t n = strlen(prefix);
  if (strncmp(line, prefix, n) != 0) return nullptr;
  line += n;
  while (*line == ' ') line++;
  return line;
}

// kopiuje tekst w cudzysłowie zaczynający się w p (p wskazuje na '"')
static const char* copyQuoted(const char* p, char* out, size_t n) {
  if (n) out[0] = 0;
  if (*p != '"') return p;
  p++;
  size_t i = 0;
  while (*p && *p != '"') {
    if (i + 1 < n) out[i++] = *p;
    p++;
  }
  if (n) out[i] = 0;
  return *p == '"' ? p + 1 : p;
}

bool parseClip(const char* line, char* number, size_t n) {
  const char* p = afterPrefix(line, "+CLIP:");
  if (!p) return false;
  copyQuoted(p, number, n);
  return true;
}

bool parseClcc(const char* line, Clcc& out) {
  const char* p = afterPrefix(line, "+CLCC:");
  if (!p) return false;
  int mpty = 0;
  if (sscanf(p, "%d,%d,%d,%d,%d", &out.idx, &out.dir, &out.stat, &out.mode, &mpty) != 5) return false;
  out.number[0] = 0;
  const char* q = strchr(p, '"');
  if (q) copyQuoted(q, out.number, sizeof(out.number));
  return true;
}

bool parseCsq(const char* line, int& rssi) {
  const char* p = afterPrefix(line, "+CSQ:");
  if (!p) return false;
  return sscanf(p, "%d", &rssi) == 1;
}

int csqToBars(int rssi) {
  if (rssi == 99 || rssi < 2) return 0;
  if (rssi < 10) return 1;
  if (rssi < 15) return 2;
  if (rssi < 20) return 3;
  return 4;
}

bool parseRegStat(const char* line, int& stat) {
  const char* p = afterPrefix(line, "+CEREG:");
  if (!p) p = afterPrefix(line, "+CREG:");
  if (!p) return false;
  int n = 0;
  if (sscanf(p, "%d,%d", &n, &stat) == 2) return true;
  return false;
}

bool parseCops(const char* line, char* op, size_t n, int& act) {
  const char* p = afterPrefix(line, "+COPS:");
  if (!p) return false;
  act = -1;
  if (n) op[0] = 0;
  const char* q = strchr(p, '"');
  if (!q) return true;  // brak sieci: "+COPS: 0"
  q = copyQuoted(q, op, n);
  if (*q == ',') act = atoi(q + 1);
  return true;
}

bool parseCbc(const char* line, int& mv) {
  const char* p = afterPrefix(line, "+CBC:");
  if (!p) return false;
  int chg, pct;
  return sscanf(p, "%d,%d,%d", &chg, &pct, &mv) == 3 && mv > 2500 && mv < 5000;
}

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

bool parseCclk(const char* line, int64_t& utc) {
  const char* p = afterPrefix(line, "+CCLK:");
  if (!p) return false;
  if (*p == '"') p++;
  int yy, mo, dd, hh, mi, ss, tz = 0;
  char sign = '+';
  int got = sscanf(p, "%d/%d/%d,%d:%d:%d%c%d", &yy, &mo, &dd, &hh, &mi, &ss, &sign, &tz);
  if (got < 6) return false;
  if (yy < 24 || yy > 69 || mo < 1 || mo > 12 || dd < 1 || dd > 31) return false;  // modem bez czasu z sieci (np. 70/01/01)
  int64_t local = daysFromCivil(2000 + yy, mo, dd) * 86400 + hh * 3600 + mi * 60 + ss;
  int offset = (got == 8) ? tz * 15 * 60 : 0;
  if (sign == '-') offset = -offset;
  utc = local - offset;
  return true;
}

static size_t lastDigits(const char* s, char* out, size_t want) {
  char buf[32];
  size_t n = 0;
  for (; *s && n < sizeof(buf); s++)
    if (isdigit((unsigned char)*s)) buf[n++] = *s;
  size_t start = n > want ? n - want : 0;
  size_t k = n - start;
  memcpy(out, buf + start, k);
  out[k] = 0;
  return k;
}

bool sameNumber(const char* a, const char* b) {
  if (!a || !b) return false;
  char x[16], y[16];
  size_t nx = lastDigits(a, x, 9), ny = lastDigits(b, y, 9);
  if (nx < 3 || ny < 3) return false;  // krótkie numery serwisowe porównuj w całości
  return nx == ny && strcmp(x, y) == 0;
}

void formatNumber(const char* in, char* out, size_t n) {
  if (!n) return;
  char d[24];
  size_t k = 0;
  const char* p = in;
  bool plus = (*p == '+');
  for (; *p && k + 1 < sizeof(d); p++)
    if (isdigit((unsigned char)*p)) d[k++] = *p;
  d[k] = 0;
  const char* digits = d;
  // polski numer z prefiksem 48: pokaż same 9 cyfr
  if (plus && k == 11 && strncmp(d, "48", 2) == 0) digits = d + 2;
  size_t len = strlen(digits), o = 0;
  for (size_t i = 0; i < len && o + 2 < n; i++) {
    if (i && (len - i) % 3 == 0) out[o++] = ' ';
    out[o++] = digits[i];
  }
  out[o] = 0;
}

}  // namespace at
