#!/bin/bash
# Build "dist/AbletonAi.pkg": the plug-ins plus the Live helper, with a postinstall that puts the
# helper in your User Library. Signing: TP_SIGN_APP / TP_SIGN_INSTALLER (Developer ID) or ad hoc.
set -euo pipefail
cd "$(dirname "$0")/.."            # abletonai/
VERSION=$(sed -nE 's/^project\(ABLETON_AI VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)
ART="${1:-../build-ai}/AbletonAi_artefacts/Release"
W="../build-ai/pkg"
rm -rf dist "$W"
mkdir -p dist "$W/vst3" "$W/au" "$W/helper/AbletonAi" "$W/resources"

ditto "$ART/VST3/AbletonAi.vst3" "$W/vst3/AbletonAi.vst3"
ditto "$ART/AU/AbletonAi.component" "$W/au/AbletonAi.component"
for b in "$W/vst3/AbletonAi.vst3" "$W/au/AbletonAi.component"; do
  if [ -n "${TP_SIGN_APP:-}" ]; then codesign --force --deep --options runtime --timestamp --sign "$TP_SIGN_APP" "$b"
  else codesign --force --deep --sign - "$b"; fi
  codesign --verify --deep --strict "$b"
done
lipo -info "$W/vst3/AbletonAi.vst3/Contents/MacOS/AbletonAi"

# the Live helper: Python files only
for f in __init__.py core.py server.py; do cp "live-helper/AbletonAi/$f" "$W/helper/AbletonAi/"; done

for k in vst3 au; do
  pkgbuild --analyze --root "$W/$k" "$W/$k.plist"
  n=$(/usr/libexec/PlistBuddy -c "Print" "$W/$k.plist" | grep -c "BundleIsRelocatable" || true)
  for ((i = 0; i < n; i++)); do /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$W/$k.plist"; done
done
pkgbuild --root "$W/vst3" --component-plist "$W/vst3.plist" --identifier com.bassbible.abletonai.vst3 --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/VST3" --scripts packaging/scripts-plugins "$W/vst3.pkg"
pkgbuild --root "$W/au" --component-plist "$W/au.plist" --identifier com.bassbible.abletonai.au --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/Components" --scripts packaging/scripts-plugins "$W/au.pkg"
pkgbuild --root "$W/helper" --identifier com.bassbible.abletonai.helper --version "$VERSION" \
  --install-location "/Library/Application Support/AbletonAi/Remote Scripts" --scripts packaging/scripts-helper "$W/helper.pkg"

sed "s/__VERSION__/$VERSION/g" packaging/distribution.xml > "$W/distribution.xml"
for f in packaging/resources/*.html; do sed "s/__VERSION__/$VERSION/g" "$f" > "$W/resources/$(basename "$f")"; done
cp brand/icon-256.png "$W/resources/icon.png"
SIGN_PKG=()
[ -n "${TP_SIGN_INSTALLER:-}" ] && SIGN_PKG=(--sign "$TP_SIGN_INSTALLER" --timestamp)
productbuild --distribution "$W/distribution.xml" --resources "$W/resources" --package-path "$W" \
  ${SIGN_PKG[@]+"${SIGN_PKG[@]}"} "dist/AbletonAi.pkg"
pkgutil --payload-files "dist/AbletonAi.pkg" | grep -E "\.(vst3|component|py)$" || true
ls -la dist
