#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

// ============================================================
// ESP32 SAFE SIMULATOR
// LittleFS + State Machine + Fail-Safe
// ============================================================

// ============================================================
// CONFIGURATION
// ============================================================

// 5 min
//static constexpr uint32_t SAFETY_TIMER_MS = 5UL * 60UL * 1000UL;

// FOR QUICK TEST:
 static constexpr uint32_t SAFETY_TIMER_MS = 10UL * 1000UL;

// ---------------- GPIO ----------------
static constexpr uint8_t LED_PIN    = 2;
static constexpr uint8_t BUTTON_PIN = 4;
static constexpr uint8_t BUZZER_PIN = 5;

// ADC control
static constexpr int VOLTAGE_ADC_PIN = 34;

// ---------------- PWM ----------------
static constexpr uint32_t PWM_FREQ = 1000;
static constexpr uint8_t PWM_BITS = 8;

// ---------------- Supply voltage ----------
static constexpr float MIN_SUPPLY_V = 4.5f;
static constexpr float MAX_SUPPLY_V = 14.0f;

// ---------------- Wi-Fi AP ----------------
const char* AP_NAME = "ESP32-SAFE-SIM";
const char* AP_PASS = "12345678";

// GLOBAL OBJECTS
WebServer server(80);
Preferences prefs;

// STATE MACHINE
enum class SystemState {
  SAFE,
  ARMING,
  ARM,
  TRIGGERED,
  FAULT
};

SystemState state = SystemState::SAFE;

uint32_t armingStartedAt = 0;

// SENSOR ABSTRACTION
class ISensor {

public:
  virtual bool begin() = 0;
  virtual bool healthy() = 0;
  virtual const char* name() = 0;
};

// ============================================================
// SIMULATED SENSOR
// ============================================================
class SimulatedSensor : public ISensor {

public:
  bool begin() override {
    available = true;

    return true;
  }

  bool healthy() override {

    return available;
  }

  const char* name() override {

    return "SimulatedSafetySensor";
  }

private:

  bool available = false;
};

SimulatedSensor safetySensor;

// DIAGNOSTICS
struct Diagnostics {
  bool filesystemOK = false;
  bool preferencesOK = false;
  bool buttonOK = false;
  bool sensorOK = false;
  bool idOK = false;
  bool settingsOK = false;
  bool voltageOK = true;

  bool allOK() const {
    return
      filesystemOK &&
      preferencesOK &&
      buttonOK &&
      sensorOK &&
      idOK &&
      settingsOK &&
      voltageOK;
  }
};

Diagnostics diag;

// Get ESP32 ID
String getEspId()
{
  uint64_t chipid = ESP.getEfuseMac();
  char id[32];

  snprintf(id, sizeof(id), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

  return String(id);
}

// LED
void setLed(uint8_t brightness)
{
  analogWrite(LED_PIN, brightness);
}

// BEEP
void beep(uint16_t durationMs)
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

// STATE INDICATORS
void updateIndicators()
{
  switch (state) {
    case SystemState::SAFE:
      setLed(0);
      digitalWrite(BUZZER_PIN, LOW);

      break;

    case SystemState::ARMING: {
      bool phase = ((millis() / 500) % 2) == 0;
      setLed(phase ? 100 : 0);
      digitalWrite(BUZZER_PIN, LOW);

      break;
    }

    case SystemState::ARM:
      setLed(255);
      digitalWrite(BUZZER_PIN, LOW);

      break;

    case SystemState::TRIGGERED:
      setLed(((millis() / 100) % 2)? 255 : 0);
      if ((millis() % 1000) < 100) {
        digitalWrite(BUZZER_PIN, HIGH);
      } else {
        digitalWrite(BUZZER_PIN, LOW);
      }

      break;

    case SystemState::FAULT:
      setLed(((millis() / 150) % 2) ? 255 : 0);
      digitalWrite(BUZZER_PIN, HIGH);

      break;
  }
}

// FAIL-SAFE
void forceSafe(const char* reason)
{
  Serial.print("[FAIL-SAFE] ");
  Serial.println(reason);

  state = SystemState::SAFE;
  armingStartedAt = 0;

  setLed(0);
  digitalWrite(BUZZER_PIN, LOW);
}

// LITTLEFS CHECK
bool checkFilesystem()
{
  Serial.println("[CHECK] LittleFS...");

  // Mount
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] LittleFS mount failed");

    return false;
  }

  Serial.println("[OK] LittleFS mounted");

  // Write test
  File file =  LittleFS.open("/selftest.txt", FILE_WRITE);

  if (!file) {
    Serial.println("[ERROR] Cannot create test file");

    return false;
  }

  file.println("ESP32 SAFE SIM LittleFS self-test");
  file.close();

  // Read test
  File readFile = LittleFS.open("/selftest.txt", FILE_READ);

  if (!readFile) {
    Serial.println("[ERROR] Cannot read test file");

    return false;
  }

  bool ok = readFile.available();
  readFile.close();

  // Remove test file
  if (!LittleFS.remove("/selftest.txt")) {
    Serial.println("[ERROR] Cannot remove test file");

    return false;
  }

  if (ok) {
    Serial.println("[OK] LittleFS read/write");

  } else {
    Serial.println("[ERROR] LittleFS read failure");
  }

  // Filesystem information
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();

  Serial.printf("[LittleFS] Total: %u bytes\n", (unsigned)total);
  Serial.printf("[LittleFS] Used : %u bytes\n", (unsigned)used);

  return ok;
}

