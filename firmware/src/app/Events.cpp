#include "Events.h"

#include <Arduino.h>

namespace bus {

static QueueHandle_t q;

void begin() { q = xQueueCreate(32, sizeof(Event)); }

static volatile uint32_t drops = 0;

bool post(const Event& e) {
  if (q && xQueueSend(q, &e, 0) == pdTRUE) return true;
  drops = drops + 1;  // pełna kolejka: zdarzenie przepada - licznik widać w "status"
  return false;
}

uint32_t dropped() { return drops; }

bool receive(Event& e, uint32_t timeoutMs) { return q && xQueueReceive(q, &e, pdMS_TO_TICKS(timeoutMs)) == pdTRUE; }

}  // namespace bus
