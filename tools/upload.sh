#!/usr/bin/env bash
# Build the release firmware and upload it to a board over USB.
# Usage: tools/upload.sh [ENV]   (default: mkr1010_release)
set -euo pipefail
cd "$(dirname "$0")/.."

ENV="${1:-mkr1010_release}"
pio run -e "$ENV" -t upload

echo "Uploaded ($ENV). Monitor with: pio device monitor -e $ENV"
