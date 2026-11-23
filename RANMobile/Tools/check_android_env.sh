#!/usr/bin/env bash
set -e

echo "=== RAN ONLINE EP7 - FAM HAWAII MOBILE: ANDROID ENV CHECK ==="
MISSING=0

for cmd in cmake ninja java gradle; do
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "[OK] $cmd: $(command -v "$cmd")"
    else
        echo "[MISSING] $cmd"
        MISSING=1
    fi
done

if [ -n "$ANDROID_NDK_HOME" ] && [ -d "$ANDROID_NDK_HOME" ]; then
    echo "[OK] ANDROID_NDK_HOME: $ANDROID_NDK_HOME"
else
    echo "[MISSING] ANDROID_NDK_HOME"
    MISSING=1
fi

if [ -n "$ANDROID_HOME" ] && [ -d "$ANDROID_HOME" ]; then
    echo "[OK] ANDROID_HOME: $ANDROID_HOME"
else
    echo "[MISSING] ANDROID_HOME"
    MISSING=1
fi

if [ "$MISSING" -ne 0 ]; then
    echo "RESULT: ANDROID BUILD = BLOCKED"
    exit 1
fi

echo "RESULT: ANDROID ENV READY"
