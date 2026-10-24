#!/bin/bash
# Gather the built plugins into "dist/Tube Player for Mac.zip" with the installer and readme.
set -euo pipefail
cd "$(dirname "$0")/.."

ART="build/TubePlayer_artefacts/Release"
OUT="dist/Tube Player for Mac"
rm -rf dist
mkdir -p "$OUT"

ditto "$ART/VST3/Tube Player.vst3" "$OUT/Tube Player.vst3"
ditto "$ART/AU/Tube Player.component" "$OUT/Tube Player.component"
ditto "$ART/Standalone/Tube Player.app" "$OUT/Extras/Tube Player.app"

# No Apple developer certificate: sign "ad hoc" so Apple Silicon Macs will load the code.
for b in "$OUT/Tube Player.vst3" "$OUT/Tube Player.component" "$OUT/Extras/Tube Player.app"; do
  codesign --force --deep --sign - "$b"
  codesign --verify --deep "$b"
done

echo "Architectures:"
lipo -info "$OUT/Tube Player.vst3/Contents/MacOS/Tube Player"
lipo -info "$OUT/Tube Player.component/Contents/MacOS/Tube Player"

cp "packaging/Install Tube Player.command" "$OUT/"
chmod +x "$OUT/Install Tube Player.command"
cp packaging/README.txt "$OUT/README.txt"

(cd dist && ditto -c -k --sequesterRsrc --keepParent "Tube Player for Mac" "Tube Player for Mac.zip")
ls -la dist
