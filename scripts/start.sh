#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[start] root: $ROOT_DIR"

# Ensure external network exists (shared by stacks)
if ! docker network inspect proxy >/dev/null 2>&1; then
  echo "[start] creating docker network: proxy"
  docker network create proxy >/dev/null
fi

echo "[start] starting proxy stack (Traefik)..."
docker compose -f "$ROOT_DIR/proxy/docker-compose.yml" up -d

echo "[start] starting iot stack..."
if [[ ! -f "$ROOT_DIR/iot/.env" ]]; then
  echo "[start] ERROR: missing iot/.env (copy iot/.env.example -> iot/.env and edit secrets)"
  exit 1
fi
docker compose -f "$ROOT_DIR/iot/docker-compose.yml" --env-file "$ROOT_DIR/iot/.env" up -d

echo "[start] starting web stack..."
if [[ ! -f "$ROOT_DIR/web/.env" ]]; then
  echo "[start] ERROR: missing web/.env (copy web/.env.example -> web/.env and edit secrets)"
  exit 1
fi
docker compose -f "$ROOT_DIR/web/docker-compose.yml" --env-file "$ROOT_DIR/web/.env" up -d

echo "[start] done."
