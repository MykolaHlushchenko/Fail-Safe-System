#include <Arduino.h>
#include "Config.h"
#include "Diagnostics.h"
#include "Sensor.h"
#include "Indicators.h"
#include "StateMachine.h"

static SystemState state = SystemState::SAFE;
static uint32_t armingStartedAt = 0;

void beginStateMachine() {
    state = SystemState::SAFE;
    armingStartedAt = 0;
}

SystemState getSystemState() {
    return state;
}

String stateName() {
    switch (state) {
        case SystemState::SAFE:
            return "SAFE";

        case SystemState::ARMING:
            return "ARMING";

        case SystemState::ARM:
            return "ARM";

        case SystemState::TRIGGERED:
            return "TRIGGERED (SIMULATION)";

        case SystemState::FAULT:
            return "FAULT";
    }

    return "UNKNOWN";
}

uint32_t getRemainingSeconds() {
    if (state != SystemState::ARMING) {
        return 0;
    }

    uint32_t elapsed = millis() - armingStartedAt;

    if (elapsed >= SAFETY_TIMER_MS) {
        return 0;
    }

    return (SAFETY_TIMER_MS - elapsed + 999) / 1000;
}

void forceSafe(const char* reason) {
    Serial.print("[FAIL-SAFE] ");
    Serial.println(reason);

    state = SystemState::SAFE;
    armingStartedAt = 0;

    setLed(0);
    digitalWrite(BUZZER_PIN, LOW);
}

bool canStart() {
    return state == SystemState::SAFE &&
           diag.allOK() &&
           safetySensor.healthy();
}

void commandStart() {
    if (!canStart()) {
        Serial.println("[START] REJECTED - fail-safe");
        forceSafe("START rejected");
        return;
    }

    armingStartedAt = millis();
    state = SystemState::ARMING;

    Serial.println("[STATE] SAFE -> ARMING");
}

void commandStop() {
    Serial.println("[STOP] Returning to SAFE");
    forceSafe("STOP command");
}

void commandTriggerSimulation() {
    if (state != SystemState::ARM) {
        Serial.println("[TRIGGER] REJECTED - system is not ARM");
        return;
    }

    if (!diag.allOK() || !safetySensor.healthy()) {
        forceSafe("Diagnostic or sensor failure before simulation");
        return;
    }

    state = SystemState::TRIGGERED;

    Serial.println("[SIMULATION] Triggered safely");
}

void updateStateMachine() {
    switch (state) {
        case SystemState::SAFE:
            break;

        case SystemState::ARMING:
            if (!diag.allOK()) {
                forceSafe("Diagnostic failure during ARMING");
                break;
            }

            if (!safetySensor.healthy()) {
                forceSafe("Sensor failure during ARMING");
                break;
            }

            if (millis() - armingStartedAt >= SAFETY_TIMER_MS) {
                state = SystemState::ARM;

                Serial.println("[STATE] ARMING -> ARM");
                beep(100);
            }
            break;

        case SystemState::ARM:
            if (!diag.allOK() || !safetySensor.healthy()) {
                forceSafe("Diagnostic or sensor failure in ARM");
            }
            break;

        case SystemState::TRIGGERED:
            break;

        case SystemState::FAULT:
            forceSafe("FAULT state");
            break;
    }
}
