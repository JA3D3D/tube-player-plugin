TUBE PLAYER 1.0 for Mac
YouTube tutorials in a resizable plugin window, inside Ableton Live

VST3 and Audio Unit. Universal: runs natively on Apple Silicon and Intel Macs.
Needs macOS 11 or newer.

------------------------------------------------------------
INSTALL
------------------------------------------------------------
The easy way is "Tube Player.pkg" (a normal Mac installer). This folder is
for installing by hand:
  Tube Player.vst3       goes in  /Library/Audio/Plug-Ins/VST3
  Tube Player.component  goes in  /Library/Audio/Plug-Ins/Components
Move only those two (not this whole folder). Files moved by hand keep
macOS's "downloaded from the internet" mark, which makes Live skip them;
clear it in Terminal with:
  xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Tube Player.vst3" "/Library/Audio/Plug-Ins/Components/Tube Player.component"

Then in Live: Settings (Cmd + ,) > Plug-Ins. Turn on "Use Audio Units" and
"Use VST3 Plug-in System Folders", hold Option and click Rescan. Tube
Player is under Plug-Ins > Bass Bible in Live's browser.

To uninstall, delete those two files.

------------------------------------------------------------
USING IT
------------------------------------------------------------
Open the plugin window with the wrench button on Tube Player's device in
Live. Drag the window's bottom-right corner to make it any size; the video
always fills it at 16:9.

Paste & Load        Loads the YouTube link on your clipboard in one click.
                    (Or paste into the box and press Return or Load.)
Play, -10, +10      Transport. The position bar jumps anywhere.
Speed               0.25x to 2x, for following fast tutorials.
Set A / Set B       Mark where a part starts and ends; it loops until you
Loop                switch Loop off.
Vol                 The video's volume.
Follow Live         The video plays and stops with Live's transport.
Recent              Your last 25 videos. Each carries on where you stopped.
Restart             Wipes the player's browser data (cookies, cache, saved
                    YouTube settings) and starts it fresh. Use it if videos
                    stop loading or the player acts up. Tick "Also forget my
                    recent videos" to clear the list too.

Saved with your Live set: the video, where you stopped, speed, volume, loop
points and the window size. Reopen the set and it's all there.

In Live: Play, Volume, Speed, Loop and Follow Live are plugin parameters.
Unfold the device (the small triangle on its title bar) to see them, then
automate them, or MIDI-map them in MIDI Map mode like any Live control.

Sound: the video's sound plays through your Mac's normal sound output, not
through Live's mixer, so it isn't recorded and Live's faders don't change it.

Closing the plugin window stops the video; opening it again carries on from
the same spot.

------------------------------------------------------------
TROUBLESHOOTING
------------------------------------------------------------
- "YouTube only": the video's owner only allows it on youtube.com. Press
  Open on YouTube.
- Videos won't load, or the player is stuck: press Restart.
- Live says the plug-in couldn't be loaded, or it doesn't appear: run the
  installer again (it clears macOS's download flag), restart Live, Rescan.
- Cmd+V doesn't paste into the link box: use Paste & Load instead.

Extras/Tube Player.app is the same player as a standalone app (optional).

Recent videos are kept in ~/Library/Application Support/Tube Player/.
Built with JUCE (juce.com).
