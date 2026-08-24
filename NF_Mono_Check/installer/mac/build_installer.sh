#!/bin/bash
# Builds the full "final product" macOS installer for NF Mono Check:
# a welcome/choices/conclusion .pkg (VST3 + AU, user picks which) wrapped in a DMG.
# Requires the VST3 and AU targets to already be built (Release config).
set -euo pipefail

PLUGIN_NAME="NF Mono Check"
VERSION="1.0.0"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INSTALLER_DIR="$ROOT_DIR/installer/mac"
MANUAL_DIR="$ROOT_DIR/manual"
ARTEFACTS_DIR="$ROOT_DIR/build/NFMonoCheck_artefacts/Release"
VST3_PATH="$ARTEFACTS_DIR/VST3/$PLUGIN_NAME.vst3"
AU_PATH="$ARTEFACTS_DIR/AU/$PLUGIN_NAME.component"
MANUAL_EN_PATH="$MANUAL_DIR/NF_Mono_Check_Manual_EN.pdf"
MANUAL_PT_PATH="$MANUAL_DIR/NF_Mono_Check_Manual_PT.pdf"
OUT_DIR="$ROOT_DIR/$PLUGIN_NAME - Download Package"
FINAL_PKG_NAME="NF Mono Check Installer.pkg"
DMG_NAME="$PLUGIN_NAME - Mac Installer.dmg"

for path in "$VST3_PATH" "$AU_PATH" "$MANUAL_EN_PATH" "$MANUAL_PT_PATH"; do
    if [ ! -e "$path" ]; then
        echo "error: not found: $path" >&2
        echo "Build both plugin formats first with: cmake --build \"$ROOT_DIR/build\" --config Release" >&2
        echo "and generate the manual with: python3 \"$MANUAL_DIR/build_manual.py\"" >&2
        exit 1
    fi
done

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

VST3_ROOT="$WORK_DIR/vst3-root/Library/Audio/Plug-Ins/VST3"
AU_ROOT="$WORK_DIR/au-root/Library/Audio/Plug-Ins/Components"
DOCS_ROOT="$WORK_DIR/docs-root/Users/Shared/NF Audio Tools/$PLUGIN_NAME/Manual"
mkdir -p "$VST3_ROOT" "$AU_ROOT" "$DOCS_ROOT"
cp -R "$VST3_PATH" "$VST3_ROOT/"
cp -R "$AU_PATH" "$AU_ROOT/"
cp "$MANUAL_EN_PATH" "$DOCS_ROOT/"
cp "$MANUAL_PT_PATH" "$DOCS_ROOT/"

PKG_DIR="$WORK_DIR/pkgs"
mkdir -p "$PKG_DIR"

pkgbuild \
    --root "$WORK_DIR/vst3-root" \
    --identifier "com.nfaudiotools.monocheck.vst3" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFMonoCheck-VST3.pkg"

pkgbuild \
    --root "$WORK_DIR/au-root" \
    --identifier "com.nfaudiotools.monocheck.au" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFMonoCheck-AU.pkg"

pkgbuild \
    --root "$WORK_DIR/docs-root" \
    --identifier "com.nfaudiotools.monocheck.docs" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFMonoCheck-Docs.pkg"

mkdir -p "$OUT_DIR"
FINAL_PKG_PATH="$WORK_DIR/$FINAL_PKG_NAME"

productbuild \
    --distribution "$INSTALLER_DIR/Distribution.xml" \
    --resources "$INSTALLER_DIR" \
    --package-path "$PKG_DIR" \
    "$FINAL_PKG_PATH"

DMG_STAGING="$WORK_DIR/dmg-staging"
mkdir -p "$DMG_STAGING"
cp "$FINAL_PKG_PATH" "$DMG_STAGING/"

rm -f "$OUT_DIR/$DMG_NAME"
hdiutil create \
    -volname "$PLUGIN_NAME" \
    -srcfolder "$DMG_STAGING" \
    -ov -format UDZO \
    "$OUT_DIR/$DMG_NAME"

echo "Created: $OUT_DIR/$DMG_NAME"
