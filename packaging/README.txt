TUBE PLAYER 1.0 for Mac
YouTube tutorials in a resizable plugin window, inside Ableton Live

VST3 and Audio Unit. Universal: runs natively on Apple Silicon and Intel Macs.
Needs macOS 11 or newer.

------------------------------------------------------------
INSTALL
------------------------------------------------------------
1. Unzip the download (double-click it) if you haven't already.
2. Double-click "Install Tube Player.command".
   If macOS says it can't check the file ("Apple could not verify..."):
   open System Settings > Privacy & Security, scroll down and press
   "Open Anyway" next to Install Tube Player.command, then confirm.
   (Or open Terminal, type  bash  and a space, drag the installer into the
   Terminal window and press Return.)
3. Quit Live and open it again.
4. In Live: Settings/Preferences > Plug-Ins. Turn on "Use Audio Units" and/or
   "Use VST3 Plug-in System Folders". If Tube Player doesn't show up,
   press Rescan.
5. Find it in Live's browser under Plug-Ins > Bass Bible > Tube Player and
   drop it on any track (an empty audio track or the master is a good spot).
   Audio passes through untouched.

The installer puts the plug-ins here:
  ~/Library/Audio/Plug-Ins/VST3/Tube Player.vst3
  ~/Library/Audio/Plug-Ins/Components/Tube Player.component
To uninstall, delete those two.

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
