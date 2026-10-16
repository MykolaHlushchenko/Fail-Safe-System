#pragma once
#include <Arduino.h>
#include "SystemState.h"

void beginStateMachine();

SystemState getSystemState();
String stateName();

bool canStart();
void commandStart();
void commandStop();
void commandTriggerSimulation();
void forceSafe(const char* reason);

void updateStateMachine();
uint32_t getRemainingSeconds();
