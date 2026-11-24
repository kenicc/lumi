#pragma once
// Pliki czytane z katalogu podanego w SIM_FS_ROOT (domyślnie firmware/data)
#include <stdio.h>
#include <stdint.h>
#include <string>
class File {
 public:
  File(FILE* f = nullptr) : f_(f) {}
  explicit operator bool() const { return f_ != nullptr; }
  size_t size() { long p = ftell(f_); fseek(f_, 0, SEEK_END); long s = ftell(f_); fseek(f_, p, SEEK_SET); return s; }
  size_t read(uint8_t* b, size_t n) { return fread(b, 1, n, f_); }
  void close() { if (f_) fclose(f_); f_ = nullptr; }
 private:
  FILE* f_;
};
struct LittleFSStub {
  std::string root = "data";
  File open(const char* path, const char* mode) { return File(fopen((root + path).c_str(), mode)); }
};
extern LittleFSStub LittleFS;
