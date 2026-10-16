#pragma once

class ISensor {
public:
    virtual ~ISensor() = default;

    virtual bool begin() = 0;
    virtual bool healthy() = 0;
    virtual const char* name() = 0;
};

class SimulatedSensor : public ISensor {
public:
    bool begin() override;
    bool healthy() override;
    const char* name() override;

private:
    bool available = false;
};

extern SimulatedSensor safetySensor;
