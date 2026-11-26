#pragma once
// Rozmowy przez modem LTE (VoLTE) sterowany komendami AT po UART.
// Jedna warstwa dla modemów: SIMCom A7670E (KAmod), Quectel EG915U-EU (seria) i SIM800L
// (prototyp z Allegro, tylko 2G - bez VoLTE).
// Modem rozpoznawany automatycznie (ATI). Używamy tylko standardowych komend 3GPP
// (ATD, ATA, AT+CHUP, AT+CLCC, AT+CLIP, AT+CLVL), różnice są w jednej tabeli w Modem.cpp.
//
// Prywatność: oprogramowanie nigdy nie włącza mikrofonu poza rozmową (żadnych komend
// nagrywania ani pętli audio). Mikrofon jest podłączony tylko do modemu.
#include <stdint.h>

#include <Stream.h>

namespace modem {

enum class Vendor : uint8_t { Unknown, Simcom, Quectel, Sim800 };

struct Status {
  bool ready = false;
  bool simOk = false;
  Vendor vendor = Vendor::Unknown;
  char model[24] = {0};
  char fw[48] = {0};
  int bars = 0;  // 0..4
  bool registered = false;
  bool lte = false;
  char op[20] = {0};
  int batteryMv = 0;  // SIM800: napięcie akumulatora z AT+CBC
  uint16_t restarts = 0;       // modem sam się zrestartował (zwykle za słabe zasilanie przy nadawaniu)
  uint16_t voltageWarnings = 0;  // SIM800: UNDER-/OVER-VOLTAGE od modemu
};

void begin();  // uruchamia zadanie modemu na rdzeniu 1

void dial(const char* number);
void answer();
void hangup();
void setCallVolume(uint8_t level);  // 0..10, przeliczane na zakres AT+CLVL danego modemu
void powerOff();                    // przed wyłączeniem LUMI

// Dźwięk z modemu - tylko SIM800 (prototyp bez DAC): głośność dzwonka 0..10 i ton na głośniku modemu
void ringer(uint8_t level);
void tone(uint16_t hz, uint16_t onMs, uint16_t offMs, uint16_t totalMs);

// Konsola serwisowa
void raw(const char* cmd);  // wynik wypisywany na USB
void diag();                // zestaw komend do testu VoLTE (etap 1)
void bridge(Stream& usb);   // przezroczyste połączenie USB <-> modem, koniec: linia "+++"

Status status();
const char* vendorName(Vendor v);

}  // namespace modem
