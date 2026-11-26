#pragma once
#include <stdlib.h>
#define ESP_SLEEP_WAKEUP_EXT0 2
inline void esp_sleep_enable_ext0_wakeup(int, int) {}
inline void esp_deep_sleep_start() { exit(0); }
