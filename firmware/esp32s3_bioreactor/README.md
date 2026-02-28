# ESP32-S3 bioreactor telemetry (Arduino)

Firmware per ESP32-S3 con:
- OLED SSD1306 I2C 128x64 (SDA=IO8, SCL=IO9, addr 0x3C)
- BH1750 (I2C)
- MAX31865 + PT100 (SPI software)
- NTP per timestamp epoch
- MQTT su **MQTTS** (TLS) verso Traefik/Mosquitto

## Wiring (attuale)

### I2C
- SDA → IO8
- SCL → IO9

### MAX31865 (SPI)
- CLK/SCK → IO4
- SDO/MISO → IO5
- SDI/MOSI → IO6
- CS → IO7
- RDY → IO15 (non usato dalla libreria)

## MQTT payload

Il flow Node-RED attuale si aspetta un JSON *piatto* con `ts` (epoch seconds) e campi numerici:

```json
{"ts": 1710000000, "temperature": 22.3, "lux": 120, "ph": 10.0, "do": 8.0, "biomass": 0.45, "od": 0.65, "nitrates": 180}
```

I campi `ph/do/biomass/od/nitrates` sono simulati finché non colleghi i sensori reali.

## Secrets

Copia `secrets.h.example` in `secrets.h` e compila:

```bash
cp secrets.h.example secrets.h
```

**Non committare** `secrets.h`.

## Build / Upload

Esempio con arduino-cli:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306" "Adafruit MAX31865 library" "BH1750" "PubSubClient"
arduino-cli compile --fqbn esp32:esp32:esp32s3 .
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3 .
```
