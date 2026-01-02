# Bootstrap iniziale dell’infrastruttura

Questo documento descrive come inizializzare **da zero** l’infrastruttura VLNet.

È pensato esclusivamente per:
- una VM nuova
- una directory di lavoro vuota
- la **prima creazione** del progetto

**Attenzione**
- usare questo documento **una sola volta**
- **non rieseguire** il bootstrap su un ambiente già avviato

---

## Scopo del bootstrap

Il bootstrap serve a:

- creare la struttura delle cartelle del progetto
- generare i file Docker Compose iniziali
- preparare template di configurazione sicuri
- impostare una base pronta per un utilizzo in produzione

Dopo il bootstrap:
- l’infrastruttura viene gestita tramite gli script presenti nella directory `scripts`
- questo file resta come riferimento storico e operativo

---

## Prerequisiti

Prima di iniziare assicurarsi che sulla VM siano presenti:

- Docker Engine installato e funzionante
- Docker Compose (plugin v2)
- accesso root o sudo
- DNS dei domini configurato verso l’IP pubblico della VM
- porte 80, 443 e 8883 aperte sul firewall

---

## Esecuzione del bootstrap

Il bootstrap viene eseguito tramite uno script dedicato.

Passaggi:

- posizionarsi nella directory desiderata
- rendere eseguibile lo script di bootstrap
- eseguire lo script

Comandi da eseguire:

- `chmod +x bootstrap_vlnet_infra.sh`
- `./bootstrap_vlnet_infra.sh`

Al termine verrà creata la directory:

- `vlnet-infra`

contenente l’intera struttura del progetto.

---

## Struttura generata

La struttura creata dal bootstrap è la seguente:

- `vlnet-infra/`
  - `proxy/` – Traefik
  - `iot/` – MQTT, Node-RED, InfluxDB, Grafana, Portainer
  - `web/` – WordPress + WooCommerce
  - `scripts/` – script di gestione (start, stop, backup, check)
  - `backups/` – directory per i backup
  - `docs/` – documentazione

---

## Passaggi manuali obbligatori dopo il bootstrap

Il bootstrap **non completa** la configurazione dell’ambiente.

I passaggi seguenti sono **obbligatori**.

---

### 1) Configurazione delle variabili segrete

Modificare i seguenti file inserendo password e token **forti**:

- `iot/.env`
- `web/.env`

Questi file:
- contengono segreti
- **non devono essere committati** nel repository

---

### 2) Creazione utenti MQTT (Mosquitto)

Mosquitto è configurato con autenticazione obbligatoria.

Senza il file degli utenti:
- il container **non si avvia**

Operazioni richieste:

- entrare nella directory `iot`
- creare il file utenti con `mosquitto_passwd`
- assicurarsi che i permessi siano corretti

Esempio logico:
- creare l’utente `sensor1`
- assegnare una password sicura

---

### 3) Protezione di Node-RED

Node-RED è protetto tramite autenticazione amministrativa.

Passaggi richiesti:

- avviare temporaneamente lo stack IoT
- generare un hash bcrypt della password admin
- inserire l’hash nel file `iot/nodered-data/settings.js`
- riavviare il servizio Node-RED

Senza questo passaggio:
- Node-RED rimane accessibile senza autenticazione

---

### 4) Verifica DNS e prerequisiti

Prima del primo avvio completo eseguire:

- `scripts/preflight-check.sh`

Questo script verifica:

- presenza dei file di configurazione richiesti
- risoluzione DNS dei domini
- prerequisiti minimi del sistema

---

## Primo avvio dell’infrastruttura

Dopo aver completato i passaggi manuali:

1. Avviare l’infrastruttura tramite:
   - `scripts/start.sh`

2. Attendere alcuni minuti per:
   - l’ottenimento dei certificati Let’s Encrypt
   - l’avvio completo di tutti i servizi

3. Verificare l’accesso:
   - alle interfacce web
   - al broker MQTT tramite TLS

---

## Cosa NON fare

Per evitare problemi gravi e difficili da recuperare:

- non rieseguire lo script di bootstrap
- non cancellare i volumi Docker dello stack proxy
- non eseguire `docker compose down -v` sullo stack proxy
- non committare file `.env` o directory di backup

---

## Dopo il bootstrap

Una volta completata l’inizializzazione:

- usare esclusivamente gli script nella directory `scripts`
- gestire avvio e stop con `start.sh` e `stop.sh`
- eseguire backup regolari
- conservare questo documento come riferimento operativo

---

## Nota finale

Questo documento è volutamente dettagliato.

In un’infrastruttura di produzione:
- la chiarezza è più importante della brevità
- prevenire errori è più importante dell’automazione totale
