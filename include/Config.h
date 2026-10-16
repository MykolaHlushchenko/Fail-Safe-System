#pragma once
#include <Arduino.h>

inline constexpr uint8_t LED_PIN = 2;
inline constexpr uint8_t BUTTON_PIN = 4;
inline constexpr uint8_t BUZZER_PIN = 5;
inline constexpr int VOLTAGE_ADC_PIN = 34;

inline constexpr uint32_t SAFETY_TIMER_MS = 10UL * 1000UL;

inline constexpr uint32_t PWM_FREQ = 1000;
inline constexpr uint8_t PWM_BITS = 8;

inline constexpr float MIN_SUPPLY_V = 4.5f;
inline constexpr float MAX_SUPPLY_V = 14.0f;

inline constexpr char AP_NAME[] = "ESP32-SAFE-SIM";
inline constexpr char AP_PASS[] = "12345678";
