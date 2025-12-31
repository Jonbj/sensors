# vlnet-infra

Infrastruttura Docker Compose per:
- IoT stack: MQTT (Mosquitto), Node-RED, InfluxDB, Grafana, Portainer
- Web stack: WordPress + WooCommerce (+ MariaDB + Redis + backup)
- Reverse proxy unico: Traefik con HTTPS automatico (Let’s Encrypt)

## Perché è fatto così (scelte architetturali)
### 1) Traefik unico (proxy/stack dedicato)
Traefik è l’unico componente che espone porte pubbliche (80/443/8883) e gestisce:
- HTTPS automatico (Let’s Encrypt)
- routing per sottodomini
- MQTT TLS su 8883 (terminazione TLS su Traefik)

**Perché:** un solo entrypoint, meno porte esposte, certificati centralizzati.

### 2) Stack separati (proxy / iot / web)
La VM ospita sia IoT che WordPress: WordPress/WooCommerce è più “rischioso” (plugin, bruteforce, carico).
Separare gli stack riduce:
- impatti incrociati (un problema WP non deve fermare IoT)
- errori operativi (update/riavvii/backup)
- complessità di debugging

### 3) Rete `proxy` condivisa + reti interne
- `proxy` (docker network esterna): Traefik + servizi esposti (grafana/nodered/portainer/wp/mqtt)
- reti `internal`: DB e servizi non esposti (InfluxDB, MariaDB, Redis)

**Perché:** minimizzare la superficie di rete.

### 4) Regola d’oro: NON cancellare i volumi del proxy
Traefik salva certificati in `traefik-letsencrypt/acme.json` (volume).
Se fai `docker compose down -v` sul proxy perdi lo storage ACME e Traefik richiederà nuovi cert → rischio rate-limit Let’s Encrypt.

## DNS richiesto
Tutti i record A (o CNAME) devono puntare all’IP della VM:

- `www.vlnet.me`
- `vlnet.me` (redirect a www)
- `grafana.vlnet.me`
- `nodered.vlnet.me`
- `portainer.vlnet.me`
- `mqtt.vlnet.me`

## Porte richieste sul firewall della VM
- 80/tcp (Let’s Encrypt HTTP-01 challenge + redirect)
- 443/tcp (HTTPS)
- 8883/tcp (MQTT TLS)

## Prerequisiti
- Docker Engine + docker compose plugin (Compose v2)
- DNS già propagato
- Porte aperte sul server

## Struttura cartelle
vlnet-infra/
├── proxy/        # Traefik
├── iot/          # MQTT, Node-RED, InfluxDB, Grafana, Portainer
├── web/          # WordPress + WooCommerce
├── scripts/      # start / stop / backup / check
├── backups/      # backup locali
└── docs/         # documentazione


## Config da completare prima del primo avvio
### IoT
- `iot/.env` (password e token Influx, credenziali Grafana)
- `iot/mosquitto/config/passwords` (crearlo PRIMA di avviare mosquitto)
- `iot/nodered-data/settings.js` (inserire hash bcrypt reale)

### Web
- `web/.env` (password DB WordPress)

## Avvio rapido (ordine consigliato)
1) Preflight (controlli)
```bash
./scripts/preflight-check.sh
2) Avvio degli stack
```bash
./scripts/start.sh
3) Verifica servizi
https://www.vlnet.me
https://grafana.vlnet.me
https://nodered.vlnet.me
https://portainer.vlnet.me
MQTT TLS:
host: mqtt.vlnet.me
porta: 8883

## Stop del progetto
```bash
./scripts/stop.sh

## Stop del progetto
Backup manuale completo:

```bash
./scripts/backup-all.sh

I backup vengono salvati in:
backups/iot/
backups/web/

##  Note di sicurezza
MQTT protetto con user/password e TLS
Node-RED e Grafana protetti da login
InfluxDB e MariaDB non esposti pubblicamente
HTTPS automatico con Let’s Encrypt

##  Note operative
usare immagini Docker con versioni fissate (no latest)
aggiornare con docker compose pull && docker compose up -d
non committare file .env o backup
testare periodicamente i backup

##  Stato del progetto
Fase iniziale, ma infrastruttura production-ready.
Il sistema è pensato per crescere senza dover essere riprogettato.

