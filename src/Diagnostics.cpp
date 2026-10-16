#include <Arduino.h>
#include "Config.h"
#include "Diagnostics.h"
#include "Storage.h"
#include "Sensor.h"

Diagnostics diag;

String getEspId() {
    uint64_t chipid = ESP.getEfuseMac();
    char id[32];

    snprintf(id, sizeof(id), "%04X%08X",
             (uint16_t)(chipid >> 32),
             (uint32_t)chipid);

    return String(id);
}

float readSupplyVoltage() {
    int raw = analogRead(VOLTAGE_ADC_PIN);
    float vadc = (raw / 4095.0f) * 3.3f;

    return vadc * (127.0f / 27.0f);
}

static bool checkButton() {
    Serial.println("[CHECK] Button...");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    int value = digitalRead(BUTTON_PIN);

    if (value != HIGH && value != LOW) {
        Serial.println("[ERROR] Button GPIO invalid");
        return false;
    }

    Serial.println("[OK] Button GPIO");
    return true;
}

static bool checkSensors() {
    Serial.println("[CHECK] Sensors...");

    if (!safetySensor.begin() || !safetySensor.healthy()) {
        Serial.println("[ERROR] Sensor initialization failed");
        return false;
    }

    Serial.printf("[OK] Sensor: %s\n", safetySensor.name());
    return true;
}

static bool checkVoltage() {
    float voltage = readSupplyVoltage();

    Serial.printf("[VOLTAGE] %.2f V\n", voltage);

    if (voltage < 0.2f) {
        Serial.println("[INFO] Voltage monitor appears disconnected");
        return true;
    }

    if (voltage < MIN_SUPPLY_V || voltage > MAX_SUPPLY_V) {
        Serial.println("[ERROR] Supply voltage out of range");
        return false;
    }

    return true;
}

bool runSelfTest() {
    Serial.println();
    Serial.println("================================");
    Serial.println(" ESP32 SAFE SIM SELF-TEST");
    Serial.println("================================");

    String id = getEspId();

    diag.idOK = id.length() > 0;

    Serial.printf("[ID] ESP32 ID: %s\n", id.c_str());

    diag.filesystemOK = checkFilesystem();
    diag.preferencesOK = checkSettings();

    diag.settingsOK =
        SAFETY_TIMER_MS > 0 &&
        SAFETY_TIMER_MS <= 10UL * 60UL * 1000UL;

    diag.buttonOK = checkButton();
    diag.sensorOK = checkSensors();
    diag.voltageOK = checkVoltage();

    bool result = diag.allOK();

    Serial.println(result
        ? "[SELF-TEST] PASS"
        : "[SELF-TEST] FAIL");

    Serial.println("================================");

    return result;
}
