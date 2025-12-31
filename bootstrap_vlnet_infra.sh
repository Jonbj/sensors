#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="vlnet-infra"

# Fail-fast if root dir exists (to avoid overwriting)
if [[ -e "$ROOT_DIR" ]]; then
  echo "ERROR: '$ROOT_DIR' already exists. Rename/remove it or run in an empty folder."
  exit 1
fi

echo "Creating folder structure..."
mkdir -p \
  "$ROOT_DIR/proxy/traefik" \
  "$ROOT_DIR/iot/mosquitto/config" \
  "$ROOT_DIR/iot/mosquitto/data" \
  "$ROOT_DIR/iot/mosquitto/log" \
  "$ROOT_DIR/iot/nodered-data" \
  "$ROOT_DIR/web/backups"

echo "Writing files..."

########################################
# PROXY: docker-compose.yml
########################################
cat > "$ROOT_DIR/proxy/docker-compose.yml" <<'YAML'
services:
  traefik:
    image: traefik:v3.0
    container_name: traefik
    command:
      - "--providers.docker=true"
      - "--providers.docker.exposedbydefault=false"

      - "--entrypoints.web.address=:80"
      - "--entrypoints.websecure.address=:443"
      - "--entrypoints.mqtts.address=:8883"

      - "--entrypoints.web.http.redirections.entrypoint.to=websecure"
      - "--entrypoints.web.http.redirections.entrypoint.scheme=https"

      - "--certificatesresolvers.le.acme.email=admin@vlnet.me"
      - "--certificatesresolvers.le.acme.storage=/letsencrypt/acme.json"
      - "--certificatesresolvers.le.acme.httpchallenge.entrypoint=web"

    ports:
      - "80:80"
      - "443:443"
      - "8883:8883"

    volumes:
      - /var/run/docker.sock:/var/run/docker.sock:ro
      - traefik-letsencrypt:/letsencrypt

    networks:
      - proxy

    restart: unless-stopped

networks:
  proxy:
    name: proxy
    external: true

volumes:
  traefik-letsencrypt:
    name: traefik-letsencrypt
YAML

########################################
# IOT: docker-compose.yml
########################################
cat > "$ROOT_DIR/iot/docker-compose.yml" <<'YAML'
services:

  mqtt:
    image: eclipse-mosquitto:2.0.18
    container_name: mqtt-broker
    volumes:
      - ./mosquitto/config:/mosquitto/config:ro
      - ./mosquitto/data:/mosquitto/data
      - ./mosquitto/log:/mosquitto/log
    labels:
      - "traefik.enable=true"
      - "traefik.tcp.routers.mqtts.rule=HostSNI(`mqtt.vlnet.me`)"
      - "traefik.tcp.routers.mqtts.entrypoints=mqtts"
      - "traefik.tcp.routers.mqtts.tls=true"
      - "traefik.tcp.routers.mqtts.tls.certresolver=le"
      - "traefik.tcp.services.mqtts.loadbalancer.server.port=1883"
    networks:
      - proxy
      - internal
    restart: unless-stopped

  influxdb:
    image: influxdb:2.7
    container_name: influxdb
    volumes:
      - influxdb-data:/var/lib/influxdb2
      - influxdb-backup:/backup
    environment:
      DOCKER_INFLUXDB_INIT_MODE: setup
      DOCKER_INFLUXDB_INIT_USERNAME: ${INFLUXDB_USER}
      DOCKER_INFLUXDB_INIT_PASSWORD: ${INFLUXDB_PASSWORD}
      DOCKER_INFLUXDB_INIT_ORG: ${INFLUXDB_ORG}
      DOCKER_INFLUXDB_INIT_BUCKET: ${INFLUXDB_BUCKET}
      DOCKER_INFLUXDB_INIT_ADMIN_TOKEN: ${INFLUXDB_TOKEN}
    networks:
      - internal
    restart: unless-stopped

  grafana:
    image: grafana/grafana-oss:10.2.3
    container_name: grafana
    volumes:
      - grafana-storage:/var/lib/grafana
    environment:
      GF_SECURITY_ADMIN_USER: ${GRAFANA_ADMIN_USER}
      GF_SECURITY_ADMIN_PASSWORD: ${GRAFANA_ADMIN_PASSWORD}
    labels:
      - "traefik.enable=true"
      - "traefik.http.routers.grafana.rule=Host(`grafana.vlnet.me`)"
      - "traefik.http.routers.grafana.entrypoints=websecure"
      - "traefik.http.routers.grafana.tls.certresolver=le"
      - "traefik.http.services.grafana.loadbalancer.server.port=3000"
    depends_on:
      - influxdb
    networks:
      - proxy
      - internal
    restart: unless-stopped

  nodered:
    image: nodered/node-red:3.1
    container_name: nodered
    volumes:
      - ./nodered-data:/data
    labels:
      - "traefik.enable=true"
      - "traefik.http.routers.nodered.rule=Host(`nodered.vlnet.me`)"
      - "traefik.http.routers.nodered.entrypoints=websecure"
      - "traefik.http.routers.nodered.tls.certresolver=le"
      - "traefik.http.services.nodered.loadbalancer.server.port=1880"
    depends_on:
      - mqtt
      - influxdb
    networks:
      - proxy
      - internal
    restart: unless-stopped

  portainer:
    image: portainer/portainer-ce:2.20.3
    container_name: portainer
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
      - portainer-data:/data
    labels:
      - "traefik.enable=true"
      - "traefik.http.routers.portainer.rule=Host(`portainer.vlnet.me`)"
      - "traefik.http.routers.portainer.entrypoints=websecure"
      - "traefik.http.routers.portainer.tls.certresolver=le"
      - "traefik.http.services.portainer.loadbalancer.server.port=9000"
    networks:
      - proxy
    restart: unless-stopped

