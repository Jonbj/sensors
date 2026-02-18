# OpenClaw worker image with extra tooling for Spirulina KB processing
# - python3 for local scripts
# - curl for health checks and fetching
# - poppler-utils/pdftotext + ocrmypdf/tesseract for OCR pipeline
# NOTE: image base is Node because OpenClaw is a Node CLI.

FROM node:22-bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

# System deps
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    python3 \
    python3-venv \
    python3-pip \
    poppler-utils \
    ghostscript \
    tesseract-ocr \
    ocrmypdf \
  && rm -rf /var/lib/apt/lists/*

# OpenClaw CLI (includes gateway)
RUN npm install -g openclaw

# Create a non-root user home (node user exists in base image)
USER node
WORKDIR /home/node

# Default state dir can live under the mounted volume
ENV OPENCLAW_STATE_DIR=/home/node/.openclaw

# Default command: run the gateway in foreground (worker)
CMD ["openclaw", "gateway", "run", "--bind", "loopback", "--allow-unconfigured"]
