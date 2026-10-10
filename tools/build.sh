#!/usr/bin/env bash
# Full verification: native unit tests + all firmware builds.
set -euo pipefail
cd "$(dirname "$0")/.."

pio test -e native_test
pio run -e mkr1010_release
pio run -e mkr1010_solaredge
pio run -e mkr1010_debug
pio run -e mkr1010_ethernet
pio run -e mkr1010_ota

echo "All tests and builds passed."
