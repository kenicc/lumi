#include "RtcCodec.h"

#include "AtParse.h"

namespace rtcc {

namespace {

uint8_t toBcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
int fromBcd(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

// odwrotność at::daysFromCivil (algorytm H. Hinnanta)
void civilFromDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t yy = (int64_t)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yy + (m <= 2));
}

}  // namespace

void encode(int64_t utc, uint8_t regs[7]) {
  int64_t days = utc >= 0 ? utc / 86400 : (utc - 86399) / 86400;
  int secs = (int)(utc - days * 86400);
  int y, m, d;
  civilFromDays(days, y, m, d);
  regs[0] = toBcd(secs % 60);
  regs[1] = toBcd((secs / 60) % 60);
  regs[2] = toBcd(secs / 3600);                          // tryb 24-godzinny
  regs[3] = (uint8_t)(((days + 4) % 7 + 7) % 7 + 1);     // dzień tygodnia 1-7 (niedziela = 1); zegar go nie używa
  regs[4] = toBcd(d);
  regs[5] = toBcd(m);                                    // bit stulecia 0: lata 2000-2099
  regs[6] = toBcd(y - 2000);
}

bool decode(const uint8_t regs[7], int64_t& utc) {
  if (regs[2] & 0x40) return false;  // tryb 12-godzinny - nie nasz zapis
  int s = fromBcd(regs[0] & 0x7F), mi = fromBcd(regs[1] & 0x7F), h = fromBcd(regs[2] & 0x3F);
  int d = fromBcd(regs[4] & 0x3F), mo = fromBcd(regs[5] & 0x1F), y = 2000 + fromBcd(regs[6]);
  if (s > 59 || mi > 59 || h > 23 || d < 1 || d > 31 || mo < 1 || mo > 12) return false;
  if (y < 2024) return false;  // nowy moduł startuje od 2000 roku
  utc = at::daysFromCivil(y, (unsigned)mo, (unsigned)d) * 86400 + h * 3600 + mi * 60 + s;
  return true;
}

}  // namespace rtcc
