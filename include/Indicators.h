#pragma once
#include <Arduino.h>
#include "SystemState.h"

void setLed(uint8_t brightness);
void beep(uint16_t durationMs);
void updateIndicators(SystemState state);
