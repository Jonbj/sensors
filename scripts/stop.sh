#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[stop] stopping openclaw..."
docker compose -f "$ROOT_DIR/openclaw/docker-compose.yml" down || true

echo "[stop] stopping web..."
docker compose -f "$ROOT_DIR/web/docker-compose.yml" --env-file "$ROOT_DIR/web/.env" down || true

echo "[stop] stopping iot..."
docker compose -f "$ROOT_DIR/iot/docker-compose.yml" --env-file "$ROOT_DIR/iot/.env" down || true

echo "[stop] stopping proxy..."
docker compose -f "$ROOT_DIR/proxy/docker-compose.yml" down || true

echo "[stop] done. (volumes preserved)"
