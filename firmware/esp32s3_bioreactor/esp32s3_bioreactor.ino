/*
  ESP32-S3: OLED (SSD1306 I2C) + PT100 via MAX31865 + BH1750 + NTP + MQTT

  NOTE IMPORTANTI (hardware):
  - I2C qui è su SDA=8, SCL=9 (Wire.begin(8,9))
  - MAX31865 è configurato in *software SPI* sui pin definiti sotto.
    Su ESP32-S3 i pin SPI "comodi" spesso NON sono 10-13: se la temperatura non torna,
    quasi certamente va corretto il mapping dei pin (o passare a HW SPI).

  Sicurezza:
  - Le credenziali WiFi/MQTT vanno idealmente in un file non versionato (secrets.h).
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX31865.h>
#include <BH1750.h>
#include <time.h>
#include <PubSubClient.h>

// === TLS CA (Let's Encrypt ISRG Root X1) ===
// Source: https://letsencrypt.org/certificates/
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

// === OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === WIFI / MQTT credentials ===
// Suggerito: spostare credenziali in un file separato (secrets.h) non versionato.
// Se non esiste, usa i fallback qui sotto.

#ifdef __has_include
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "FRITZ!Box 7530 ZL"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "79987700587151914856"
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

// === MQTT (via Traefik, TLS) ===
const char* mqtt_server = "mqtt.vlnet.me";
const int mqtt_port = 8883; // Traefik entrypoint mqtts
const char* mqtt_topic = "sensors/esp32/data";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// === NTP ===
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// === BH1750 ===
BH1750 lightMeter;

// === MAX31865 (software SPI) ===
// Cablaggio dichiarato: IO4, IO5, IO6, IO7, IO15.
// Un MAX31865 usa tipicamente 4 linee SPI + CS (5 fili): SCK, MOSI(SDI), MISO(SDO), CS, (opzionale) RDY.
// Qui assumo la mappatura più comune/lineare:
//   SCK=IO4, MOSI=IO5, MISO=IO6, CS=IO7
//   IO15 (se collegato) potrebbe essere RDY/DRDY ma questa libreria non lo usa.
// Se non leggi temperature plausibili, dimmi esattamente quale filo va su quale pin del breakout.
// Mapping trovato dal probe: SCK=IO4, MOSI=IO6, MISO=IO5, CS=IO7
#define MAX31865_SCK  4
#define MAX31865_MOSI 6
#define MAX31865_MISO 5
#define MAX31865_CS   7

#define RNOMINAL 100.0
#define RREF     430.0

Adafruit_MAX31865 max31865(MAX31865_CS, MAX31865_MOSI, MAX31865_MISO, MAX31865_SCK);

// === BUFFER ===
#define BUFFER_SIZE 50
struct SensorMessage {
  unsigned long ts;       // epoch seconds
  float lux;
  float temperature;
  // simulated (placeholder) fields until sensors are wired
  float ph;
  float do_mgL;
  float biomass;
  float od;
  float nitrates;
};
SensorMessage buffer[BUFFER_SIZE];
int bufferStart = 0;
int bufferCount = 0;

// === UTIL ===
int wifiBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

void drawWiFiBars(int x, int y, int bars) {
  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 4;
    if (i < bars) display.fillRect(x + i * 6, y - h, 4, h, SSD1306_WHITE);
    else display.drawRect(x + i * 6, y - h, 4, h, SSD1306_WHITE);
  }
}

void reconnectMQTT() {
  // IMPORTANTISSIMO: non bloccare mai il loop.
  // Facciamo un solo tentativo ogni N secondi.
  static unsigned long lastAttemptMs = 0;
  const unsigned long retryEveryMs = 5000;

  if (client.connected()) return;

  if (WiFi.status() != WL_CONNECTED) {
    // niente WiFi -> niente tentativi
    return;
  }

  unsigned long now = millis();
  if (now - lastAttemptMs < retryEveryMs) return;
  lastAttemptMs = now;

  String clientId = "ESP32Client-" + String((uint32_t)esp_random(), HEX);
  const char* user = MQTT_USER;
  const char* pass = MQTT_PASSWORD;

  bool ok;
  if (user && user[0] != '\0') {
    ok = client.connect(clientId.c_str(), user, pass);
  } else {
    ok = client.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("✅ MQTT connesso");
  } else {
    Serial.print("❌ MQTT fallito, rc=");
    Serial.println(client.state());
  }
}

// --- Simulatore (placeholder finché non colleghi i sensori reali) ---
// Obiettivo: valori "realistici" con pattern giorno/notte.
// Nota: usa l'orario NTP (se non c'è, comunque non inviamo).

static float sim_ph = 10.00f;
static float sim_do = 8.0f;          // mg/L
static float sim_biomass = 0.45f;    // g/L
static float sim_od = 0.65f;         // ~proporzionale alla biomassa
static float sim_nitrates = 180.0f;  // mg/L
static unsigned long sim_last_ts = 0;

static float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static void updateSimulation(unsigned long ts, const struct tm& timeinfo, float lux) {
  // Integratore semplice: aggiorna gli stati in base al delta tempo
  if (sim_last_ts == 0) {
    sim_last_ts = ts;
    return;
  }

  long dt = (long)ts - (long)sim_last_ts;
  if (dt <= 0) return;
  if (dt > 3600) dt = 3600; // evita salti enormi
  sim_last_ts = ts;

  const float dt_h = dt / 3600.0f;

  // Fotoperiodo: 16h ON (06:00–22:00), 8h OFF
  const int h = timeinfo.tm_hour;
  const bool light_on = (h >= 6 && h < 22);

  // Inoltre usa un fattore continuo basato su lux (normalizzato) per dare variabilità reale.
  // lux tipici in ambiente interno: 0..500+. qui lo clampiamo a 0..1 su 0..300.
  const float luxFactor = clampf(lux / 300.0f, 0.0f, 1.0f);
  const float lightFactor = light_on ? (0.6f + 0.4f * luxFactor) : 0.0f;

  // pH: base ~9.7, sale di giorno (fotosintesi) fino ~10.3
  const float ph_target = 9.70f + 0.60f * lightFactor;
  sim_ph += (ph_target - sim_ph) * clampf(0.8f * dt_h, 0.02f, 0.25f);
  sim_ph = clampf(sim_ph, 9.2f, 10.8f);

  // DO: più alto di giorno. target 6..12 mg/L
  const float do_target = 6.0f + 6.0f * lightFactor;
  sim_do += (do_target - sim_do) * clampf(0.9f * dt_h, 0.02f, 0.35f);
  sim_do = clampf(sim_do, 2.0f, 18.0f);

  // Nitrati: consumo lento nel tempo, più consumo di giorno
  const float no3_consumption_per_h = 0.30f + 0.40f * lightFactor; // mg/L/h
  sim_nitrates -= no3_consumption_per_h * dt_h;
  sim_nitrates = clampf(sim_nitrates, 0.0f, 5000.0f);

  // Biomassa: crescita lenta (soprattutto di giorno)
  const float growth_per_h = 0.0015f + 0.0025f * lightFactor; // g/L/h
  const float loss_per_h = light_on ? 0.0002f : 0.0008f;      // g/L/h
  sim_biomass += (growth_per_h - loss_per_h) * dt_h;
  sim_biomass = clampf(sim_biomass, 0.05f, 5.0f);

  // OD: proporzionale (semplificato) alla biomassa
  sim_od = clampf(sim_biomass * 1.45f, 0.05f, 8.0f);
}

void addToBuffer(unsigned long ts, float lux, float temp) {
  SensorMessage m;
  m.ts = ts;
  m.lux = lux;
  m.temperature = temp;
  m.ph = sim_ph;
  m.do_mgL = sim_do;
  m.biomass = sim_biomass;
  m.od = sim_od;
  m.nitrates = sim_nitrates;

  if (bufferCount < BUFFER_SIZE) {
    int idx = (bufferStart + bufferCount) % BUFFER_SIZE;
    buffer[idx] = m;
    bufferCount++;
  } else {
    buffer[bufferStart] = m;
    bufferStart = (bufferStart + 1) % BUFFER_SIZE;
  }

  Serial.print("🧠 Buffer+ → ts: ");
  Serial.print(ts);
  Serial.print(" | Lux: ");
  Serial.print(lux);
  Serial.print(" | Temp: ");
  Serial.print(temp);
  Serial.print(" | pH(sim): ");
  Serial.println(sim_ph, 2);
}

void flushBufferMQTT() {
  while (bufferCount > 0 && client.connected()) {
    SensorMessage &msg = buffer[bufferStart];

    // Node-RED flow expects a *flat* JSON with `ts` in seconds.
    // Everything else becomes fields in Influx.
    char payload[320];
    snprintf(
        payload,
        sizeof(payload),
        "{\"ts\":%lu,\"temperature\":%.2f,\"lux\":%.1f,\"ph\":%.2f,\"do\":%.2f,\"biomass\":%.3f,\"od\":%.3f,\"nitrates\":%.1f}",
        msg.ts,
        msg.temperature,
        msg.lux,
        msg.ph,
        msg.do_mgL,
        msg.biomass,
        msg.od,
        msg.nitrates
    );

    Serial.print("📤 MQTT → ");
    Serial.println(payload);

    if (client.publish(mqtt_topic, payload)) {
      bufferStart = (bufferStart + 1) % BUFFER_SIZE;
      bufferCount--;
    } else {
      Serial.println("⚠️ Publish fallito, interrompo flush");
      break;
    }
  }

  Serial.print("📦 Buffer rimanente: ");
  Serial.println(bufferCount);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // I2C
  Wire.begin(8, 9);
  Wire.setClock(100000);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ OLED init fallita");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ESP32-S3");
  display.println("Connessione WiFi...");
  display.display();

  // WiFi (con timeout)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - t0 > 20000) {
      Serial.println("⚠️ WiFi timeout (20s), continuo offline");
      break;
    }
  }

  // NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // BH1750
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("⚠️ BH1750 non trovato su I2C");
  } else {
    Serial.println("✅ BH1750 OK");
  }

  // MAX31865
  max31865.begin(MAX31865_3WIRE);

  // MQTT TLS
  // Verifica certificato (Let's Encrypt ISRG Root X1)
  espClient.setCACert(LE_ROOT_CA);
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(256);

  Serial.println("✅ Setup completato");
}

void loop() {
  // Decoupling:
  // - Display refresh + sensor read: fast
  // - MQTT publish: slow (5 min)
  static unsigned long lastUiMs = 0;
  static unsigned long lastPublishMs = 0;

  const unsigned long UI_INTERVAL_MS = 2000;       // 2s refresh UI
  const unsigned long PUBLISH_INTERVAL_MS = 300000; // 5 min

  // keep MQTT client responsive
  reconnectMQTT();
  client.loop();

  unsigned long nowMs = millis();
  const bool doUi = (nowMs - lastUiMs) >= UI_INTERVAL_MS;
  const bool doPublish = (nowMs - lastPublishMs) >= PUBLISH_INTERVAL_MS;

  if (!doUi && !doPublish) {
    delay(10);
    return;
  }

  if (doUi) lastUiMs = nowMs;
  if (doPublish) lastPublishMs = nowMs;

  // TIME
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(timeinfo));

  unsigned long timestamp = 0;
  bool haveTime = getLocalTime(&timeinfo, 1000 /*ms*/);
  if (haveTime) {
    // Epoch seconds (localtime->mktime)
    timestamp = (unsigned long)mktime(&timeinfo);
  } else {
    Serial.println("⚠️ NTP non disponibile: salto invio (nessun timestamp valido)");
  }

  // SENSORI
  float lux = lightMeter.readLightLevel();
  float temp = max31865.temperature(RNOMINAL, RREF);

  // Aggiorna simulazione (placeholder)
  updateSimulation(timestamp, timeinfo, lux);

  // Debug seriale (utile per verificare che il loop giri)
  Serial.print("Lux="); Serial.print(lux, 1);
  Serial.print(" | Temp="); Serial.print(temp, 2);
  Serial.print(" | pH="); Serial.print(sim_ph, 2);
  Serial.print(" | DO="); Serial.print(sim_do, 2);
  Serial.print(" | NO3="); Serial.print(sim_nitrates, 1);
  Serial.print(" | Bio="); Serial.print(sim_biomass, 3);
  Serial.print(" | WiFi="); Serial.print((WiFi.status()==WL_CONNECTED)?"OK":"NO");
  Serial.print(" RSSI="); Serial.println((WiFi.status()==WL_CONNECTED)?WiFi.RSSI():-127);

  int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  int bars = wifiBars(rssi);

  // OLED
  display.clearDisplay();

  // UNICA PAGINA, più leggibile:
  // - Riga 1 in grande: temperatura + lux
  // - Resto in piccolo

  // Riga 1 (grande)
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (temp < -200 || temp > 850 || isnan(temp)) {
    display.print("T:--");
  } else {
    // es: T:22.1
    display.print("T:");
    display.print(temp, 1);
  }

  // pH a destra (al posto di Lux)
  display.setCursor(78, 0);
  display.print("pH:");
  display.print(sim_ph, 1);

  // Barre WiFi: spostate SOTTO la riga T/L per non sovrapporsi a L.
  drawWiFiBars(104, 34, bars);

  // Riga 2: pH + DO etichette
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.print("pH");
  display.setCursor(64, 18);
  display.print("DO");

  // Riga 3: pH + DO valori grandi
  display.setTextSize(2);
  display.setCursor(0, 26);
  display.print(sim_ph, 1);
  display.setCursor(64, 26);
  display.print(sim_do, 1);

  // Riga 4: etichette per NO3/Bio/OD (stesso formato: label sopra value)
  display.setTextSize(1);
  display.setCursor(0, 44);
  display.print("NO3");
  display.setCursor(54, 44);
  display.print("Bio");
  display.setCursor(96, 44);
  display.print("OD");

  // Riga 5: valori per NO3/Bio/OD
  display.setCursor(0, 54);
  display.print(sim_nitrates, 0);
  display.setCursor(54, 54);
  display.print(sim_biomass, 2);
  display.setCursor(96, 54);
  display.print(sim_od, 2);

  // (MQTT status rimosso: non necessario sul display)

  display.display();

  // Se non ho un timestamp valido, non pubblico.
  if (timestamp == 0) {
    // però posso comunque mostrare i sensori (anche senza ora)
    return;
  }

  // Pubblica solo ogni PUBLISH_INTERVAL_MS
  if (doPublish) {
    addToBuffer(timestamp, lux, temp);
    if (client.connected()) {
      flushBufferMQTT();
    }
  }
}
