#pragma once
#include <Arduino.h>

struct Diagnostics {
    bool filesystemOK = false;
    bool preferencesOK = false;
    bool buttonOK = false;
    bool sensorOK = false;
    bool idOK = false;
    bool settingsOK = false;
    bool voltageOK = true;

    bool allOK() const {
        return filesystemOK &&
               preferencesOK &&
               buttonOK &&
               sensorOK &&
               idOK &&
               settingsOK &&
               voltageOK;
    }
};

extern Diagnostics diag;

bool runSelfTest();
float readSupplyVoltage();
String getEspId();