// PREFERENCES CHECK
bool checkSettings()
{
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

  Serial.printf("[OK] Settings version: %lu\n", (unsigned long)version);

  return true;
}

// BUTTON CHECK
bool checkButton()
{
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

// SENSOR CHECK
bool checkSensors()
{
  Serial.println("[CHECK] Sensors...");

  if (!safetySensor.begin()) {
    Serial.println("[ERROR] Sensor init failed");

    return false;
  }

  if (!safetySensor.healthy()) {
    Serial.println("[ERROR] Sensor unhealthy");

    return false;
  }

  Serial.printf("[OK] Sensor: %s\n", safetySensor.name());

  return true;
}

// VOLTAGE MONITORING
float readSupplyVoltage()
{
  int raw = analogRead(VOLTAGE_ADC_PIN);
  float vadc = (raw / 4095.0f) * 3.3f;

  return vadc * (127.0f / 27.0f);
}

// CHECK VOLTAGE
bool checkVoltage()
{
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

// COMPLETE SELF TEST
bool runSelfTest()
{
  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 SAFE SIM SELF-TEST");
  Serial.println("================================");

  // ESP ID
  diag.idOK = getEspId().length() > 0;

  Serial.print("[ID] ESP32 ID: ");
  Serial.println(getEspId());

  // LittleFS
  diag.filesystemOK = checkFilesystem();

  // Preferences
  diag.preferencesOK = checkSettings();

  // Settings
  diag.settingsOK = SAFETY_TIMER_MS > 0 && SAFETY_TIMER_MS <= 10UL * 60UL * 1000UL;

  // Button
  diag.buttonOK = checkButton();

  // Sensors
  diag.sensorOK = checkSensors();

  // Voltage
  diag.voltageOK = checkVoltage();

  bool result = diag.allOK();
  Serial.println();

  if (result) {
    Serial.println("[SELF-TEST] PASS");
  } else {
    Serial.println("[SELF-TEST] FAIL");
  }

  Serial.println("================================");

  return result;
}

// START COMMAND
bool canStart()
{
  // Start with SAFE.
  if (state != SystemState::SAFE) {

    return false;
  }

  // All diagnostics should be OK.
  if (!diag.allOK()) {

    return false;
  }

  if (!safetySensor.healthy()) {

    return false;
  }

  return true;
}

// COMMAND START
void commandStart()
{
  if (!canStart()) {
    Serial.println("[START] REJECTED - fail-safe");
    forceSafe("START rejected");

    return;
  }

  armingStartedAt =  millis();
  state = SystemState::ARMING;
  Serial.println("[STATE] SAFE -> ARMING");
}

// STOP COMMAND
void commandStop()
{
  Serial.println("[STOP] Returning to SAFE");
  forceSafe("STOP command");
}

// TRIGGER SIMULATION
void commandTriggerSimulation() {
  if (state != SystemState::ARM) {
    Serial.println("[TRIGGER] REJECTED - system is not ARM");

    return;
  }

  if (!safetySensor.healthy()) {
    forceSafe("Sensor failure before simulation");

    return;
  }

  state = SystemState::TRIGGERED;

  Serial.println("[SIMULATION] Triggered safely");
}

// PWM COMMAND
void handlePWM()
{
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing value");

    return;
  }

  int value = server.arg("value").toInt();

  if (value < 0 || value > 255) {
    server.send(400, "text/plain", "PWM must be 0..255");

    return;
  }

  // PWM FOR LED.
  setLed((uint8_t)value);
  server.send(200, "text/plain", "Demo LED PWM updated");
}

// STATE NAME
String stateName()
{
  switch (state) {
    case SystemState::SAFE:
      return "SAFE";

    case SystemState::ARMING:
      return "ARMING";

    case SystemState::ARM:
      return "ARM";

    case SystemState::TRIGGERED:
      return "TRIGGERED (SIMULATION)";

    case SystemState::FAULT:
      return "FAULT";
  }

  return "UNKNOWN";
}

// WEB PAGE
String htmlPage()
{
  String stateClass;
  switch (state) {
    case SystemState::SAFE:
      stateClass = "safe";
      break;

    case SystemState::ARMING:
      stateClass = "arming";
      break;

    case SystemState::ARM:
      stateClass = "arm";
      break;

    case SystemState::TRIGGERED:
      stateClass = "triggered";
      break;

    case SystemState::FAULT:
      stateClass = "fault";
      break;
  }

  uint32_t remainingSeconds = 0;

  if (state == SystemState::ARMING) {
    uint32_t elapsed = millis() - armingStartedAt;

    if (elapsed < SAFETY_TIMER_MS) {
      remainingSeconds = (SAFETY_TIMER_MS - elapsed) / 1000;
    }
  }

  // HTML PAGE
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
body{margin:0;min-height:100vh;font-family:Arial,sans-serif;background:linear-gradient(135deg,#080b12,#101827);color:#e8edf5}
.container{max-width:760px;margin:auto;padding:24px}
.header{margin-bottom:20px}
.header h1{margin:0;font-size:30px}
.header p,.info{color:#8d9aad;font-size:13px}
.card{background:#111925;border:1px solid #243044;border-radius:18px;padding:20px;margin-bottom:16px}
.state-card{text-align:center;padding:30px 20px}
.state-label{color:#8996aa;font-size:12px;letter-spacing:1.5px;margin-bottom:10px}
.state{font-size:36px;font-weight:800}
.safe,.ok{color:#42d392}
.arming{color:#f5c451}
.arm{color:#ff9f43}
.triggered,.bad,.fault{color:#ff5c5c}
.timer{margin-top:14px;color:#f5c451}
.warning{display:inline-block;margin-top:16px;padding:7px 12px;border-radius:999px;background:#281a1a;color:#ff7777;font-size:12px}
.section-title{margin:0 0 15px;font-size:17px}
.buttons,.diagnostics{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{border:0;border-radius:12px;padding:14px 10px;font-size:15px;font-weight:bold;color:white;cursor:pointer}
.btn-start{background:#20895c}
.btn-stop{background:#596579}
.btn-trigger{background:#a93636}
.pwm-row{display:flex;gap:10px}
input[type=number]{flex:1;min-width:0;padding:14px;border-radius:10px;border:1px solid #2c394d;background:#0b111b;color:white;font-size:16px}
.btn-pwm{background:#425fc7;min-width:120px}
.diagnostics{grid-template-columns:repeat(2,1fr)}
.diag{display:flex;justify-content:space-between;gap:10px;padding:12px;border-radius:10px;background:#0c131e;border:1px solid #202c3e;font-size:13px}
.ok,.bad{font-weight:bold}
.footer{text-align:center;color:#566276;font-size:11px;padding:10px}
@media(max-width:600px){.container{padding:14px}.buttons,.diagnostics{grid-template-columns:1fr}.state{font-size:30px}.pwm-row{flex-direction:column}.btn-pwm{width:100%}}
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
<button class="btn-trigger" onclick="location.href='/trigger'">Fire<br><small>(symulation)</small></button>
</div>
</div>

<div class="card">
<h2 class="section-title">DEMO PWM</h2>
<form class="pwm-row" action="/pwm" method="get">
<input type="number" name="value" min="0" max="255" value="128" required>
<button class="btn-pwm" type="submit">Set PWM</button>
</form>
<p class="info">PWM for LED.</p>
</div>

<div class="card">
<h2 class="section-title">Diagnostics</h2>
<div class="diagnostics">
%DIAGNOSTICS%
<div class="diag"><span>ESP ID</span><span class="ok">%ESP_ID%</span></div>
</div>
</div>

<div class="footer">ESP32 Safe Simulator<br>LittleFS · State Machine · Fail-Safe</div>
</div>
</body>
</html>
)rawliteral";

  // State
  page.replace("%STATE%", stateClass);
  page.replace("%STATE_NAME%", stateName());

  // Timer
  String timerHtml;
  if (state == SystemState::ARMING) {
    timerHtml = "<div class=\"timer\">Timer: " +
                String(remainingSeconds) +
                " secconds</div><div class=\"warning\">System waiting</div>";
  }
  page.replace("%TIMER%", timerHtml);

  // Warning
  String warningHtml;
  if (state == SystemState::TRIGGERED) {
    warningHtml = "<div class=\"warning\">Only SYMULATION</div>";
  } else if (state == SystemState::FAULT) {
    warningHtml = "<div class=\"warning\">FAIL-SAFE: System blocked</div>";
  }
  page.replace("%WARNING%", warningHtml);

  // Diagnostics
  String diagnosticsHtml;
  const char* labels[] = {
    "LittleFS", "Preferences", "Button",
    "Sensor", "Power"
  };
  bool values[] = {
    diag.filesystemOK, diag.preferencesOK, diag.buttonOK,
    diag.sensorOK, diag.voltageOK
  };

  for (int i = 0; i < 5; i++) {
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

  return page;

}

// WEB HANDLERS
void handleRoot()
{
  server.send(200, "text/html; charset=utf-8", htmlPage());
}

void handleStart()
{
  commandStart();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStop()
{
  commandStop();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleTrigger()
{
  commandTriggerSimulation();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
// PHYSICAL BUTTON
// ============================================================
bool lastButtonState = HIGH;

uint32_t lastButtonChange = 0;

bool buttonHandled = false;

void processButton()
{
  bool current = digitalRead(BUTTON_PIN);

  // Detect change
  if (current != lastButtonState) {
    lastButtonChange = millis();
    lastButtonState = current;
  }

  // Debounce
  if (millis() - lastButtonChange < 50) {

    return;
  }

  // Button pressed
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

// STATE MACHINE UPDATE
void updateStateMachine()
{
  switch (state) {
    // SAFE
    case SystemState::SAFE:
      // do nothing
      break;

    // ARMING
    case SystemState::ARMING:
      if (!diag.allOK()) {
        forceSafe("Diagnostic failure during ARMING");

        break;
      }

      // Check sensor.
      if (!safetySensor.healthy()) {
        forceSafe("Sensor failure during ARMING");

        break;
      }

      // Check timer
      if (millis() - armingStartedAt >= SAFETY_TIMER_MS) {
        state = SystemState::ARM;
        Serial.println("[STATE] ARMING -> ARM");

        beep(100);
      }

      break;

    // ARM
    case SystemState::ARM:
      if (!safetySensor.healthy()) {
        forceSafe("Sensor failure in ARM");
      }

      break;

    // TRIGGERED
    case SystemState::TRIGGERED:
      break;

    // FAULT
    case SystemState::FAULT:
      forceSafe("FAULT state");

      break;
  }
}


// SETUP
void setup()
{
  Serial.begin(115200);

  delay(3000);

  // GPIO
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  setLed(0);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // ADC
  analogReadResolution(12);

  // GUARANTEED INITIAL STATE
  state = SystemState::SAFE;

  // SELF TEST
  bool selfTestOK =  runSelfTest();

  if (!selfTestOK) {
    Serial.println("[BOOT] SELF-TEST FAILED");
    state = SystemState::FAULT;

    delay(500);

    forceSafe("Boot self-test failure");
  }

  // WIFI ACCESS POINT
  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(AP_NAME, AP_PASS)) {
    Serial.println("[ERROR] WiFi AP failed");

    forceSafe("WiFi initialization failed");

  } else {
    Serial.print("[WEB] AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  // HTTP ROUTES
  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/trigger", HTTP_GET, handleTrigger);
  server.on("/pwm", HTTP_GET, handlePWM);
  server.onNotFound([]() {
      server.send(404, "text/plain", "Not found");
    }
  );

  server.begin();

  Serial.println("[WEB] Server started");
  Serial.println("[BOOT] System ready in SAFE");
}

// LOOP
void loop()
{
  server.handleClient();
  processButton();
  updateStateMachine();
  updateIndicators();

  delay(5);
}