networks:
  proxy:
    external: true
  internal:
    name: iot-internal
    internal: true

volumes:
  influxdb-data:
  influxdb-backup:
  grafana-storage:
  portainer-data:
YAML

########################################
# IOT: .env (placeholders)
########################################
cat > "$ROOT_DIR/iot/.env" <<'ENV'
# InfluxDB (init only on first run with empty volume)
INFLUXDB_USER=admin
INFLUXDB_PASSWORD=CHANGE_ME_STRONG_PASSWORD
INFLUXDB_ORG=my-org
INFLUXDB_BUCKET=sensor-data
INFLUXDB_TOKEN=CHANGE_ME_LONG_RANDOM_TOKEN

# Grafana admin
GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=CHANGE_ME_STRONG_PASSWORD
ENV

########################################
# IOT: mosquitto.conf
########################################
cat > "$ROOT_DIR/iot/mosquitto/config/mosquitto.conf" <<'CONF'
listener 1883
protocol mqtt

allow_anonymous false
password_file /mosquitto/config/passwords

persistence true
persistence_location /mosquitto/data/

log_dest file /mosquitto/log/mosquitto.log
log_type error
log_type warning
log_type notice
CONF

########################################
# IOT: Node-RED settings.js (placeholder hash)
########################################
cat > "$ROOT_DIR/iot/nodered-data/settings.js" <<'JS'
module.exports = {
  adminAuth: {
    type: "credentials",
    users: [{
      username: "admin",
      // Generate with: docker exec -it nodered node-red-admin hash-pw
      password: "$2b$08$REPLACE_WITH_REAL_BCRYPT_HASH",
      permissions: "*"
    }]
  }
}
JS

