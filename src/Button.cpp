#include <Arduino.h>
#include "Config.h"
#include "Button.h"
#include "StateMachine.h"

static bool lastButtonState = HIGH;
static uint32_t lastButtonChange = 0;
static bool buttonHandled = false;

void beginButton() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    lastButtonState = digitalRead(BUTTON_PIN);
    lastButtonChange = millis();
    buttonHandled = false;
}

void processButton() {
    bool current = digitalRead(BUTTON_PIN);

    if (current != lastButtonState) {
        lastButtonChange = millis();
        lastButtonState = current;
    }

    if (millis() - lastButtonChange < 50) {
        return;
    }

    if (current == LOW) {
        if (!buttonHandled) {
            buttonHandled = true;

            Serial.println("[BUTTON] STOP");
            commandStop();
        }
    } else {
        buttonHandled = false;
    }
}
