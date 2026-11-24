#pragma once
#include <map>
#include <string>
#include "Arduino.h"
class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  void putInt(const char* k, int v) { i_[k] = v; }
  int getInt(const char* k, int d) { return i_.count(k) ? (int)i_[k] : d; }
  void putLong64(const char* k, int64_t v) { i_[k] = v; }
  int64_t getLong64(const char* k, int64_t d) { return i_.count(k) ? i_[k] : d; }
  void putString(const char* k, const char* v) { s_[k] = v; }
  String getString(const char* k, const char* d) { return String(s_.count(k) ? s_[k].c_str() : d); }
  bool isKey(const char* k) { return i_.count(k) || s_.count(k); }
  void remove(const char* k) { i_.erase(k); s_.erase(k); }
 private:
  std::map<std::string, int64_t> i_;
  std::map<std::string, std::string> s_;
};
