# OpenClaw worker (cloud)

This stack runs **OpenClaw Gateway** in Docker as an always-on **worker** on the server.

Design goals:
- Always-on cron jobs (collector/comprehension/consolidation)
- Persist KB + cron store on a Docker volume
- No WhatsApp / no calendar integrations in cloud
- Local GROBID service for PDF-first extraction

## Start
From repo root:

```bash
./scripts/start.sh
```

This will build the image and start:
- `openclaw-gateway`
- `grobid`

## First-time onboarding
You need to create the OpenClaw config/workspace inside the persisted volume.

Option A (interactive):

```bash
docker compose -f openclaw/docker-compose.yml run --rm openclaw-gateway openclaw onboard
```

Then start the gateway:

```bash
docker compose -f openclaw/docker-compose.yml up -d
```

## Notes
- The OpenClaw state (including workspace) is stored in the `openclaw-state` named volume.
- GROBID is reachable from OpenClaw at: `http://grobid:8070`.
  Update any hard-coded `127.0.0.1:8070` references in job prompts/scripts when moving to cloud.

## Backup
Included in `./scripts/backup-all.sh` (exports `openclaw-state` as a tar.gz).
