#!/bin/zsh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build/release"
DERIVED_DATA_DIR="${ROOT_DIR}/build/DerivedData"

mkdir -p "${BUILD_DIR}"

xcodebuild \
  -project "${ROOT_DIR}/PeanutButterArchiver.xcodeproj" \
  -scheme "PeanutButterArchiver" \
  -configuration Release \
  -derivedDataPath "${DERIVED_DATA_DIR}" \
  CONFIGURATION_BUILD_DIR="${BUILD_DIR}" \
  PRODUCT_NAME="PBCrypt" \
  CODE_SIGNING_ALLOWED=NO \
  build

printf '\nBuilt app:\n%s\n' "${BUILD_DIR}/PBCrypt.app"
printf 'Portable config path:\n%s\n' "${BUILD_DIR}/PBCrypt.config.json"
