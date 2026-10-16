#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "Storage.h"

static Preferences prefs;

bool checkFilesystem() {
    Serial.println("[CHECK] LittleFS...");

    if (!LittleFS.begin(false)) {
        Serial.println("[ERROR] LittleFS mount failed");
        return false;
    }

    File file = LittleFS.open("/selftest.txt", FILE_WRITE);

    if (!file) {
        Serial.println("[ERROR] Cannot create test file");
        return false;
    }

    file.println("ESP32 SAFE SIM self-test");
    file.close();

    File readFile = LittleFS.open("/selftest.txt", FILE_READ);

    if (!readFile) {
        Serial.println("[ERROR] Cannot read test file");
        LittleFS.remove("/selftest.txt");
        return false;
    }

    bool ok = readFile.available();
    readFile.close();

    if (!LittleFS.remove("/selftest.txt")) {
        Serial.println("[ERROR] Cannot remove test file");
        return false;
    }

    Serial.printf("[LittleFS] Total: %u bytes\n",
                  (unsigned)LittleFS.totalBytes());

    Serial.printf("[LittleFS] Used: %u bytes\n",
                  (unsigned)LittleFS.usedBytes());

    Serial.println(ok ? "[OK] LittleFS read/write"
                      : "[ERROR] LittleFS read failure");

    return ok;
}

bool checkSettings() {
    Serial.println("[CHECK] Preferences...");

    if (!prefs.begin("safe-sim", false)) {
        Serial.println("[ERROR] Preferences failed");
        return false;
    }

    if (!prefs.isKey("version")) {
        if (prefs.putUInt("version", 1) == 0) {
            Serial.println("[ERROR] Cannot save settings");
            return false;
        }
    }

    uint32_t version = prefs.getUInt("version", 0);

    if (version != 1) {
        Serial.println("[ERROR] Invalid settings version");
        return false;
    }

    Serial.printf("[OK] Settings version: %lu\n",
                  (unsigned long)version);

    return true;
}
