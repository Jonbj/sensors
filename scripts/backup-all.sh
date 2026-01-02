#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TS="$(date +%F_%H-%M-%S)"

OUT_IOT="$ROOT_DIR/backups/iot/$TS"
OUT_WEB="$ROOT_DIR/backups/web/$TS"

mkdir -p "$OUT_IOT" "$OUT_WEB"

echo "== backup started: $TS =="

echo "[1/5] InfluxDB backup (portable tar.gz)"
docker exec influxdb influx backup /backup/"$TS" >/dev/null
docker exec influxdb sh -c "tar -czf /backup/influx_${TS}.tar.gz -C /backup ${TS}"
docker cp influxdb:/backup/influx_${TS}.tar.gz "$OUT_IOT/influx_${TS}.tar.gz"
docker exec influxdb rm -rf /backup/"$TS" /backup/influx_${TS}.tar.gz

echo "[2/5] Node-RED data"
tar -czf "$OUT_IOT/nodered_${TS}.tar.gz" -C "$ROOT_DIR/iot" nodered-data

echo "[3/5] Mosquitto (config+data+log)"
tar -czf "$OUT_IOT/mosquitto_${TS}.tar.gz" -C "$ROOT_DIR/iot" mosquitto

echo "[4/5] Grafana volume export"
docker run --rm -v grafana-storage:/var/lib/grafana -v "$OUT_IOT":/out alpine:3.20 sh -c "cd /var/lib/grafana && tar -czf /out/grafana_${TS}.tar.gz ."

echo "[5/5] WordPress DB + files"
if [[ ! -f "$ROOT_DIR/web/.env" ]]; then
  echo "ERROR: missing web/.env (copy web/.env.example -> web/.env)"
  exit 1
fi
set -a
source "$ROOT_DIR/web/.env"
set +a

docker exec wp-db sh -c "mysqldump -u${WP_DB_USER} -p${WP_DB_PASSWORD} ${WP_DB_NAME} | gzip -c" > "$OUT_WEB/wp_db_${TS}.sql.gz"
docker run --rm -v wp-html:/var/www/html:ro -v "$OUT_WEB":/out alpine:3.20 sh -c "cd /var/www/html && tar -czf /out/wp_files_${TS}.tar.gz ."

echo "== backup completed =="
echo "iot: $OUT_IOT"
echo "web: $OUT_WEB"
