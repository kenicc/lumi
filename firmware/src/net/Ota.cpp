#include "net/Ota.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "Version.h"
#include "app/Diag.h"
#include "net/ota_pubkey.h"

extern "C" const uint8_t lumi_ca_bundle[];

namespace ota {

namespace {
constexpr int MAX_TRIES = 3;
bool justUpdated = false;
int rolledBack = 0;

bool verifySignature(const uint8_t digest[32], const std::string& sigB64) {
  uint8_t sig[128];
  size_t sigLen = 0;
  if (mbedtls_base64_decode(sig, sizeof sig, &sigLen, (const uint8_t*)sigB64.data(), sigB64.size()) != 0) return false;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const uint8_t*)LUMI_OTA_PUBKEY, sizeof(LUMI_OTA_PUBKEY)) == 0 &&
            mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, digest, 32, sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}
}  // namespace

bool enabled() { return sizeof(LUMI_OTA_PUBKEY) > 1; }

void begin() {
  Preferences p;
  p.begin("lumi-ota", false);
  int pend = p.getInt("pend", 0);
  if (pend) {
    if (pend == LUMI_FW_BUILD) {
      int tries = p.getInt("tries", 0) + 1;
      if (tries > MAX_TRIES) {
        // nowa wersja nie potrafi się utrzymać - wracamy na partycję z poprzednią
        const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
        p.remove("pend");
        p.remove("tries");
        p.putInt("rolled", pend);
        p.putInt("bad", pend);  // nie pobieraj tej wersji ponownie
        p.end();
        if (other && esp_ota_set_boot_partition(other) == ESP_OK) {
          Serial.printf("[ota] build %d nie wstal %d razy - powrot do poprzedniej wersji\n", pend, MAX_TRIES);
          delay(100);
          esp_restart();
        }
        return;
      }
      p.putInt("tries", tries);
      justUpdated = tries == 1;
    } else {
      // działa inna wersja niż oczekiwana (np. powrót zrobił już bootloader)
      p.remove("pend");
      p.remove("tries");
      p.putInt("rolled", pend);
      p.putInt("bad", pend);
    }
  }
  rolledBack = p.getInt("rolled", 0);
  p.end();
}

void markHealthy() {
  esp_ota_mark_app_valid_cancel_rollback();  // gdy bootloader ma włączony powrót - też potwierdź
  Preferences p;
  p.begin("lumi-ota", false);
  if (p.getInt("pend", 0) == LUMI_FW_BUILD) {
    p.remove("pend");
    p.remove("tries");
  }
  if (rolledBack) {
    diag::error("[ota] wrocono z build %d do %d", rolledBack, LUMI_FW_BUILD);
    p.remove("rolled");  // raz zgłoszone wystarczy
  }
  p.end();
}

bool updatedThisBoot() { return justUpdated; }
int rolledBackFrom() { return rolledBack; }

bool install(const Offer& o, std::string& err) {
  if (!enabled()) return err = "brak klucza publicznego", false;
  if (o.build <= LUMI_FW_BUILD) return err = "nie nowsza", false;
  if (o.size < 100000 || o.sha256.size() != 64) return err = "zla oferta", false;
  Preferences rolledCheck;
  rolledCheck.begin("lumi-ota", true);
  bool badBefore = rolledCheck.getInt("bad", 0) == o.build;
  rolledCheck.end();
  if (badBefore) return err = "ta wersja juz raz nie wstala", false;

  WiFiClientSecure secure;
  WiFiClient plain;
  HTTPClient http;
  http.setTimeout(15000);
  bool https = o.url.rfind("https://", 0) == 0;
  if (https) secure.setCACertBundle(lumi_ca_bundle);
  if (!(https ? http.begin(secure, o.url.c_str()) : http.begin(plain, o.url.c_str()))) return err = "adres", false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    return err = "HTTP " + std::to_string(code), false;
  }
  if ((size_t)http.getSize() != o.size) {
    http.end();
    return err = "rozmiar", false;
  }
  if (!Update.begin(o.size, U_FLASH)) {
    http.end();
    return err = "miejsce na partycji", false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);
  WiFiClient* s = http.getStreamPtr();
  static uint8_t buf[4096];
  size_t done = 0;
  uint32_t lastData = millis();
  while (done < o.size && millis() - lastData < 20000) {
    size_t avail = s->available();
    if (!avail) {
      delay(5);
      continue;
    }
    int n = s->readBytes(buf, avail > sizeof buf ? sizeof buf : avail);
    if (n <= 0) continue;
    mbedtls_sha256_update_ret(&sha, buf, n);
    if (Update.write(buf, n) != (size_t)n) break;
    done += n;
    lastData = millis();
  }
  http.end();
  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&sha, digest);
  mbedtls_sha256_free(&sha);

  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  if (done != o.size) {
    Update.abort();
    return err = "przerwane pobieranie", false;
  }
  if (o.sha256 != hex) {
    Update.abort();
    return err = "zly skrot SHA-256", false;
  }
  if (!verifySignature(digest, o.sig)) {
    Update.abort();
    diag::error("[ota] odrzucony obraz build %d: zly podpis", o.build);
    return err = "zly podpis", false;
  }
  if (!Update.end(true)) return err = std::string("zapis: ") + Update.errorString(), false;

  Preferences p;
  p.begin("lumi-ota", false);
  p.putInt("pend", o.build);
  p.putInt("tries", 0);
  p.end();
  Serial.printf("[ota] zainstalowano %s (build %d), restart\n", o.version.c_str(), o.build);
  return true;
}

}  // namespace ota
