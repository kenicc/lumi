#pragma once
// Minimalne zastępniki Arduino dla symulatora ekranu (sim/). Nie są używane w firmware.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>

time_t sim_time(time_t* t);
#define time(x) sim_time(x)

uint32_t millis();
void delay(uint32_t ms);
int digitalRead(int pin);
void digitalWrite(int pin, int v);
#define LOW 0
#define HIGH 1

class String {
 public:
  String(const char* s = "") : s_(s ? s : "") {}
  String(int v) : s_(std::to_string(v)) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }

 private:
  std::string s_;
};

struct SerialStub {
  template <typename... A>
  void printf(const char* f, A... a) { ::printf(f, a...); }
  void println(const char* s = "") { ::printf("%s\n", s); }
  void print(const char* s) { ::printf("%s", s); }
};
extern SerialStub Serial;

struct EspStub {
  uint64_t getEfuseMac() { return 0xA1B2C3D4E5F6ULL; }
  void restart() {}
};
extern EspStub ESP;
