#include <Arduino.h>

#include "Config.h"
#include "Diagnostics.h"
#include "StateMachine.h"
#include "Indicators.h"
#include "Button.h"
#include "WebUI.h"

void setup() {
    Serial.begin(115200);
    delay(3000);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);
    setLed(0);

    analogReadResolution(12);

    beginButton();
    beginStateMachine();

    bool selfTestOK = runSelfTest();

    if (!selfTestOK) {
        Serial.println("[BOOT] SELF-TEST FAILED");
        forceSafe("Boot self-test failure");
    }

    if (!beginWebUI()) {
        forceSafe("WiFi initialization failed");
    }

    Serial.println("[BOOT] System ready in SAFE");
}

void loop() {
    processWebUI();
    processButton();
    updateStateMachine();
    updateIndicators(getSystemState());

    delay(5);
}