########################################
# WEB: docker-compose.yml (WordPress + MariaDB + Redis + simple backup loop)
########################################
cat > "$ROOT_DIR/web/docker-compose.yml" <<'YAML'
services:

  db:
    image: mariadb:10.11
    container_name: wp-db
    environment:
      MARIADB_DATABASE: ${WP_DB_NAME}
      MARIADB_USER: ${WP_DB_USER}
      MARIADB_PASSWORD: ${WP_DB_PASSWORD}
      MARIADB_ROOT_PASSWORD: ${WP_DB_ROOT_PASSWORD}
    volumes:
      - wp-db:/var/lib/mysql
    networks:
      - internal
    restart: unless-stopped

  redis:
    image: redis:7.2-alpine
    container_name: wp-redis
    command: ["redis-server", "--save", "60", "1", "--loglevel", "warning"]
    volumes:
      - wp-redis:/data
    networks:
      - internal
    restart: unless-stopped

  wordpress:
    image: wordpress:6.6.2-php8.2-apache
    container_name: wp
    environment:
      WORDPRESS_DB_HOST: db:3306
      WORDPRESS_DB_NAME: ${WP_DB_NAME}
      WORDPRESS_DB_USER: ${WP_DB_USER}
      WORDPRESS_DB_PASSWORD: ${WP_DB_PASSWORD}
      WORDPRESS_CONFIG_EXTRA: |
        define('WP_HOME','https://www.vlnet.me');
        define('WP_SITEURL','https://www.vlnet.me');
        define('DISALLOW_FILE_EDIT', true);
        define('WP_MEMORY_LIMIT', '256M');
        define('WP_REDIS_HOST', 'redis');
        define('WP_REDIS_PORT', 6379);
    volumes:
      - wp-html:/var/www/html
    depends_on:
      - db
      - redis
    labels:
      - "traefik.enable=true"

      # www.vlnet.me → WordPress
      - "traefik.http.routers.wp.rule=Host(`www.vlnet.me`)"
      - "traefik.http.routers.wp.entrypoints=websecure"
      - "traefik.http.routers.wp.tls.certresolver=le"
      - "traefik.http.services.wp.loadbalancer.server.port=80"

      # vlnet.me → redirect → www.vlnet.me
      - "traefik.http.routers.wp-root.rule=Host(`vlnet.me`)"
      - "traefik.http.routers.wp-root.entrypoints=websecure"
      - "traefik.http.routers.wp-root.tls.certresolver=le"
      - "traefik.http.routers.wp-root.middlewares=redirect-to-www@docker"
      - "traefik.http.middlewares.redirect-to-www.redirectregex.regex=^https://vlnet\\.me/(.*)"
      - "traefik.http.middlewares.redirect-to-www.redirectregex.replacement=https://www.vlnet.me/$$1"
      - "traefik.http.middlewares.redirect-to-www.redirectregex.permanent=true"

    networks:
      - proxy
      - internal
    restart: unless-stopped

  backup:
    image: alpine:3.20
    container_name: wp-backup
    depends_on:
      - db
    volumes:
      - wp-html:/var/www/html:ro
      - ./backups:/backups
    networks:
      - internal
    entrypoint: ["/bin/sh","-c"]
    command: >
      "apk add --no-cache mariadb-client tar gzip &&
       while true; do
         TS=$$(date +%F_%H-%M);
         echo \"[backup] $$TS\";
         mysqldump -h db -u${WP_DB_USER} -p${WP_DB_PASSWORD} ${WP_DB_NAME} | gzip > /backups/db_$$TS.sql.gz;
         tar -czf /backups/wp_html_$$TS.tar.gz -C /var/www/html .;
         find /backups -type f -mtime +14 -delete;
         sleep 86400;
       done"

networks:
  proxy:
    external: true
  internal:
    name: wp-internal
    internal: true

volumes:
  wp-db:
  wp-html:
  wp-redis:
YAML

########################################
# WEB: .env (placeholders)
########################################
cat > "$ROOT_DIR/web/.env" <<'ENV'
WP_DB_NAME=wordpress
WP_DB_USER=wpuser
WP_DB_PASSWORD=CHANGE_ME_STRONG_PASSWORD
WP_DB_ROOT_PASSWORD=CHANGE_ME_STRONG_ROOT_PASSWORD
ENV

########################################
# Helpful README (mini)
########################################
cat > "$ROOT_DIR/README_FIRST_STEPS.txt" <<'TXT'
FIRST STEPS (high level)

1) Create proxy network once:
   docker network create proxy

2) Start Traefik:
   cd vlnet-infra/proxy
   docker compose up -d

3) Start IoT:
   cd ../iot
   # Create mosquitto user file BEFORE starting mosquitto:
   docker run --rm -it -v "$(pwd)/mosquitto/config:/mosquitto/config" eclipse-mosquitto:2.0.18 \
     mosquitto_passwd -c /mosquitto/config/passwords sensor1
   sudo chown -R 1883:1883 mosquitto
   docker compose up -d

4) Protect Node-RED:
   cd ../iot
   docker exec -it nodered node-red-admin hash-pw
   # paste hash into nodered-data/settings.js
   docker compose restart nodered

5) Start WordPress:
   cd ../web
   docker compose up -d

DNS:
- www.vlnet.me, vlnet.me, grafana.vlnet.me, nodered.vlnet.me, portainer.vlnet.me, mqtt.vlnet.me -> your VM IP

NOTE:
- Do NOT run `docker compose down -v` on the proxy stack, or you'll lose Let's Encrypt storage (acme.json).
TXT

echo "Creating/ensuring external docker network 'proxy' exists..."
if docker network inspect proxy >/dev/null 2>&1; then
  echo "Docker network 'proxy' already exists."
else
  docker network create proxy
  echo "Docker network 'proxy' created."
fi

echo
echo "Done. Structure created under: $ROOT_DIR"
echo "Next: edit vlnet-infra/iot/.env and vlnet-infra/web/.env, then follow README_FIRST_STEPS.txt"
