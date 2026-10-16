#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "Config.h"
#include "Diagnostics.h"
#include "StateMachine.h"
#include "Indicators.h"
#include "WebUI.h"

static WebServer server(80);

static String htmlPage() {
    String stateClass;

    switch (getSystemState()) {
        case SystemState::SAFE:      stateClass = "safe"; break;
        case SystemState::ARMING:    stateClass = "arming"; break;
        case SystemState::ARM:       stateClass = "arm"; break;
        case SystemState::TRIGGERED: stateClass = "triggered"; break;
        case SystemState::FAULT:     stateClass = "fault"; break;
    }

    String page = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="5">
<title>ESP32 Safe Simulator</title>
<style>
*{box-sizing:border-box}
body{margin:0;min-height:100vh;font-family:Arial,sans-serif;
background:linear-gradient(135deg,#080b12,#101827);color:#e8edf5}
.container{max-width:760px;margin:auto;padding:24px}
.header{margin-bottom:20px}
.header h1{margin:0;font-size:30px}
.header p,.info{color:#8d9aad;font-size:13px}
.card{background:#111925;border:1px solid #243044;border-radius:18px;
padding:20px;margin-bottom:16px}
.state-card{text-align:center;padding:30px 20px}
.state-label{color:#8996aa;font-size:12px;letter-spacing:1.5px;margin-bottom:10px}
.state{font-size:36px;font-weight:800}
.safe,.ok{color:#42d392}
.arming{color:#f5c451}
.arm{color:#ff9f43}
.triggered,.bad,.fault{color:#ff5c5c}
.timer{margin-top:14px;color:#f5c451}
.warning{display:inline-block;margin-top:16px;padding:7px 12px;
border-radius:999px;background:#281a1a;color:#ff7777;font-size:12px}
.section-title{margin:0 0 15px;font-size:17px}
.buttons,.diagnostics{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{border:0;border-radius:12px;padding:14px 10px;font-size:15px;
font-weight:bold;color:white;cursor:pointer}
.btn-start{background:#20895c}
.btn-stop{background:#596579}
.btn-trigger{background:#a93636}
.pwm-row{display:flex;gap:10px}
input[type=number]{flex:1;min-width:0;padding:14px;border-radius:10px;
border:1px solid #2c394d;background:#0b111b;color:white;font-size:16px}
.btn-pwm{background:#425fc7;min-width:120px}
.diagnostics{grid-template-columns:repeat(2,1fr)}
.diag{display:flex;justify-content:space-between;gap:10px;padding:12px;
border-radius:10px;background:#0c131e;border:1px solid #202c3e;font-size:13px}
.ok,.bad{font-weight:bold}
.footer{text-align:center;color:#566276;font-size:11px;padding:10px}
@media(max-width:600px){
.container{padding:14px}
.buttons,.diagnostics{grid-template-columns:1fr}
.state{font-size:30px}
.pwm-row{flex-direction:column}
.btn-pwm{width:100%}}
</style>
</head>
<body>
<div class="container">
<header class="header">
<h1>ESP32 Safe Simulator</h1>
<p>State Machine</p>
</header>

<div class="card state-card">
<div class="state-label">Current state</div>
<div class="state %STATE%">%STATE_NAME%</div>
%TIMER%
%WARNING%
</div>

<div class="card">
<h2 class="section-title">Management</h2>
<div class="buttons">
<button class="btn-start" onclick="location.href='/start'">Start</button>
<button class="btn-stop" onclick="location.href='/stop'">Stop</button>
<button class="btn-trigger" onclick="location.href='/trigger'">
Fire<br><small>(simulation)</small></button>
</div>
</div>

<div class="card">
<h2 class="section-title">DEMO PWM</h2>
<form class="pwm-row" action="/pwm" method="get">
<input type="number" name="value" min="0" max="255" value="128" required>
<button class="btn-pwm" type="submit">Set PWM</button>
</form>
<p class="info">PWM for LED (demo only).</p>
</div>

<div class="card">
<h2 class="section-title">Diagnostics</h2>
<div class="diagnostics">
%DIAGNOSTICS%
<div class="diag"><span>ESP ID</span><span class="%ID_CLASS%">%ESP_ID%</span></div>
</div>
</div>

<div class="footer">ESP32 Safe Simulator<br>LittleFS · State Machine · Fail-Safe</div>
</div>
</body>
</html>
)rawliteral";

    page.replace("%STATE%", stateClass);
    page.replace("%STATE_NAME%", stateName());

    String timerHtml;

    if (getSystemState() == SystemState::ARMING) {
        timerHtml =
            "<div class=\"timer\">Timer: " +
            String(getRemainingSeconds()) +
            " seconds</div><div class=\"warning\">System waiting</div>";
    }

    page.replace("%TIMER%", timerHtml);

    String warningHtml;

    if (getSystemState() == SystemState::TRIGGERED) {
        warningHtml = "<div class=\"warning\">SIMULATION ONLY</div>";
    } else if (getSystemState() == SystemState::FAULT) {
        warningHtml = "<div class=\"warning\">FAIL-SAFE: System blocked</div>";
    }

    page.replace("%WARNING%", warningHtml);

    const char* labels[] = {
        "LittleFS", "Preferences", "Button", "Sensor", "Power", "Settings"
    };

    bool values[] = {
        diag.filesystemOK, diag.preferencesOK, diag.buttonOK,
        diag.sensorOK, diag.voltageOK, diag.settingsOK
    };

    String diagnosticsHtml;

    for (int i = 0; i < 6; i++) {
        diagnosticsHtml += "<div class=\"diag\"><span>";
        diagnosticsHtml += labels[i];
        diagnosticsHtml += "</span><span class=\"";
        diagnosticsHtml += values[i] ? "ok" : "bad";
        diagnosticsHtml += "\">";
        diagnosticsHtml += values[i] ? "OK" : "FAIL";
        diagnosticsHtml += "</span></div>";
    }

    page.replace("%DIAGNOSTICS%", diagnosticsHtml);
    page.replace("%ESP_ID%", getEspId());
    page.replace("%ID_CLASS%", diag.idOK ? "ok" : "bad");

    return page;
}

static void handleRoot() {
    server.send(200, "text/html; charset=utf-8", htmlPage());
}

static void handleStart() {
    commandStart();
    server.sendHeader("Location", "/");
    server.send(303);
}

static void handleStop() {
    commandStop();
    server.sendHeader("Location", "/");
    server.send(303);
}

static void handleTrigger() {
    commandTriggerSimulation();
    server.sendHeader("Location", "/");
    server.send(303);
}

static void handlePWM() {
    if (!server.hasArg("value")) {
        server.send(400, "text/plain", "Missing value");
        return;
    }

    String arg = server.arg("value");

    if (arg.isEmpty()) {
        server.send(400, "text/plain", "Missing value");
        return;
    }

    for (size_t i = 0; i < arg.length(); i++) {
        if (arg[i] < '0' || arg[i] > '9') {
            server.send(400, "text/plain", "PWM must be 0..255");
            return;
        }
    }

    long value = arg.toInt();

    if (value < 0 || value > 255) {
        server.send(400, "text/plain", "PWM must be 0..255");
        return;
    }

    setLed(static_cast<uint8_t>(value));
    server.send(200, "text/plain", "Demo LED PWM updated");
}

bool beginWebUI() {
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP(AP_NAME, AP_PASS)) {
        Serial.println("[ERROR] WiFi AP failed");
        return false;
    }

    Serial.print("[WEB] AP IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/start", HTTP_GET, handleStart);
    server.on("/stop", HTTP_GET, handleStop);
    server.on("/trigger", HTTP_GET, handleTrigger);
    server.on("/pwm", HTTP_GET, handlePWM);

    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });

    server.begin();

    Serial.println("[WEB] Server started");
    return true;
}

void processWebUI() {
    server.handleClient();
}
