/*
  ESP32-S3: PT100 via MAX31865 + BH1750 + pH SEN0169-V2 + EC DFR0300 + Torbidità SEN0189 + NTP + MQTT + Web dashboard

  Hardware:
  - I2C su SDA=8, SCL=9 — BH1750
  - MAX31865 software SPI: SCK=IO4, MOSI=IO6, MISO=IO5, CS=IO7
  - pH SEN0169-V2: analogico su IO1
  - EC DFR0300: analogico su IO2 (da collegare)
  - Torbidità SEN0189: analogico su IO3 (da collegare) → proxy OD

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
// Le credenziali reali vanno in secrets.h (non versionato).
// I fallback hardcoded sono rimossi per sicurezza: senza secrets.h il firmware
// non si connette al WiFi, ma compila e gira comunque (offline).
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
// Timezone POSIX string per Europa/Roma: CET-1CEST,M3.5.0,M10.5.0/3
// Gestisce correttamente il cambio ora legale (DST) automaticamente.
#define TZ_EUROPE_ROME "CET-1CEST,M3.5.0,M10.5.0/3"
const char* ntpServer = "pool.ntp.org";

// === SENSORI ===
BH1750 lightMeter;

// === BH1750 calibrazione ===
// Fattore correttivo misurato confrontando con app lux meter su telefono:
// lampada accesa: telefono 616 lux, sensore 202 lux → fattore 3.05
// Nota: warm white 3000K ha spettro diverso dalla luce bianca standard del BH1750.
#define LUX_FACTOR 3.05f

#define MAX31865_SCK  4
#define MAX31865_MOSI 6
#define MAX31865_MISO 5
#define MAX31865_CS   7
#define RNOMINAL 100.0
#define RREF     430.0
Adafruit_MAX31865 max31865(MAX31865_CS, MAX31865_MOSI, MAX31865_MISO, MAX31865_SCK);

// === pH (SEN0169-V2, uscita analogica su IO1) ===
//
// NOTA SCALA: il SEN0169 è progettato per uscita 0-5V (Arduino 5V).
// L'ESP32 legge 0-3.3V → la tensione va riscalata a 5V prima di calcolare il pH.
//
// Calibrazione: regola PH_OFFSET dopo aver misurato una soluzione buffer nota (pH 7.0).
// Es: se legge 7.3 con buffer pH 7.0 → PH_OFFSET = -0.3
#define PH_PIN     1
#define PH_OFFSET  0.0f   // da calibrare con soluzione buffer pH 7.0

// Algoritmo DFRobot ufficiale: 40 campioni, scarta il 25% outlier alto e basso,
// fa media del resto — robusto contro spike e rumore ADC.
#define PH_ARRAY_LEN 40
int phArray[PH_ARRAY_LEN];
int phArrayIndex = 0;

float phAverageArray(int* arr, int len) {
  // Insertion sort
  for (int i = 1; i < len; i++) {
    int key = arr[i], j = i - 1;
    while (j >= 0 && arr[j] > key) { arr[j + 1] = arr[j]; j--; }
    arr[j + 1] = key;
  }
  // Scarta il 25% più basso e più alto
  long sum = 0;
  int from = len / 4, to = len - len / 4;
  for (int i = from; i < to; i++) sum += arr[i];
  return (float)sum / (to - from);
}

float readPH() {
  phArray[phArrayIndex++] = analogRead(PH_PIN);
  if (phArrayIndex >= PH_ARRAY_LEN) phArrayIndex = 0;

  // Copia il buffer per non alterare l'originale durante il sort
  int tmp[PH_ARRAY_LEN];
  memcpy(tmp, phArray, sizeof(phArray));

  float raw     = phAverageArray(tmp, PH_ARRAY_LEN);
  float voltage = raw * 3.3f / 4095.0f;
  // Riscala a 5V (scala originale del SEN0169)
  float voltageScaled = voltage * (5.0f / 3.3f);
  return 7.0f + ((2.5f - voltageScaled) / 0.18f) + PH_OFFSET;
}

// === EC (DFR0300, uscita analogica su IO2) ===
// NOTA: DFR0300 è progettato per 5V — stessa correzione di scala del pH.
// La conversione tensione→EC dipende dalla temperatura (compensazione a 25°C).
// EC_OFFSET: da calibrare con soluzione standard (es. 1413 µS/cm).
// Formula: EC (µS/cm) = (voltageScaled / 0.4) * 1000  (lineare, valida ~0–20 mS/cm)
// ⚠️ Il sensore NON è ancora collegato — restituisce NAN finché IO2 non è connesso.
#define EC_PIN    2
#define EC_OFFSET 0.0f  // da calibrare

#define EC_ARRAY_LEN 40
int ecArray[EC_ARRAY_LEN];
int ecArrayIndex = 0;
bool ecConnected = false;  // impostare true quando il sensore è fisicamente collegato

float readEC(float temperatureC) {
  if (!ecConnected) return NAN;

  ecArray[ecArrayIndex++] = analogRead(EC_PIN);
  if (ecArrayIndex >= EC_ARRAY_LEN) ecArrayIndex = 0;

  int tmp[EC_ARRAY_LEN];
  memcpy(tmp, ecArray, sizeof(ecArray));

  float raw     = phAverageArray(tmp, EC_ARRAY_LEN);  // stessa funzione di averaging
  float voltage = raw * 3.3f / 4095.0f;
  float voltageScaled = voltage * (5.0f / 3.3f);

  // Conversione tensione → EC grezza
  float ecRaw = (voltageScaled / 0.4f) * 1000.0f;  // µS/cm a 25°C

  // Compensazione temperatura (coefficiente standard 2%/°C rispetto a 25°C)
  float tempCoeff = 1.0f + 0.02f * (temperatureC - 25.0f);
  float ec = ecRaw / tempCoeff;

  return ec + EC_OFFSET;
}

// === Torbidità / OD proxy (SEN0189, analogico su IO3) ===
// Il SEN0189 misura torbidità in NTU (uscita analogica inversa: più torbido = tensione più bassa).
// Usato come proxy per OD — non è OD₇₅₀ vero ma correlato alla densità cellulare.
// TURB_OFFSET: da calibrare con campioni a densità nota.
// ⚠️ Non ancora collegato — restituisce NAN finché IO3 non è connesso.
#define TURB_PIN      3
#define TURB_OFFSET   0.0f
bool turbConnected = false;  // impostare true quando collegato

float readTurbidity() {
  if (!turbConnected) return NAN;
  int tmp[40];
  for (int i = 0; i < 40; i++) { tmp[i] = analogRead(TURB_PIN); delay(2); }
  float raw     = phAverageArray(tmp, 40);
  float voltage = raw * 3.3f / 4095.0f;
  float voltageScaled = voltage * (5.0f / 3.3f);
  // SEN0189: NTU = -1120.4*V^2 + 5742.3*V - 4353.8 (formula DFRobot, valida 2.5–4.2V)
  float ntu = -1120.4f * voltageScaled * voltageScaled + 5742.3f * voltageScaled - 4353.8f;
  if (ntu < 0.0f) ntu = 0.0f;
  return ntu + TURB_OFFSET;
}

// === WEB SERVER ===
WebServer server(80);

// === STATO GLOBALE ===
struct SensorState {
  float temperature = NAN;
  float lux         = NAN;
  bool  temp_fault  = false;   // fault MAX31865
  float ph       = NAN;   // reale — SEN0169-V2 su IO1
  float ec_uScm  = NAN;   // reale — DFR0300 su IO2 (da collegare), µS/cm
  float od       = NAN;   // reale — SEN0189 torbidità su IO3 (da collegare), proxy OD
  unsigned long ts  = 0;
  bool  wifi_ok     = false;
  bool  mqtt_ok     = false;
  bool  ntp_ok      = false;
  int   rssi        = -127;
} state;

// === HTML dashboard ===
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
  .card{background:#1a1d27;border-radius:12px;padding:18px 16px;position:relative;overflow:hidden}
  .card::before{content:'';position:absolute;inset:0;border-radius:12px;padding:1px;
    background:linear-gradient(135deg,#2a2d3e,#1a1d27);-webkit-mask:linear-gradient(#fff 0 0) content-box,linear-gradient(#fff 0 0);mask-composite:exclude}
  .label{font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:#555;margin-bottom:8px}
  .value{font-size:2rem;font-weight:700;line-height:1;color:#fff}
  .value.fault{color:#ef5350!important;font-size:1rem;margin-top:6px}
  .unit{font-size:.8rem;color:#555;margin-top:4px}
  .temp    .value{color:#ff7043}
  .lux     .value{color:#fdd835}
  .ph      .value{color:#42a5f5}
  .od      .value{color:#ab47bc}
  .ec      .value{color:#26c6da}
  .status-bar{margin-top:20px;display:flex;gap:12px;flex-wrap:wrap;font-size:.75rem;align-items:center}
  .badge{padding:4px 10px;border-radius:20px;background:#1a1d27}
  .badge.ok{color:#66bb6a}
  .badge.err{color:#ef5350}
  .rssi{font-size:.7rem;color:#555;margin-left:4px}
  .updated{margin-top:14px;font-size:.7rem;color:#444;text-align:right}
</style>
</head>
<body>
<h1>Bioreattore</h1>
<div class="subtitle" id="ts">—</div>

<div class="grid">
  <div class="card temp">
    <div class="label">Temperatura</div>
    <div class="value" id="temperature">—</div>
    <div class="unit">°C</div>
  </div>
  <div class="card lux">
    <div class="label">Luminosità</div>
    <div class="value" id="lux">—</div>
    <div class="unit">lux</div>
  </div>
  <div class="card ph">
    <div class="label">pH</div>
    <div class="value" id="ph">—</div>
    <div class="unit">&nbsp;</div>
  </div>
  <div class="card ec">
    <div class="label">Conducibilità (EC)</div>
    <div class="value" id="ec">—</div>
    <div class="unit">µS/cm</div>
  </div>
  <div class="card od">
    <div class="label">Torbidità (OD proxy)</div>
    <div class="value" id="od">—</div>
    <div class="unit">NTU</div>
  </div>
</div>

<div class="status-bar">
  <span class="badge" id="b-wifi">WiFi —</span>
  <span class="badge" id="b-mqtt">MQTT —</span>
  <span class="badge" id="b-ntp">NTP —</span>
  <span class="rssi" id="rssi"></span>
</div>
<div class="updated" id="updated">—</div>

<script>
function fmt(v, dec) {
  if (v === null || v === undefined || isNaN(v)) return '—';
  return parseFloat(v).toFixed(dec);
}
function badge(el, ok, label) {
  el.textContent = label + (ok ? ' ✓' : ' ✗');
  el.className = 'badge ' + (ok ? 'ok' : 'err');
}

async function refresh() {
  try {
    const r = await fetch('/data');
    if (!r.ok) throw new Error(r.status);
    const d = await r.json();

    const tempEl = document.getElementById('temperature');
    if (d.temp_fault) {
      tempEl.textContent = 'FAULT';
      tempEl.className = 'value fault';
    } else {
      tempEl.textContent = fmt(d.temperature, 2);
      tempEl.className = 'value';
    }

    document.getElementById('lux').textContent = d.lux !== null ? fmt(d.lux, 0) : '—';
    document.getElementById('ph').textContent  = fmt(d.ph, 2);
    document.getElementById('ec').textContent  = d.ec !== null ? fmt(d.ec, 0) : '—';
    document.getElementById('od').textContent  = d.od !== null ? fmt(d.od, 0) : '—';

    badge(document.getElementById('b-wifi'), d.wifi, 'WiFi');
    badge(document.getElementById('b-mqtt'), d.mqtt, 'MQTT');
    badge(document.getElementById('b-ntp'),  d.ntp,  'NTP');

    if (d.rssi && d.wifi) {
      document.getElementById('rssi').textContent = d.rssi + ' dBm';
    }

    if (d.ts) {
      const dt = new Date(d.ts * 1000);
      document.getElementById('ts').textContent = dt.toLocaleString('it-IT');
    }
    document.getElementById('updated').textContent =
      'aggiornato ' + new Date().toLocaleTimeString('it-IT');
  } catch(e) {
    document.getElementById('updated').textContent = 'errore fetch: ' + e;
  }
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>)rawhtml";

// === HANDLER HTTP ===
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleData() {
  char buf[384];
  // Usa "null" per valori NaN — JSON valido e gestibile dal browser
  char t_str[12], l_str[12], ph_str[12];
  if (isnan(state.temperature) || state.temp_fault)
    snprintf(t_str, sizeof(t_str), "null");
  else
    snprintf(t_str, sizeof(t_str), "%.2f", state.temperature);

  if (isnan(state.lux) || state.lux < 0)
    snprintf(l_str, sizeof(l_str), "null");
  else
    snprintf(l_str, sizeof(l_str), "%.1f", state.lux);

  if (isnan(state.ph))
    snprintf(ph_str, sizeof(ph_str), "null");
  else
    snprintf(ph_str, sizeof(ph_str), "%.2f", state.ph);

  char ec_str[12], od_str[12];
  if (isnan(state.ec_uScm)) snprintf(ec_str, sizeof(ec_str), "null");
  else snprintf(ec_str, sizeof(ec_str), "%.1f", state.ec_uScm);

  if (isnan(state.od)) snprintf(od_str, sizeof(od_str), "null");
  else snprintf(od_str, sizeof(od_str), "%.1f", state.od);

  snprintf(buf, sizeof(buf),
    "{\"temperature\":%s,\"lux\":%s,\"temp_fault\":%s"
    ",\"ph\":%s,\"ec\":%s,\"od\":%s"
    ",\"ts\":%lu,\"wifi\":%s,\"mqtt\":%s,\"ntp\":%s,\"rssi\":%d}",
    t_str, l_str, state.temp_fault ? "true" : "false",
    ph_str, ec_str, od_str,
    state.ts,
    state.wifi_ok ? "true" : "false",
    state.mqtt_ok ? "true" : "false",
    state.ntp_ok  ? "true" : "false",
    state.rssi
  );
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buf);
}

// === BUFFER MQTT ===
#define BUFFER_SIZE 50
struct SensorMessage {
  unsigned long ts;
  float lux, temperature, ph, ec_uScm, od;
};
SensorMessage mqttBuffer[BUFFER_SIZE];
int bufferStart = 0, bufferCount = 0;

// === MQTT reconnect ===
void reconnectMQTT() {
  static unsigned long lastAttemptMs = 0;
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastAttemptMs < 5000) return;
  lastAttemptMs = millis();
  String id = "ESP32Client-" + String((uint32_t)esp_random(), HEX);
  const char* u = MQTT_USER; const char* p = MQTT_PASSWORD;
  bool ok = (u && u[0]) ? mqttClient.connect(id.c_str(), u, p)
                        : mqttClient.connect(id.c_str());
  if (ok) {
    Serial.println("✅ MQTT connesso");
  } else {
    Serial.print("❌ MQTT fallito, rc=");
    Serial.println(mqttClient.state());
  }
}

// Simulatore rimosso — tutti i KPI sono ora reali o non misurati.

// === BUFFER MQTT helpers ===
void addToBuffer(unsigned long ts, float lux, float temp) {
  SensorMessage m = {ts, lux, temp, state.ph, state.ec_uScm, state.od};
  if (bufferCount < BUFFER_SIZE) {
    mqttBuffer[(bufferStart + bufferCount++) % BUFFER_SIZE] = m;
  } else {
    mqttBuffer[bufferStart] = m;
    bufferStart = (bufferStart + 1) % BUFFER_SIZE;
  }
}
void flushBufferMQTT() {
  while (bufferCount > 0 && mqttClient.connected()) {
    SensorMessage &m = mqttBuffer[bufferStart];
    char payload[320];
    char ec_str[12], od_str[12];
    if (isnan(m.ec_uScm)) snprintf(ec_str, sizeof(ec_str), "null");
    else snprintf(ec_str, sizeof(ec_str), "%.1f", m.ec_uScm);
    if (isnan(m.od)) snprintf(od_str, sizeof(od_str), "null");
    else snprintf(od_str, sizeof(od_str), "%.1f", m.od);
    snprintf(payload, sizeof(payload),
      "{\"ts\":%lu,\"temperature\":%.2f,\"lux\":%.1f,\"ph\":%.2f"
      ",\"ec\":%s,\"od\":%s}",
      m.ts, m.temperature, m.lux, m.ph, ec_str, od_str);
    Serial.print("📤 MQTT → "); Serial.println(payload);
    if (mqttClient.publish(mqtt_topic, payload)) {
      bufferStart = (bufferStart + 1) % BUFFER_SIZE;
      bufferCount--;
    } else { Serial.println("⚠️ Publish fallito"); break; }
  }
}

// ================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(8, 9);
  Wire.setClock(100000);

  // WiFi con auto-reconnect esplicito
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  Serial.print("Connessione WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(300); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi OK — IP: " + WiFi.localIP().toString());
    Serial.println("🌐 Dashboard: http://" + WiFi.localIP().toString() + "/");
  } else {
    Serial.println("\n⚠️ WiFi timeout, continuo offline (auto-reconnect attivo)");
  }

  // NTP con timezone POSIX (gestisce DST automaticamente)
  configTzTime(TZ_EUROPE_ROME, ntpServer);

  // BH1750
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)
    ? Serial.println("✅ BH1750 OK")
    : Serial.println("⚠️ BH1750 non trovato");

  // MAX31865
  max31865.begin(MAX31865_3WIRE);

  // MQTT TLS
  espClient.setCACert(LE_ROOT_CA);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(256);

  // Web server
  server.on("/",     HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.begin();
  Serial.println("✅ Web server avviato");
}

void loop() {
  static unsigned long lastSensorMs  = 0;
  static unsigned long lastPublishMs = 0;
  static bool firstPublishDone       = false;
  const unsigned long SENSOR_INTERVAL_MS  = 2000;
  const unsigned long PUBLISH_INTERVAL_MS = 300000;

  server.handleClient();
  reconnectMQTT();
  mqttClient.loop();

  unsigned long nowMs = millis();

  // --- lettura sensori ogni 2s ---
  if (nowMs - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = nowMs;

    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    state.ntp_ok = getLocalTime(&timeinfo, 500);
    if (state.ntp_ok) state.ts = (unsigned long)mktime(&timeinfo);

    state.lux     = lightMeter.readLightLevel() * LUX_FACTOR;
    state.ph      = readPH();
    state.ec_uScm = readEC(isnan(state.temperature) ? 25.0f : state.temperature);
    state.od      = readTurbidity();

    // MAX31865: controlla fault prima di leggere la temperatura
    uint8_t fault = max31865.readFault();
    if (fault) {
      state.temp_fault = true;
      state.temperature = NAN;
      Serial.printf("⚠️ MAX31865 fault: 0x%02X", fault);
      if (fault & MAX31865_FAULT_HIGHTHRESH)  Serial.print(" [HIGH_THRESH]");
      if (fault & MAX31865_FAULT_LOWTHRESH)   Serial.print(" [LOW_THRESH]");
      if (fault & MAX31865_FAULT_REFINLOW)    Serial.print(" [REFINLOW]");
      if (fault & MAX31865_FAULT_REFINHIGH)   Serial.print(" [REFINHIGH]");
      if (fault & MAX31865_FAULT_RTDINLOW)    Serial.print(" [RTDINLOW]");
      if (fault & MAX31865_FAULT_OVUV)        Serial.print(" [OV/UV]");
      Serial.println();
      max31865.clearFault();
    } else {
      state.temp_fault  = false;
      state.temperature = max31865.temperature(RNOMINAL, RREF);
    }

    state.wifi_ok = WiFi.status() == WL_CONNECTED;
    state.mqtt_ok = mqttClient.connected();
    state.rssi    = state.wifi_ok ? WiFi.RSSI() : -127;

    Serial.printf("T=%.2f%s Lux=%.1f pH=%.2f EC=%.0f OD(NTU)=%.0f | WiFi=%s(%ddBm) MQTT=%s NTP=%s\n",
      state.temp_fault ? 0.0f : state.temperature,
      state.temp_fault ? "(FAULT)" : "",
      state.lux,
      isnan(state.ph) ? 0.0f : state.ph,
      isnan(state.ec_uScm) ? 0.0f : state.ec_uScm,
      isnan(state.od) ? 0.0f : state.od,
      state.wifi_ok ? "OK" : "NO", state.rssi,
      state.mqtt_ok ? "OK" : "NO",
      state.ntp_ok  ? "OK" : "NO");

    // Primo publish non appena NTP e MQTT sono pronti
    if (!firstPublishDone && state.ntp_ok && state.mqtt_ok && state.ts > 0) {
      firstPublishDone = true;
      lastPublishMs = nowMs;
      addToBuffer(state.ts, state.lux, state.temperature);
      flushBufferMQTT();
    }
  }

  // --- publish MQTT ogni 5 min ---
  if (firstPublishDone && nowMs - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = nowMs;
    if (state.ts > 0) {
      addToBuffer(state.ts, state.lux, state.temperature);
      if (mqttClient.connected()) flushBufferMQTT();
    }
  }
}
