#!/usr/bin/env bash
set -euo pipefail

mkdir -p /run/systemd/journal /var/log/journal /etc/systemd

/usr/lib/systemd/systemd-journald &
JOURNALD_PID=$!

i=0
while [ ! -S /run/systemd/journal/socket ]; do
  i=$((i + 1))
  if [ "$i" -ge 30 ]; then
    echo "error: journald socket did not appear" >&2
    exit 1
  fi
  sleep 0.1
done

echo "journald is ready (pid $JOURNALD_PID)"

exec ctest --test-dir /src/build --output-on-failure -V
