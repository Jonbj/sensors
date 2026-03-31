/*
  ESP32-S3: PT100 via MAX31865 + BH1750 + pH SEN0169-V2 + EC DFR0300 + Torbidità SEN0189 + NTP + MQTT + Web dashboard

  Hardware:
  - I2C su SDA=8, SCL=9 — BH1750
  - MAX31865 software SPI: SCK=IO4, MOSI=IO6, MISO=IO5, CS=IO7
  - pH SEN0169-V2: analogico su IO1
  - EC DFR0300: analogico su IO2 (da collegare)
  - Torbidità SEN0189: analogico su IO3 (da collegare) → proxy OD
  - MOSFET dimming LED: PWM su IO10

  Web dashboard: http://<IP_ESP32>/
  JSON live:     http://<IP_ESP32>/data
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include <BH1750.h>
#include <time.h>
#include <PubSubClient.h>
#include <Preferences.h>

// === LED PWM / MOSFET ===
#define LED_PWM_PIN       10
#define LED_PWM_FREQ      1000
#define LED_PWM_RES_BITS  8
#define LED_PWM_TEST_MODE false
#define LED_PWM_DEFAULT   128
#define LED_MODE_AUTO      false
#define DAY_START_MIN      (8 * 60)
#define DAY_END_MIN        (20 * 60)
#define RAMP_MINUTES       30
#define LED_DAY_PWM        180
#define LED_NIGHT_PWM      0
#define TEMP_SOFT_LIMIT_C  30.0f
#define TEMP_HARD_LIMIT_C  32.0f
#define MIDDAY_BOOST_PCT   100
#define TEMP_EWA_ALPHA     0.10f
#define LUX_EWA_ALPHA      0.15f
#define TEMP_OUTLIER_C     5.0f
#define THERMAL_HYST_C     0.3f

Preferences preferences;

const char* LED_PROFILE_SAFE = "safe";
const char* LED_PROFILE_GROWTH = "growth";
const char* LED_PROFILE_MAINT = "maint";

// === TLS CA (Let's Encrypt ISRG Root X1) ===
static const char LE_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// === CREDENTIALS ===
#ifdef __has_include
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// === MQTT ===
const char* mqtt_server = "mqtt.vlnet.me";
const int   mqtt_port   = 8883;
const char* mqtt_topic  = "sensors/esp32/data";

WiFiClientSecure espClient;
PubSubClient     mqttClient(espClient);

// === NTP ===
#define TZ_EUROPE_ROME "CET-1CEST,M3.5.0,M10.5.0/3"
const char* ntpServer = "pool.ntp.org";

// === SENSORI ===
BH1750 lightMeter;
#define LUX_FACTOR 2.77f

#define MAX31865_SCK  4
#define MAX31865_MOSI 6
#define MAX31865_MISO 5
#define MAX31865_CS   7
#define RNOMINAL 100.0
#define RREF     430.0
Adafruit_MAX31865 max31865(MAX31865_CS, MAX31865_MOSI, MAX31865_MISO, MAX31865_SCK);

#define PH_PIN     1
#define PH_OFFSET  0.0f
#define PH_ARRAY_LEN 40
int phArray[PH_ARRAY_LEN];
int phArrayIndex = 0;

float phAverageArray(int* arr, int len) {
  for (int i = 1; i < len; i++) {
    int key = arr[i], j = i - 1;
    while (j >= 0 && arr[j] > key) { arr[j + 1] = arr[j]; j--; }
    arr[j + 1] = key;
  }
  long sum = 0;
  int from = len / 4, to = len - len / 4;
  for (int i = from; i < to; i++) sum += arr[i];
  return (float)sum / (to - from);
}

float readPH() {
  phArray[phArrayIndex++] = analogRead(PH_PIN);
  if (phArrayIndex >= PH_ARRAY_LEN) phArrayIndex = 0;
  int tmp[PH_ARRAY_LEN];
  memcpy(tmp, phArray, sizeof(phArray));
  float raw     = phAverageArray(tmp, PH_ARRAY_LEN);
  float voltage = raw * 3.3f / 4095.0f;
  float voltageScaled = voltage * (5.0f / 3.3f);
  return 7.0f + ((2.5f - voltageScaled) / 0.18f) + PH_OFFSET;
}

#define EC_PIN    2
#define EC_OFFSET 0.0f
#define EC_ARRAY_LEN 40
int ecArray[EC_ARRAY_LEN];
int ecArrayIndex = 0;
int ecValidSamples = 0;
bool ecConnected = false;

float readEC(float temperatureC) {
  if (!ecConnected) return NAN;
  ecArray[ecArrayIndex++] = analogRead(EC_PIN);
  if (ecArrayIndex >= EC_ARRAY_LEN) ecArrayIndex = 0;
  if (ecValidSamples < EC_ARRAY_LEN) ecValidSamples++;
  if (ecValidSamples < 20) return NAN;
  int tmp[EC_ARRAY_LEN];
  memcpy(tmp, ecArray, sizeof(ecArray));
  float raw     = phAverageArray(tmp, EC_ARRAY_LEN);
  float voltage = raw * 3.3f / 4095.0f;
  float ecRaw = (133.42f * voltage * voltage * voltage
               - 255.86f * voltage * voltage
               + 857.39f * voltage) * 0.5f;
  float tempCoeff = 1.0f + 0.02f * (temperatureC - 25.0f);
  float ec = ecRaw / tempCoeff;
  return (ec * 1000.0f) + EC_OFFSET;
}

#define TURB_PIN      3
#define TURB_OFFSET   0.0f
bool turbConnected = false;

float readTurbidity() {
  if (!turbConnected) return NAN;
  int tmp[40];
  for (int i = 0; i < 40; i++) { tmp[i] = analogRead(TURB_PIN); delay(2); }
  float raw     = phAverageArray(tmp, 40);
  float voltage = raw * 3.3f / 4095.0f;
  float voltageScaled = voltage * (5.0f / 3.3f);
  float ntu = -1120.4f * voltageScaled * voltageScaled + 5742.3f * voltageScaled - 4353.8f;
  if (ntu < 0.0f) ntu = 0.0f;
  return ntu + TURB_OFFSET;
}

WebServer server(80);

struct SensorState {
  float temperature = NAN;
  float lux         = NAN;
  bool  temp_fault  = false;
  float ph          = NAN;
  float ec_uScm     = NAN;
  float od          = NAN;
  int   led_pwm     = 0;
  bool  led_auto    = LED_MODE_AUTO;
  int   day_start_min = DAY_START_MIN;
  int   day_end_min   = DAY_END_MIN;
  int   ramp_minutes  = RAMP_MINUTES;
  int   led_day_pwm   = LED_DAY_PWM;
  int   led_night_pwm = LED_NIGHT_PWM;
  float temp_soft_limit_c = TEMP_SOFT_LIMIT_C;
  float temp_hard_limit_c = TEMP_HARD_LIMIT_C;
  int   midday_boost_pct = MIDDAY_BOOST_PCT;
  String led_phase = "manual";
  String led_profile = "custom";
  String led_next_change = "—";
  String current_time_str = "—";
  int   led_target_pwm = 0;
  int   thermal_limited_pwm = 0;
  int   thermal_reduction_pct = 0;
  bool  thermal_limit_active = false;
  bool  temp_initialized = false;
  bool  lux_initialized = false;
  unsigned long ts  = 0;
  bool  wifi_ok     = false;
  bool  mqtt_ok     = false;
  bool  ntp_ok      = false;
  int   rssi        = -127;
} state;

static const char HTML_PAGE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bioreattore — sensori</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,sans-serif;background:#0f1117;color:#e0e0e0;min-height:100vh;padding:24px 16px}
  h1{font-size:1.3rem;font-weight:600;color:#fff;margin-bottom:4px}
  .subtitle{font-size:.8rem;color:#666;margin-bottom:24px}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:14px}
  .card,.panel{background:#1a1d27;border-radius:12px;padding:16px;position:relative;overflow:hidden}
  .label{font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:#555;margin-bottom:8px}
  .value{font-size:2rem;font-weight:700;line-height:1;color:#fff}
  .value.fault{color:#ef5350!important;font-size:1rem;margin-top:6px}
  .unit{font-size:.8rem;color:#555;margin-top:4px}
  .panel{margin-top:20px}
  .toggle-line{display:flex;align-items:center;gap:10px;color:#ddd;margin-bottom:14px;font-size:.95rem}
  .hint{margin-top:10px;font-size:.8rem;color:#999}
  .form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:14px}
  .field label{display:block;font-size:.8rem;color:#aaa;margin-bottom:6px}
  .field input{width:100%;padding:8px 10px;border-radius:8px;border:1px solid #333;background:#11141c;color:#fff}
  input[type=range]{width:100%}
  input[type=checkbox]{transform:scale(1.2)}
  button{margin-top:14px;padding:10px 14px;border:none;border-radius:8px;background:#42a5f5;color:white;cursor:pointer}
</style>
</head>
<body>
<h1>Bioreattore</h1>
<div class="subtitle">Sensori + controllo luce damigiana</div>

<div class="grid">
  <div class="card"><div class="label">Ora attuale</div><div class="value" id="clock_now">—</div><div class="unit">Europe/Rome</div></div>
  <div class="card"><div class="label">Fase luce</div><div class="value" id="led_phase">—</div><div class="unit">modalità ciclo</div></div>
  <div class="card"><div class="label">Prossimo cambio</div><div class="value" id="led_next_change">—</div><div class="unit">evento successivo</div></div>
  <div class="card"><div class="label">Temperatura</div><div class="value" id="temperature">—</div><div class="unit">°C</div></div>
  <div class="card"><div class="label">Luminosità</div><div class="value" id="lux">—</div><div class="unit">lux</div></div>
  <div class="card"><div class="label">pH</div><div class="value" id="ph">—</div><div class="unit">&nbsp;</div></div>
  <div class="card"><div class="label">Conducibilità (EC)</div><div class="value" id="ec">—</div><div class="unit">µS/cm</div></div>
  <div class="card"><div class="label">Torbidità (OD proxy)</div><div class="value" id="od">—</div><div class="unit">NTU</div></div>
  <div class="card"><div class="label">LED PWM effettivo</div><div class="value" id="led_pwm">—</div><div class="unit">0–255</div></div>
  <div class="card"><div class="label">LED target</div><div class="value" id="led_target_pwm">—</div><div class="unit">prima dei limiti termici</div></div>
  <div class="card"><div class="label">Riduzione termica</div><div class="value" id="thermal_reduction_pct">—</div><div class="unit">%</div></div>
</div>

<div class="panel" id="ledConfigForm">
  <div class="label" style="margin-bottom:12px">Controllo luce damigiana</div>
  <label class="toggle-line" for="ledAuto">
    <input type="checkbox" id="ledAuto">
    <span>Modalità automatica giorno/notte</span>
  </label>
  <div style="margin-bottom:8px;font-size:.95rem;color:#ddd">Duty manuale: <span id="ledSliderValue">0</span></div>
  <input type="range" min="0" max="255" value="0" id="ledSlider">
  <div class="form-grid">
    <div class="field"><label for="ledProfile">Profilo biologico</label><select id="ledProfile" style="width:100%;padding:8px 10px;border-radius:8px;border:1px solid #333;background:#11141c;color:#fff"><option value="custom">Custom</option><option value="safe">Safe</option><option value="growth">Growth</option><option value="maint">Maintenance</option></select></div>
    <div class="field"><label for="dayStart">Inizio giorno</label><input type="time" id="dayStart"></div>
    <div class="field"><label for="dayEnd">Fine giorno</label><input type="time" id="dayEnd"></div>
    <div class="field"><label for="rampMinutes">Rampa (min)</label><input type="number" id="rampMinutes" min="0" max="180"></div>
    <div class="field"><label for="dayPwm">Duty giorno</label><input type="number" id="dayPwm" min="0" max="255"></div>
    <div class="field"><label for="nightPwm">Duty notte</label><input type="number" id="nightPwm" min="0" max="255"></div>
    <div class="field"><label for="tempSoft">T soft limit (°C)</label><input type="number" id="tempSoft" min="10" max="50" step="0.1"></div>
    <div class="field"><label for="tempHard">T hard limit (°C)</label><input type="number" id="tempHard" min="10" max="50" step="0.1"></div>
    <div class="field"><label for="middayBoost">Boost mezzogiorno (%)</label><input type="number" id="middayBoost" min="50" max="150"></div>
  </div>
  <button onclick="saveLedConfig()">Salva configurazione luce</button>
  <div class="hint">Configurazione persistente al riavvio. Logica biologica: boost moderato nelle ore centrali e riduzione automatica se la temperatura sale troppo.</div>
</div>

<script>
function fmt(v, dec) { if (v === null || v === undefined || isNaN(v)) return '—'; return parseFloat(v).toFixed(dec); }
function minToHHMM(m) { const h = String(Math.floor(m/60)).padStart(2,'0'); const mm = String(m%60).padStart(2,'0'); return `${h}:${mm}`; }
async function refresh() {
  const r = await fetch('/data'); const d = await r.json();
  document.getElementById('clock_now').textContent = d.current_time || '—';
  document.getElementById('led_phase').textContent = d.led_phase || '—';
  document.getElementById('led_next_change').textContent = d.led_next_change || '—';
  const tempEl = document.getElementById('temperature');
  if (d.temp_fault) { tempEl.textContent = 'FAULT'; tempEl.className = 'value fault'; }
  else { tempEl.textContent = fmt(d.temperature, 2); tempEl.className = 'value'; }
  document.getElementById('lux').textContent = d.lux !== null ? fmt(d.lux, 0) : '—';
  document.getElementById('ph').textContent = fmt(d.ph, 2);
  document.getElementById('ec').textContent = d.ec !== null ? fmt(d.ec, 0) : '—';
  document.getElementById('od').textContent = d.od !== null ? fmt(d.od, 0) : '—';
  document.getElementById('led_pwm').textContent = d.led_pwm ?? '—';
  document.getElementById('led_target_pwm').textContent = d.led_target_pwm ?? '—';
  document.getElementById('thermal_reduction_pct').textContent = d.thermal_reduction_pct ?? '—';

  const slider = document.getElementById('ledSlider');
  const sliderValue = document.getElementById('ledSliderValue');
  const autoChk = document.getElementById('ledAuto');
  autoChk.checked = !!d.led_auto;
  slider.disabled = !!d.led_auto;
  if (document.activeElement !== slider) slider.value = d.led_pwm ?? 0;
  sliderValue.textContent = slider.value;

  if (Date.now() > suspendFieldRefreshUntil) {
    if (document.activeElement.id !== 'ledProfile') document.getElementById('ledProfile').value = d.led_profile || 'custom';
    if (document.activeElement.id !== 'dayStart') document.getElementById('dayStart').value = minToHHMM(d.day_start_min || 480);
    if (document.activeElement.id !== 'dayEnd') document.getElementById('dayEnd').value = minToHHMM(d.day_end_min || 1200);
    if (document.activeElement.id !== 'rampMinutes') document.getElementById('rampMinutes').value = d.ramp_minutes ?? 30;
    if (document.activeElement.id !== 'dayPwm') document.getElementById('dayPwm').value = d.led_day_pwm ?? 180;
    if (document.activeElement.id !== 'nightPwm') document.getElementById('nightPwm').value = d.led_night_pwm ?? 0;
    if (document.activeElement.id !== 'tempSoft') document.getElementById('tempSoft').value = d.temp_soft_limit_c ?? 30.0;
    if (document.activeElement.id !== 'tempHard') document.getElementById('tempHard').value = d.temp_hard_limit_c ?? 32.0;
    if (document.activeElement.id !== 'middayBoost') document.getElementById('middayBoost').value = d.midday_boost_pct ?? 100;
  }
}
const slider = document.getElementById('ledSlider');
const sliderValue = document.getElementById('ledSliderValue');
const autoChk = document.getElementById('ledAuto');
let ledDebounce = null;
let suspendFieldRefreshUntil = 0;
document.querySelectorAll('#ledConfigForm input, #ledConfigForm select').forEach(el => {
  el.addEventListener('focus', () => { suspendFieldRefreshUntil = Date.now() + 60000; });
  el.addEventListener('blur', () => { suspendFieldRefreshUntil = Date.now() + 3000; });
});
slider.addEventListener('input', () => { sliderValue.textContent = slider.value; clearTimeout(ledDebounce); ledDebounce = setTimeout(() => setLedPwm(slider.value), 120); });
autoChk.addEventListener('change', async () => { await fetch('/ledmode?auto=' + (autoChk.checked ? '1' : '0')); setTimeout(refresh, 150); });
async function setLedPwm(v) { await fetch('/led?duty=' + encodeURIComponent(v)); setTimeout(refresh, 120); }
async function saveLedConfig() {
  suspendFieldRefreshUntil = Date.now() + 8000;
  const params = new URLSearchParams({
    profile: document.getElementById('ledProfile').value,
    dayStart: document.getElementById('dayStart').value,
    dayEnd: document.getElementById('dayEnd').value,
    ramp: document.getElementById('rampMinutes').value,
    dayPwm: document.getElementById('dayPwm').value,
    nightPwm: document.getElementById('nightPwm').value,
    tempSoft: document.getElementById('tempSoft').value,
    tempHard: document.getElementById('tempHard').value,
    middayBoost: document.getElementById('middayBoost').value,
  });
  const saveBtn = document.getElementById('saveLedBtn');
  const saveStatus = document.getElementById('saveStatus');
  saveBtn.disabled = true;
  saveStatus.textContent = 'Salvataggio in corso...';
  try {
    const r = await fetch('/ledconfig?' + params.toString());
    const j = await r.json();
    if (j.ok) saveStatus.textContent = 'Configurazione salvata (rampa=' + j.ramp_minutes + ', verify=' + j.verifyRamp + ')';
    else saveStatus.textContent = 'Salvataggio fallito';
  } catch(e) {
    saveStatus.textContent = 'Errore salvataggio';
  } finally {
    saveBtn.disabled = false;
    setTimeout(refresh, 150);
  }
}
refresh(); setInterval(refresh, 2000);
</script>
</body>
</html>)rawhtml";

void applyLedControl();
int computeAutoLedPwm();
void loadLedConfig();
void saveLedConfig();
int parseHHMMToMin(const String &hhmm);
String minToHHMMString(int m);
void updateLedCycleStatus();
void applyLedProfile(const String &profile);

void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleSetLed() {
  if (!server.hasArg("duty")) {
    server.send(400, "text/plain", "missing duty");
    return;
  }
  int duty = server.arg("duty").toInt();
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;
  state.led_auto = false;
  state.led_pwm = duty;
  applyLedControl();
  Serial.printf("LED PWM manuale -> duty=%d/255\n", state.led_pwm);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", String("{\"ok\":true,\"led_pwm\":") + state.led_pwm + "}");
}

void handleLedConfig() {
  String incomingProfile = server.hasArg("profile") ? server.arg("profile") : state.led_profile;
  bool validProfile = (incomingProfile == "custom" || incomingProfile == "safe" || incomingProfile == "growth" || incomingProfile == "maint");
  if (!validProfile) incomingProfile = "custom";

  bool userEditedFields = server.hasArg("dayStart") || server.hasArg("dayEnd") || server.hasArg("ramp") ||
                          server.hasArg("dayPwm") || server.hasArg("nightPwm") || server.hasArg("tempSoft") ||
                          server.hasArg("tempHard") || server.hasArg("middayBoost");

  // Presets act as explicit templates: apply only when explicitly selected and different from current profile.
  if (incomingProfile != "custom" && incomingProfile != state.led_profile) {
    applyLedProfile(incomingProfile);
  }

  // Individual submitted fields always override preset values.
  if (server.hasArg("dayStart")) state.day_start_min = parseHHMMToMin(server.arg("dayStart"));
  if (server.hasArg("dayEnd")) state.day_end_min = parseHHMMToMin(server.arg("dayEnd"));
  if (server.hasArg("ramp")) state.ramp_minutes = constrain(server.arg("ramp").toInt(), 0, 180);
  if (server.hasArg("dayPwm")) state.led_day_pwm = constrain(server.arg("dayPwm").toInt(), 0, 255);
  if (server.hasArg("nightPwm")) state.led_night_pwm = constrain(server.arg("nightPwm").toInt(), 0, 255);
  if (server.hasArg("tempSoft")) state.temp_soft_limit_c = constrain(server.arg("tempSoft").toFloat(), 10.0f, 50.0f);
  if (server.hasArg("tempHard")) state.temp_hard_limit_c = constrain(server.arg("tempHard").toFloat(), 10.0f, 50.0f);
  if (server.hasArg("middayBoost")) state.midday_boost_pct = constrain(server.arg("middayBoost").toInt(), 50, 150);

  // If the user manually edits any field, the result becomes custom.
  if (userEditedFields && incomingProfile != "custom") state.led_profile = "custom";
  else state.led_profile = incomingProfile;

  Serial.printf("LED config richiesta -> profile=%s ramp=%d\n", state.led_profile.c_str(), state.ramp_minutes);
  saveLedConfig();

  preferences.begin("bioreactor", true);
  int verifyRamp = preferences.getInt("ramp_min", -999);
  String verifyProfile = preferences.getString("profile", "missing");
  preferences.end();

  applyLedControl();
  Serial.printf("LED config salvata -> profile=%s start=%s end=%s ramp=%d verifyRamp=%d verifyProfile=%s day=%d night=%d soft=%.1f hard=%.1f boost=%d%%\n",
    state.led_profile.c_str(),
    minToHHMMString(state.day_start_min).c_str(),
    minToHHMMString(state.day_end_min).c_str(),
    state.ramp_minutes,
    verifyRamp,
    verifyProfile.c_str(),
    state.led_day_pwm,
    state.led_night_pwm,
    state.temp_soft_limit_c,
    state.temp_hard_limit_c,
    state.midday_boost_pct);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String resp = String("{\"ok\":true,\"ramp_minutes\":") + state.ramp_minutes + ",\"verifyRamp\":" + verifyRamp + ",\"profile\":\"" + state.led_profile + "\",\"verifyProfile\":\"" + verifyProfile + "\"}";
  server.send(200, "application/json", resp);
}

void handleLedMode() {
  bool autoMode = false;
  if (server.hasArg("auto")) autoMode = server.arg("auto").toInt() != 0;
  state.led_auto = autoMode;
  applyLedControl();
  Serial.printf("LED mode -> %s\n", state.led_auto ? "AUTO" : "MANUAL");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", String("{\"ok\":true,\"led_auto\":") + (state.led_auto ? "true" : "false") + "}");
}

void handleData() {
  char buf[1536];
  char t_str[12], l_str[12], ph_str[12], ec_str[12], od_str[12];
  if (isnan(state.temperature) || state.temp_fault) snprintf(t_str, sizeof(t_str), "null");
  else snprintf(t_str, sizeof(t_str), "%.2f", state.temperature);
  if (isnan(state.lux) || state.lux < 0) snprintf(l_str, sizeof(l_str), "null");
  else snprintf(l_str, sizeof(l_str), "%.1f", state.lux);
  if (isnan(state.ph)) snprintf(ph_str, sizeof(ph_str), "null");
  else snprintf(ph_str, sizeof(ph_str), "%.2f", state.ph);
  if (isnan(state.ec_uScm)) snprintf(ec_str, sizeof(ec_str), "null");
  else snprintf(ec_str, sizeof(ec_str), "%.1f", state.ec_uScm);
  if (isnan(state.od)) snprintf(od_str, sizeof(od_str), "null");
  else snprintf(od_str, sizeof(od_str), "%.1f", state.od);

  int written = snprintf(buf, sizeof(buf),
    "{\"temperature\":%s,\"lux\":%s,\"temp_fault\":%s,\"ph\":%s,\"ec\":%s,\"od\":%s,\"led_pwm\":%d,\"led_auto\":%s,\"led_phase\":\"%s\",\"led_profile\":\"%s\",\"led_next_change\":\"%s\",\"current_time\":\"%s\",\"led_target_pwm\":%d,\"thermal_limited_pwm\":%d,\"thermal_reduction_pct\":%d,\"day_start_min\":%d,\"day_end_min\":%d,\"ramp_minutes\":%d,\"led_day_pwm\":%d,\"led_night_pwm\":%d,\"temp_soft_limit_c\":%.1f,\"temp_hard_limit_c\":%.1f,\"midday_boost_pct\":%d,\"ts\":%lu,\"wifi\":%s,\"mqtt\":%s,\"ntp\":%s,\"rssi\":%d}",
    t_str, l_str, state.temp_fault ? "true" : "false", ph_str, ec_str, od_str, state.led_pwm, state.led_auto ? "true" : "false", state.led_phase.c_str(), state.led_profile.c_str(), state.led_next_change.c_str(), state.current_time_str.c_str(), state.led_target_pwm, state.thermal_limited_pwm, state.thermal_reduction_pct, state.day_start_min, state.day_end_min, state.ramp_minutes, state.led_day_pwm, state.led_night_pwm, state.temp_soft_limit_c, state.temp_hard_limit_c, state.midday_boost_pct, state.ts,
    state.wifi_ok ? "true" : "false", state.mqtt_ok ? "true" : "false", state.ntp_ok ? "true" : "false", state.rssi);
  if (written >= (int)sizeof(buf)) Serial.println("⚠️ handleData JSON truncated");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buf);
}

#define BUFFER_SIZE 50
struct SensorMessage { unsigned long ts; float lux, temperature, ph, ec_uScm, od; };
SensorMessage mqttBuffer[BUFFER_SIZE];
int bufferStart = 0, bufferCount = 0;

void reconnectMQTT() {
  static unsigned long lastAttemptMs = 0;
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastAttemptMs < 5000) return;
  lastAttemptMs = millis();
  String id = "ESP32Client-" + String((uint32_t)esp_random(), HEX);
  const char* u = MQTT_USER; const char* p = MQTT_PASSWORD;
  bool ok = (u && u[0]) ? mqttClient.connect(id.c_str(), u, p) : mqttClient.connect(id.c_str());
  if (ok) Serial.println("✅ MQTT connesso");
  else { Serial.print("❌ MQTT fallito, rc="); Serial.println(mqttClient.state()); }
}

void addToBuffer(unsigned long ts, float lux, float temp) {
  SensorMessage m = {ts, lux, temp, state.ph, state.ec_uScm, state.od};
  if (bufferCount < BUFFER_SIZE) mqttBuffer[(bufferStart + bufferCount++) % BUFFER_SIZE] = m;
  else { mqttBuffer[bufferStart] = m; bufferStart = (bufferStart + 1) % BUFFER_SIZE; }
}

void flushBufferMQTT() {
  while (bufferCount > 0 && mqttClient.connected()) {
    SensorMessage &m = mqttBuffer[bufferStart];
    char payload[320], ec_str[12], od_str[12];
    if (isnan(m.ec_uScm)) snprintf(ec_str, sizeof(ec_str), "null"); else snprintf(ec_str, sizeof(ec_str), "%.1f", m.ec_uScm);
    if (isnan(m.od)) snprintf(od_str, sizeof(od_str), "null"); else snprintf(od_str, sizeof(od_str), "%.1f", m.od);
    snprintf(payload, sizeof(payload),
      "{\"ts\":%lu,\"temperature\":%.2f,\"lux\":%.1f,\"ph\":%.2f,\"ec\":%s,\"od\":%s,\"led_pwm\":%d}",
      m.ts, m.temperature, m.lux, m.ph, ec_str, od_str, state.led_pwm);
    Serial.print("📤 MQTT → "); Serial.println(payload);
    if (mqttClient.publish(mqtt_topic, payload)) { bufferStart = (bufferStart + 1) % BUFFER_SIZE; bufferCount--; }
    else { Serial.println("⚠️ Publish fallito"); break; }
  }
}

int parseHHMMToMin(const String &hhmm) {
  int c = hhmm.indexOf(':');
  if (c < 0) return 0;
  int h = hhmm.substring(0, c).toInt();
  int m = hhmm.substring(c + 1).toInt();
  return constrain(h, 0, 23) * 60 + constrain(m, 0, 59);
}

String minToHHMMString(int m) {
  m = constrain(m, 0, 23 * 60 + 59);
  int h = m / 60;
  int mm = m % 60;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", h, mm);
  return String(buf);
}

void updateLedCycleStatus() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    state.current_time_str = "—";
    state.led_next_change = "—";
    state.led_phase = state.led_auto ? "auto" : "manual";
    return;
  }

  int nowMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  state.current_time_str = minToHHMMString(nowMin);

  if (!state.led_auto) {
    state.led_phase = "manual";
    state.led_next_change = "—";
    return;
  }

  int dayStart = state.day_start_min;
  int dayEnd = state.day_end_min;
  int ramp = state.ramp_minutes;

  if (ramp <= 0) {
    if (nowMin >= dayStart && nowMin < dayEnd) {
      state.led_phase = "day";
      state.led_next_change = minToHHMMString(dayEnd);
    } else {
      state.led_phase = "night";
      state.led_next_change = minToHHMMString(dayStart);
    }
    return;
  }

  if (nowMin < dayStart - ramp) {
    state.led_phase = "night";
    state.led_next_change = minToHHMMString(dayStart - ramp);
  } else if (nowMin < dayStart) {
    state.led_phase = "sunrise";
    state.led_next_change = minToHHMMString(dayStart);
  } else if (nowMin < dayEnd) {
    state.led_phase = "day";
    state.led_next_change = minToHHMMString(dayEnd);
  } else if (nowMin < dayEnd + ramp) {
    state.led_phase = "sunset";
    state.led_next_change = minToHHMMString(dayEnd + ramp);
  } else {
    state.led_phase = "night";
    state.led_next_change = minToHHMMString(dayStart - ramp);
  }
}

void applyLedProfile(const String &profile) {
  if (profile == LED_PROFILE_SAFE) {
    state.day_start_min = 8 * 60;
    state.day_end_min = 20 * 60;
    state.ramp_minutes = 45;
    state.led_day_pwm = 160;
    state.led_night_pwm = 0;
    state.temp_soft_limit_c = 28.5f;
    state.temp_hard_limit_c = 30.5f;
    state.midday_boost_pct = 105;
  } else if (profile == LED_PROFILE_GROWTH) {
    state.day_start_min = 7 * 60 + 30;
    state.day_end_min = 20 * 60 + 30;
    state.ramp_minutes = 60;
    state.led_day_pwm = 185;
    state.led_night_pwm = 0;
    state.temp_soft_limit_c = 29.0f;
    state.temp_hard_limit_c = 31.0f;
    state.midday_boost_pct = 110;
  } else if (profile == LED_PROFILE_MAINT) {
    state.day_start_min = 8 * 60;
    state.day_end_min = 19 * 60;
    state.ramp_minutes = 45;
    state.led_day_pwm = 135;
    state.led_night_pwm = 0;
    state.temp_soft_limit_c = 28.0f;
    state.temp_hard_limit_c = 30.0f;
    state.midday_boost_pct = 100;
  }
}

void loadLedConfig() {
  preferences.begin("bioreactor", true);
  state.led_auto = preferences.getBool("led_auto", LED_MODE_AUTO);
  state.day_start_min = preferences.getInt("day_start", DAY_START_MIN);
  state.day_end_min = preferences.getInt("day_end", DAY_END_MIN);
  state.ramp_minutes = preferences.getInt("ramp_min", RAMP_MINUTES);
  state.led_day_pwm = preferences.getInt("day_pwm", LED_DAY_PWM);
  state.led_night_pwm = preferences.getInt("night_pwm", LED_NIGHT_PWM);
  state.temp_soft_limit_c = preferences.getFloat("t_soft", TEMP_SOFT_LIMIT_C);
  state.temp_hard_limit_c = preferences.getFloat("t_hard", TEMP_HARD_LIMIT_C);
  state.midday_boost_pct = preferences.getInt("mid_boost", MIDDAY_BOOST_PCT);
  state.led_profile = preferences.getString("profile", "custom");
  preferences.end();
}

void saveLedConfig() {
  preferences.begin("bioreactor", false);
  preferences.putBool("led_auto", state.led_auto);
  preferences.putInt("day_start", state.day_start_min);
  preferences.putInt("day_end", state.day_end_min);
  preferences.putInt("ramp_min", state.ramp_minutes);
  preferences.putInt("day_pwm", state.led_day_pwm);
  preferences.putInt("night_pwm", state.led_night_pwm);
  preferences.putFloat("t_soft", state.temp_soft_limit_c);
  preferences.putFloat("t_hard", state.temp_hard_limit_c);
  preferences.putInt("mid_boost", state.midday_boost_pct);
  preferences.putString("profile", state.led_profile);
  preferences.end();
}

int computeAutoLedPwm() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return state.led_pwm;
  int nowMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int dayStart = state.day_start_min;
  int dayEnd = state.day_end_min;
  int ramp = state.ramp_minutes;

  int basePwm = state.led_night_pwm;
  state.thermal_reduction_pct = 0;

  if (ramp <= 0) {
    basePwm = (nowMin >= dayStart && nowMin < dayEnd) ? state.led_day_pwm : state.led_night_pwm;
  } else if (nowMin < dayStart - ramp || nowMin >= dayEnd + ramp) {
    basePwm = state.led_night_pwm;
  } else if (nowMin >= dayStart && nowMin < dayEnd) {
    basePwm = state.led_day_pwm;
    int center = (dayStart + dayEnd) / 2;
    int halfWindow = max(1, (dayEnd - dayStart) / 4);
    int dist = abs(nowMin - center);
    if (dist < halfWindow) {
      float x = 1.0f - (float)dist / (float)halfWindow;
      float boost = 1.0f + ((state.midday_boost_pct - 100) / 100.0f) * x;
      basePwm = (int)(basePwm * boost);
    }
  } else if (nowMin >= dayStart - ramp && nowMin < dayStart) {
    float x = float(nowMin - (dayStart - ramp)) / float(ramp);
    basePwm = int(state.led_night_pwm + x * (state.led_day_pwm - state.led_night_pwm));
  } else if (nowMin >= dayEnd && nowMin < dayEnd + ramp) {
    float x = float(nowMin - dayEnd) / float(ramp);
    basePwm = int(state.led_day_pwm + x * (state.led_night_pwm - state.led_day_pwm));
  }

  state.led_target_pwm = constrain(basePwm, 0, 255);

  if (!isnan(state.temperature)) {
    float enterSoft = state.temp_soft_limit_c;
    float exitSoft = state.temp_soft_limit_c - THERMAL_HYST_C;
    if (!state.thermal_limit_active && state.temperature > enterSoft) state.thermal_limit_active = true;
    if (state.thermal_limit_active && state.temperature < exitSoft) state.thermal_limit_active = false;

    if (state.temperature >= state.temp_hard_limit_c) {
      state.thermal_limit_active = true;
      basePwm = state.led_night_pwm;
      state.thermal_reduction_pct = 100;
    } else if (state.thermal_limit_active) {
      float span = state.temp_hard_limit_c - state.temp_soft_limit_c;
      if (span < 0.1f) span = 0.1f;
      float factor = 1.0f - (state.temperature - state.temp_soft_limit_c) / span;
      if (factor < 0.0f) factor = 0.0f;
      if (factor > 1.0f) factor = 1.0f;
      state.thermal_reduction_pct = int((1.0f - factor) * 100.0f);
      basePwm = state.led_night_pwm + int((basePwm - state.led_night_pwm) * factor);
    }
  }

  state.thermal_limited_pwm = constrain(basePwm, 0, 255);
  return state.thermal_limited_pwm;
}

void applyLedControl() {
  int duty = state.led_auto ? computeAutoLedPwm() : state.led_pwm;
  if (!state.led_auto) { state.led_target_pwm = duty; state.thermal_limited_pwm = duty; state.thermal_reduction_pct = 0; }
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;
  state.led_pwm = duty;
  ledcWrite(LED_PWM_PIN, state.led_pwm);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(8, 9);
  Wire.setClock(100000);

  for (int i = 0; i < PH_ARRAY_LEN; i++) phArray[i] = 2048;
  for (int i = 0; i < EC_ARRAY_LEN; i++) ecArray[i] = 2048;
  ecValidSamples = 0;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  Serial.print("Connessione WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(300); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi OK — IP: " + WiFi.localIP().toString());
    Serial.println("🌐 Dashboard: http://" + WiFi.localIP().toString() + "/");
  } else {
    Serial.println("\n⚠️ WiFi timeout, continuo offline (auto-reconnect attivo)");
  }

  configTzTime(TZ_EUROPE_ROME, ntpServer);

  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)
    ? Serial.println("✅ BH1750 OK")
    : Serial.println("⚠️ BH1750 non trovato");

  max31865.begin(MAX31865_3WIRE);

  espClient.setCACert(LE_ROOT_CA);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(384);

  loadLedConfig();
  ledcAttach(LED_PWM_PIN, LED_PWM_FREQ, LED_PWM_RES_BITS);
  state.led_pwm = LED_PWM_DEFAULT;
  applyLedControl();
  Serial.printf("✅ LED PWM pronto su GPIO %d (%d Hz, %d bit)\n", LED_PWM_PIN, LED_PWM_FREQ, LED_PWM_RES_BITS);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/led", HTTP_GET, handleSetLed);
  server.on("/ledmode", HTTP_GET, handleLedMode);
  server.on("/ledconfig", HTTP_GET, handleLedConfig);
  server.begin();
  Serial.println("✅ Web server avviato");
}

void loop() {
  static unsigned long lastSensorMs  = 0;
  static unsigned long lastPublishMs = 0;
  static unsigned long lastLedStepMs = 0;
  static bool firstPublishDone = false;
  static int ledStepIndex = 0;
  const unsigned long SENSOR_INTERVAL_MS = 2000;
  const unsigned long PUBLISH_INTERVAL_MS = 300000;
  const unsigned long LED_STEP_INTERVAL_MS = 3000;

  server.handleClient();
  reconnectMQTT();
  mqttClient.loop();

  unsigned long nowMs = millis();

  if (LED_PWM_TEST_MODE && nowMs - lastLedStepMs >= LED_STEP_INTERVAL_MS) {
    lastLedStepMs = nowMs;
    const int steps[] = {0, 64, 128, 192, 255};
    const int numSteps = sizeof(steps) / sizeof(steps[0]);
    state.led_pwm = steps[ledStepIndex];
    ledcWrite(LED_PWM_PIN, state.led_pwm);
    Serial.printf("LED PWM test → duty=%d/255\n", state.led_pwm);
    ledStepIndex = (ledStepIndex + 1) % numSteps;
  }

  if (state.led_auto) {
    static unsigned long lastAutoUpdateMs = 0;
    if (nowMs - lastAutoUpdateMs >= 10000) {
      lastAutoUpdateMs = nowMs;
      applyLedControl();
    }
  }

  updateLedCycleStatus();

  if (nowMs - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = nowMs;

    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    state.ntp_ok = getLocalTime(&timeinfo, 500);
    if (state.ntp_ok) state.ts = (unsigned long)mktime(&timeinfo);

    float rawLux = lightMeter.readLightLevel();
    if (rawLux >= 0) {
      float newLux = rawLux * LUX_FACTOR;
      if (!state.lux_initialized || isnan(state.lux)) {
        state.lux = newLux;
        state.lux_initialized = true;
      } else {
        state.lux = LUX_EWA_ALPHA * newLux + (1.0f - LUX_EWA_ALPHA) * state.lux;
      }
    }

    state.ph      = readPH();
    state.od      = readTurbidity();

    uint8_t fault = max31865.readFault();
    if (fault) {
      state.temp_fault = true;
      state.temperature = NAN;
      state.temp_initialized = false;
      Serial.printf("⚠️ MAX31865 fault: 0x%02X\n", fault);
      max31865.clearFault();
    } else {
      state.temp_fault = false;
      float newTemp = max31865.temperature(RNOMINAL, RREF);
      if (!state.temp_initialized || isnan(state.temperature)) {
        state.temperature = newTemp;
        state.temp_initialized = true;
      } else if (fabsf(newTemp - state.temperature) <= TEMP_OUTLIER_C) {
        state.temperature = TEMP_EWA_ALPHA * newTemp + (1.0f - TEMP_EWA_ALPHA) * state.temperature;
      } else {
        Serial.printf("⚠️ Temp outlier scartato: %.2f (prev %.2f)\n", newTemp, state.temperature);
      }
    }

    state.ec_uScm = readEC(isnan(state.temperature) ? 25.0f : state.temperature);

    state.wifi_ok = WiFi.status() == WL_CONNECTED;
    state.mqtt_ok = mqttClient.connected();
    state.rssi    = state.wifi_ok ? WiFi.RSSI() : -127;

    Serial.printf("%s | phase=%s next=%s | T=%.2f%s Lux=%.1f pH=%.2f EC=%.0f OD(NTU)=%.0f LEDPWM=%d MODE=%s | WiFi=%s(%ddBm) MQTT=%s NTP=%s\n",
      state.current_time_str.c_str(),
      state.led_phase.c_str(),
      state.led_next_change.c_str(),
      state.temp_fault ? 0.0f : state.temperature,
      state.temp_fault ? "(FAULT)" : "",
      state.lux,
      isnan(state.ph) ? 0.0f : state.ph,
      isnan(state.ec_uScm) ? 0.0f : state.ec_uScm,
      isnan(state.od) ? 0.0f : state.od,
      state.led_pwm,
      state.led_auto ? "AUTO" : "MAN",
      state.wifi_ok ? "OK" : "NO", state.rssi,
      state.mqtt_ok ? "OK" : "NO",
      state.ntp_ok ? "OK" : "NO");

    if (!firstPublishDone && state.ntp_ok && state.mqtt_ok && state.ts > 0) {
      firstPublishDone = true;
      lastPublishMs = nowMs;
      addToBuffer(state.ts, state.lux, state.temperature);
      flushBufferMQTT();
    }
  }

  if (firstPublishDone && nowMs - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = nowMs;
    if (state.ts > 0) {
      addToBuffer(state.ts, state.lux, state.temperature);
      if (mqttClient.connected()) flushBufferMQTT();
    }
  }
}
