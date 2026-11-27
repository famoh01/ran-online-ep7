#!/usr/bin/env bash
# Host-only verifier regression tests. No client data required; no parser changes.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sources=(
  RANMobile/Data/Importer/EP7Animation.cpp
  RANMobile/Data/Importer/EP7CharacterFile.cpp
  RANMobile/Data/Importer/EP7RccArchive.cpp
  RANMobile/Data/Importer/EP7Skeleton.cpp
  RANMobile/Data/Importer/EP7SkinPiece.cpp
  RANMobile/Data/Importer/EP7SkinnedMesh.cpp
  RANMobile/Data/Importer/EP7XFile.cpp
  RANMobile/Tests/test_ep7_character_verifier.cpp
)
flags=(-std=c++17 -Wall -Wextra -Werror)
"${CXX:-g++}" "${flags[@]}" "${sources[@]}" -o "$work/verifier-tests"
"$work/verifier-tests"
# Non-PIE avoids ASan shadow address collisions on restricted Linux hosts.
"${CXX:-g++}" "${flags[@]}" -g -O1 -fno-omit-frame-pointer -fno-pie -no-pie \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  "${sources[@]}" -o "$work/verifier-sanitized"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$work/verifier-sanitized"
echo 'Sanitizers: ASan + UBSan + integrated LSan passed (detect_leaks=1)'
