# Fail-Safe System (ESP32)

## Overview
Embedded firmware for **ESP32** written in C++ that monitors critical system parameters and triggers hardware fail-safe actions upon signal loss or faults. Features a built-in **Web Interface** hosted via an onboard **Wi-Fi Access Point (AP)** for real-time monitoring and configuration without external internet access.

## Key Features
* **Fail-Safe Logic:** Real-time signal/heartbeat monitoring with automated GPIO/PWM safety cutoff on timeout.
* **Wi-Fi Access Point (AP):** Standalone local Wi-Fi hotspot created directly by the ESP32.
* **Embedded Web UI:** Browser-based dashboard for live status monitoring, threshold configuration, and manual testing.

## Tech Stack
* **Hardware:** ESP32
* **Language & Framework:** C++, Arduino / ESP-IDF
* **Networking:** Wi-Fi AP, Embedded HTTP Web Server, HTML/JS
* **Build System:** PlatformIO

1. Power on the ESP32.
2. Connect to the device's Wi-Fi Access Point.
3. Open `http://192.168.4.1` in your browser to access the control panel.
