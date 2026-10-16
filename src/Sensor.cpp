#include "Sensor.h"

bool SimulatedSensor::begin() {
    available = true;
    return true;
}

bool SimulatedSensor::healthy() {
    return available;
}

const char* SimulatedSensor::name() {
    return "SimulatedSafetySensor";
}

SimulatedSensor safetySensor;
