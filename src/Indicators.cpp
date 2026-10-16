#include <Arduino.h>
#include "Config.h"
#include "Indicators.h"

void setLed(uint8_t brightness) {
    analogWrite(LED_PIN, brightness);
}

void beep(uint16_t durationMs) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(durationMs);
    digitalWrite(BUZZER_PIN, LOW);
}

void updateIndicators(SystemState state) {
    switch (state) {
        case SystemState::SAFE:
            setLed(0);
            digitalWrite(BUZZER_PIN, LOW);
            break;

        case SystemState::ARMING: {
            bool phase = ((millis() / 500) % 2) == 0;
            setLed(phase ? 100 : 0);
            digitalWrite(BUZZER_PIN, LOW);
            break;
        }

        case SystemState::ARM:
            setLed(255);
            digitalWrite(BUZZER_PIN, LOW);
            break;

        case SystemState::TRIGGERED:
            setLed(((millis() / 100) % 2) ? 255 : 0);

            digitalWrite(
                BUZZER_PIN,
                (millis() % 1000) < 100 ? HIGH : LOW
            );
            break;

        case SystemState::FAULT:
            setLed(((millis() / 150) % 2) ? 255 : 0);
            digitalWrite(BUZZER_PIN, HIGH);
            break;
    }
}
