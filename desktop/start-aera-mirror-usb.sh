#!/bin/sh
set -eu

ADB=${ADB:-adb}
PORT=${AERA_MIRROR_PORT:-8080}

if ! command -v "$ADB" >/dev/null 2>&1; then
    echo "ADB was not found. Install Android platform-tools and try again." >&2
    exit 1
fi

echo "Waiting for AERA Recovery over USB..."
"$ADB" wait-for-recovery
"$ADB" forward --remove "tcp:$PORT" >/dev/null 2>&1 || true
"$ADB" forward "tcp:$PORT" tcp:80 >/dev/null

URL="http://127.0.0.1:$PORT/"
echo "Opening $URL"
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$URL" >/dev/null 2>&1
elif command -v open >/dev/null 2>&1; then
    open "$URL"
else
    echo "Open $URL in a browser."
fi

echo "AERA Mirror is active. Press Enter to disconnect."
read ignored
"$ADB" forward --remove "tcp:$PORT" >/dev/null 2>&1 || true
