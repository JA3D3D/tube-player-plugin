# AbletonAi

An AI producer inside Ableton Live. Describe what you want ("a rolling 174 bpm reese bassline",
"an 8-bar lo-fi chord loop on a Rhodes", "a dark techno track, intro to drop") and it builds it
in your set: tracks, Live's instruments and effects, MIDI clips, device settings, scenes, tempo.

It has two parts:

| Part | What it is | Where it goes |
|---|---|---|
| **Plug-in** (VST3 + AU) | The chat window. Talks to Claude with your own API key. | `/Library/Audio/Plug-Ins` |
| **Live helper** | A Live control surface script that does the building (Live's own API). | `~/Music/Ableton/User Library/Remote Scripts/AbletonAi` |

The plug-in can't touch your set on its own (no plug-in can), which is why the helper exists.
The two talk over your own computer only (127.0.0.1, with a random token).

## Install (Mac)

```
sudo installer -pkg ~/Downloads/AbletonAi.pkg -target /
```

Then:

1. Restart Live.
2. **Settings › Link, Tempo & MIDI**: choose **AbletonAi** in an empty Control Surface slot
   (Input and Output: None).
3. **Settings › Plug-Ins**: hold Option and click **Rescan**.
4. Drop **AbletonAi** (Plug-Ins › Bass Bible) on any track, open it, and paste your API key from
   [console.anthropic.com](https://console.anthropic.com/settings/keys).

The pink dot at the top of the window turns green when the helper is connected.

## Using it

- **Model**: Fast (Haiku), Balanced (Sonnet, default) or Best (Opus). Use is billed to your
  Anthropic account; a full song with Best is a lot of calls, Balanced is usually the sweet spot.
- It never overwrites your clips: it builds in empty slots and new tracks. Everything it does
  can be undone with Cmd+Z in Live.
- The conversation is saved with your Live set.
- Closing the plug-in window stops a build that's in progress.
- Songs are built as scenes in Session view (one per section). Laying them out in the Arrangement
  works where Live lets a script place clips there; if your version doesn't, launch the scenes in
  order with Arrangement Record on.

## Building

```
git clone --depth 1 --branch 9.0.4 https://github.com/juce-framework/JUCE.git JUCE   # at the repo root
cmake -S abletonai -B build-ai -DCMAKE_BUILD_TYPE=Release -DAI_BUILD_TESTS=ON
cmake --build build-ai --config Release
./build-ai/AbletonAiTests_artefacts/Release/AbletonAiTests
python3 abletonai/live-helper/tests/test_helper.py
bash abletonai/scripts/package_mac.sh      # -> abletonai/dist/AbletonAi.pkg
```

The page tests (`abletonai/tests/test_ui.js`) run with Playwright against a mocked API and plug-in.

GitHub Actions (`.github/workflows/abletonai.yml`) builds a universal (Apple Silicon + Intel)
installer, runs every test, installs it, checks it with `auval` and pluginval, and publishes the
result to the `builds-abletonai` branch. Signing and notarization switch on automatically once the
Apple Developer secrets are added (same ones as Tube Player).

## Notes before selling

- "Ableton" is Ableton AG's trademark. A product name built on it (AbletonAi) is likely to get a
  takedown or rejection from stores; "for Ableton Live" in the description is normally fine.
- JUCE needs a paid licence for closed-source commercial plug-ins over its free tier.
