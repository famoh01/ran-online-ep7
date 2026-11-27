#!/usr/bin/env bash
# Host-only Phase 3J B12 animation container inspector regression + sanitizer tests.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sources=(
  RANMobile/Data/Importer/EP7Animation.cpp
  RANMobile/Data/Importer/EP7RccArchive.cpp
  RANMobile/Data/Importer/EP7Skeleton.cpp
  RANMobile/Data/Importer/EP7XFile.cpp
  RANMobile/Tests/test_ep7_animation_inspector.cpp
)
flags=(-std=c++17 -Wall -Wextra -Werror)

# 1. Compile standalone CLI tool cleanly under -Wall -Wextra -Werror
"${CXX:-g++}" "${flags[@]}" \
  RANMobile/Data/Importer/EP7Animation.cpp \
  RANMobile/Data/Importer/EP7RccArchive.cpp \
  RANMobile/Data/Importer/EP7Skeleton.cpp \
  RANMobile/Data/Importer/EP7XFile.cpp \
  RANMobile/Tools/inspect_ep7_animation_containers.cpp \
  -o "$work/inspect_ep7_animation_containers"

# 2. Run inspector unit tests (normal build)
"${CXX:-g++}" "${flags[@]}" "${sources[@]}" -o "$work/inspector-tests"
"$work/inspector-tests"

# 3. Run inspector unit tests under ASan + UBSan + integrated LSan
"${CXX:-g++}" "${flags[@]}" -g -O1 -fno-omit-frame-pointer -fno-pie -no-pie \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  "${sources[@]}" -o "$work/inspector-sanitized"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$work/inspector-sanitized"
echo 'Inspector sanitizers: ASan + UBSan + integrated LSan passed (detect_leaks=1)'
