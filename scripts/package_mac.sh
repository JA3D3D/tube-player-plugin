#!/bin/bash
# Gather the built plugins into:
#   dist/Tube Player.pkg            the installer (main download)
#   dist/Tube Player for Mac.zip    the same plugins for installing by hand, plus the standalone app
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -nE 's/^project\(TUBE_PLAYER VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)
ART="build/TubePlayer_artefacts/Release"
OUT="dist/Tube Player for Mac"
PKGW="build/pkg"
rm -rf dist "$PKGW"
mkdir -p "$OUT" "$PKGW/vst3" "$PKGW/au" "$PKGW/resources"

ditto "$ART/VST3/Tube Player.vst3" "$OUT/Tube Player.vst3"
ditto "$ART/AU/Tube Player.component" "$OUT/Tube Player.component"
ditto "$ART/Standalone/Tube Player.app" "$OUT/Extras/Tube Player.app"

# Signing: with an Apple "Developer ID Application" identity in TP_SIGN_APP the code is signed for
# notarization (hardened runtime + secure timestamp); without one it is signed "ad hoc", which
# Apple Silicon Macs need to load it at all.
for b in "$OUT/Tube Player.vst3" "$OUT/Tube Player.component" "$OUT/Extras/Tube Player.app"; do
  if [ -n "${TP_SIGN_APP:-}" ]; then
    codesign --force --deep --options runtime --timestamp --sign "$TP_SIGN_APP" "$b"
  else
    codesign --force --deep --sign - "$b"
  fi
  codesign --verify --deep --strict "$b"
done
echo "Architectures:"
lipo -info "$OUT/Tube Player.vst3/Contents/MacOS/Tube Player"
lipo -info "$OUT/Tube Player.component/Contents/MacOS/Tube Player"

# ---------------------------------------------------------------- the .pkg installer
ditto "$OUT/Tube Player.vst3" "$PKGW/vst3/Tube Player.vst3"
ditto "$OUT/Tube Player.component" "$PKGW/au/Tube Player.component"
for k in vst3 au; do
  pkgbuild --analyze --root "$PKGW/$k" "$PKGW/$k.plist"
  # always install into /Library/Audio/Plug-Ins, even if another copy of the bundle exists somewhere else
  n=$(/usr/libexec/PlistBuddy -c "Print" "$PKGW/$k.plist" | grep -c "BundleIsRelocatable" || true)
  for ((i = 0; i < n; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$PKGW/$k.plist"
  done
done
pkgbuild --root "$PKGW/vst3" --component-plist "$PKGW/vst3.plist" --identifier com.bassbible.tubeplayer.vst3 \
  --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" --scripts packaging/pkg/scripts "$PKGW/vst3.pkg"
pkgbuild --root "$PKGW/au" --component-plist "$PKGW/au.plist" --identifier com.bassbible.tubeplayer.au \
  --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" --scripts packaging/pkg/scripts "$PKGW/au.pkg"
sed "s/__VERSION__/$VERSION/g" packaging/pkg/distribution.xml > "$PKGW/distribution.xml"
for f in packaging/pkg/resources/*.html; do sed "s/__VERSION__/$VERSION/g" "$f" > "$PKGW/resources/$(basename "$f")"; done
# with a "Developer ID Installer" identity in TP_SIGN_INSTALLER the installer itself is signed too
SIGN_PKG=()
[ -n "${TP_SIGN_INSTALLER:-}" ] && SIGN_PKG=(--sign "$TP_SIGN_INSTALLER" --timestamp)
productbuild --distribution "$PKGW/distribution.xml" --resources "$PKGW/resources" --package-path "$PKGW" \
  ${SIGN_PKG[@]+"${SIGN_PKG[@]}"} "dist/Tube Player.pkg"
pkgutil --payload-files "dist/Tube Player.pkg" | grep -E "\.(vst3|component)$" || true

# ---------------------------------------------------------------- the zip (install by hand)
cp packaging/README.txt "$OUT/README.txt"
(cd dist && ditto -c -k --sequesterRsrc --keepParent "Tube Player for Mac" "Tube Player for Mac.zip")
ls -la dist
