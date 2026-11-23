#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RANMOBILE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

"$SCRIPT_DIR/check_android_env.sh"

cmake -S "$RANMOBILE_DIR" -B "$RANMOBILE_DIR/build-android-arm64" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24

cmake --build "$RANMOBILE_DIR/build-android-arm64" -j
echo "ARM64 native library built successfully."
