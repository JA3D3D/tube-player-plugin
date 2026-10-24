#!/bin/bash
# Installs Tube Player (VST3 + Audio Unit) into your own plug-in folders, so Live can find it.
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

echo "Installing Tube Player..."
mkdir -p "$VST3_DIR" "$AU_DIR" || exit 1

if [ ! -d "Tube Player.vst3" ] || [ ! -d "Tube Player.component" ]; then
  echo "Can't find the plug-ins next to this installer. Unzip the whole folder first, then run it again."
  read -r -p "Press Return to close." _
  exit 1
fi

rm -rf "$VST3_DIR/Tube Player.vst3" "$AU_DIR/Tube Player.component"
ditto "Tube Player.vst3" "$VST3_DIR/Tube Player.vst3" || exit 1
ditto "Tube Player.component" "$AU_DIR/Tube Player.component" || exit 1

# downloaded files are marked "from the internet"; clear that so Live will load them
xattr -dr com.apple.quarantine "$VST3_DIR/Tube Player.vst3" "$AU_DIR/Tube Player.component" 2>/dev/null
codesign --force --deep --sign - "$VST3_DIR/Tube Player.vst3" >/dev/null 2>&1
codesign --force --deep --sign - "$AU_DIR/Tube Player.component" >/dev/null 2>&1
# make macOS notice the new Audio Unit
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo
echo "Done. Tube Player is installed:"
echo "  $VST3_DIR/Tube Player.vst3"
echo "  $AU_DIR/Tube Player.component"
echo
echo "Next: quit Live and open it again. In Live's browser, look under"
echo "Plug-Ins > Bass Bible > Tube Player (it's in both VST3 and Audio Units)."
echo
read -r -p "Press Return to close this window." _
