/*
  ESP32-S3: PT100 via MAX31865 + BH1750 + NTP + MQTT + Web dashboard

  Hardware:
  - I2C su SDA=8, SCL=9 — BH1750
  - MAX31865 software SPI: SCK=IO4, MOSI=IO6, MISO=IO5, CS=IO7

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

#define MAX31865_SCK  4
#define MAX31865_MOSI 6
#define MAX31865_MISO 5
#define MAX31865_CS   7
#define RNOMINAL 100.0
#define RREF     430.0
Adafruit_MAX31865 max31865(MAX31865_CS, MAX31865_MOSI, MAX31865_MISO, MAX31865_SCK);

// === WEB SERVER ===
WebServer server(80);

// === STATO GLOBALE ===
struct SensorState {
  float temperature = NAN;
  float lux         = NAN;
  bool  temp_fault  = false;   // fault MAX31865
  // simulati (placeholder finché non colleghi i sensori reali)
  float ph       = 10.00f;
  float do_mgL   = 8.00f;
  float biomass  = 0.45f;
  float od       = 0.65f;
  float nitrates = 180.0f;
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
  .do      .value{color:#66bb6a}
  .biomass .value{color:#ab47bc}
  .od      .value{color:#26c6da}
  .nitrates .value{color:#ff7043}
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
    <div class="unit">sim</div>
  </div>
  <div class="card do">
    <div class="label">O₂ disciolto</div>
    <div class="value" id="do">—</div>
    <div class="unit">mg/L &nbsp;sim</div>
  </div>
  <div class="card biomass">
    <div class="label">Biomassa</div>
    <div class="value" id="biomass">—</div>
    <div class="unit">g/L &nbsp;sim</div>
  </div>
  <div class="card od">
    <div class="label">OD<sub>750</sub></div>
    <div class="value" id="od">—</div>
    <div class="unit">sim</div>
  </div>
  <div class="card nitrates">
    <div class="label">Nitrati</div>
    <div class="value" id="nitrates">—</div>
    <div class="unit">mg/L &nbsp;sim</div>
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

    document.getElementById('lux').textContent      = d.lux !== null ? fmt(d.lux, 0) : '—';
    document.getElementById('ph').textContent       = fmt(d.ph, 2);
    document.getElementById('do').textContent       = fmt(d.do, 2);
    document.getElementById('biomass').textContent  = fmt(d.biomass, 3);
    document.getElementById('od').textContent       = fmt(d.od, 3);
    document.getElementById('nitrates').textContent = fmt(d.nitrates, 1);

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
  char t_str[12], l_str[12];
  if (isnan(state.temperature) || state.temp_fault)
    snprintf(t_str, sizeof(t_str), "null");
  else
    snprintf(t_str, sizeof(t_str), "%.2f", state.temperature);

  if (isnan(state.lux) || state.lux < 0)
    snprintf(l_str, sizeof(l_str), "null");
  else
    snprintf(l_str, sizeof(l_str), "%.1f", state.lux);

  snprintf(buf, sizeof(buf),
    "{\"temperature\":%s,\"lux\":%s,\"temp_fault\":%s"
    ",\"ph\":%.2f,\"do\":%.2f,\"biomass\":%.3f,\"od\":%.3f,\"nitrates\":%.1f"
    ",\"ts\":%lu,\"wifi\":%s,\"mqtt\":%s,\"ntp\":%s,\"rssi\":%d}",
    t_str, l_str, state.temp_fault ? "true" : "false",
    state.ph, state.do_mgL, state.biomass, state.od, state.nitrates,
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
  float lux, temperature, ph, do_mgL, biomass, od, nitrates;
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

// === SIMULATORE ===
static unsigned long sim_last_ts = 0;
static float clampf(float x, float lo, float hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
void updateSimulation(unsigned long ts, const struct tm& ti, float lux) {
  if (sim_last_ts == 0) { sim_last_ts = ts; return; }
  long dt = (long)ts - (long)sim_last_ts;
  if (dt <= 0) return;
  if (dt > 3600) dt = 3600;
  sim_last_ts = ts;
  float dt_h = dt / 3600.0f;
  bool light_on = (ti.tm_hour >= 6 && ti.tm_hour < 22);
  float luxF = clampf(lux / 300.0f, 0.0f, 1.0f);
  float lF   = light_on ? (0.6f + 0.4f * luxF) : 0.0f;

  state.ph += (9.70f + 0.60f * lF - state.ph) * clampf(0.8f * dt_h, 0.02f, 0.25f);
  state.ph  = clampf(state.ph, 9.2f, 10.8f);

  state.do_mgL += (6.0f + 6.0f * lF - state.do_mgL) * clampf(0.9f * dt_h, 0.02f, 0.35f);
  state.do_mgL  = clampf(state.do_mgL, 2.0f, 18.0f);

  state.nitrates -= (0.30f + 0.40f * lF) * dt_h;
  state.nitrates  = clampf(state.nitrates, 0.0f, 5000.0f);

  state.biomass += ((0.0015f + 0.0025f * lF) - (light_on ? 0.0002f : 0.0008f)) * dt_h;
  state.biomass  = clampf(state.biomass, 0.05f, 5.0f);

  state.od = clampf(state.biomass * 1.45f, 0.05f, 8.0f);
}

// === BUFFER MQTT helpers ===
void addToBuffer(unsigned long ts, float lux, float temp) {
  SensorMessage m = {ts, lux, temp, state.ph, state.do_mgL, state.biomass, state.od, state.nitrates};
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
    snprintf(payload, sizeof(payload),
      "{\"ts\":%lu,\"temperature\":%.2f,\"lux\":%.1f,\"ph\":%.2f"
      ",\"do\":%.2f,\"biomass\":%.3f,\"od\":%.3f,\"nitrates\":%.1f}",
      m.ts, m.temperature, m.lux, m.ph, m.do_mgL, m.biomass, m.od, m.nitrates);
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

    state.lux = lightMeter.readLightLevel();

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

    if (state.ntp_ok) updateSimulation(state.ts, timeinfo, state.lux);

    Serial.printf("T=%.2f%s Lux=%.1f pH=%.2f DO=%.2f NO3=%.1f Bio=%.3f | WiFi=%s(%ddBm) MQTT=%s NTP=%s\n",
      state.temp_fault ? 0.0f : state.temperature,
      state.temp_fault ? "(FAULT)" : "",
      state.lux, state.ph, state.do_mgL, state.nitrates, state.biomass,
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
