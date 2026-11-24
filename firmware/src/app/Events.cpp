#include "app/Events.h"

#include <Arduino.h>

namespace bus {

static QueueHandle_t q;

void begin() { q = xQueueCreate(32, sizeof(Event)); }

bool post(const Event& e) { return q && xQueueSend(q, &e, 0) == pdTRUE; }

bool receive(Event& e, uint32_t timeoutMs) { return q && xQueueReceive(q, &e, pdMS_TO_TICKS(timeoutMs)) == pdTRUE; }

}  // namespace bus
