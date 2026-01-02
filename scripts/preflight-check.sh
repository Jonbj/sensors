#!/usr/bin/env bash
set -euo pipefail

DOMAINS=(
  "www.vlnet.me"
  "vlnet.me"
  "grafana.vlnet.me"
  "nodered.vlnet.me"
  "portainer.vlnet.me"
  "mqtt.vlnet.me"
)

PORTS=(80 443 8883)

echo "== preflight checks =="

echo "[1] docker + compose"
docker version >/dev/null
docker compose version >/dev/null
echo "  ok"

echo "[2] DNS resolution (server-side)"
for d in "${DOMAINS[@]}"; do
  if getent ahosts "$d" >/dev/null 2>&1; then
    ip="$(getent ahosts "$d" | awk '{print $1}' | head -n1)"
    echo "  ok: $d -> $ip"
  else
    echo "  ERROR: DNS not resolving for $d"
    exit 1
  fi
done

echo "[3] required ports free on host (best effort)"
for p in "${PORTS[@]}"; do
  if ss -ltn "( sport = :$p )" | grep -q ":$p"; then
    echo "  info: port $p is already LISTENing (may be OK if it's Traefik already running)"
  else
    echo "  ok: port $p is free"
  fi
done

echo "[4] required files"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for f in \
  "$ROOT_DIR/proxy/docker-compose.yml" \
  "$ROOT_DIR/iot/docker-compose.yml" \
  "$ROOT_DIR/web/docker-compose.yml" \
  "$ROOT_DIR/iot/mosquitto/config/mosquitto.conf" \
  "$ROOT_DIR/iot/nodered-data/settings.js"
do
  [[ -f "$f" ]] || { echo "  ERROR missing: $f"; exit 1; }
done
echo "  ok"

echo "[5] mosquitto passwords file"
if [[ -f "$ROOT_DIR/iot/mosquitto/config/passwords" ]]; then
  echo "  ok: passwords file exists"
else
  echo "  WARNING: missing iot/mosquitto/config/passwords"
  echo "  Mosquitto will NOT start with allow_anonymous=false."
  echo "  Create it with:"
  echo "    cd iot"
  echo "    docker run --rm -it -v \"\$(pwd)/mosquitto/config:/mosquitto/config\" eclipse-mosquitto:2.0.18 mosquitto_passwd -c /mosquitto/config/passwords sensor1"
fi

echo "[6] Let's Encrypt note"
echo "  IMPORTANT: never run 'docker compose down -v' on proxy stack (acme.json will be lost)."

echo "preflight ok."
