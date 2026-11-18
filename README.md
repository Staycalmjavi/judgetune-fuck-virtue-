# JUDGETUNE

Real-time vocal tuning plugin (JUCE 8 / C++17, VST3 + Standalone) for FL Studio.
Matte black / matte pink UI, centred on an animated double Ouroboros: one bold-line serpent, mouth open, swallowing its own tail.

## Build (Windows, for FL Studio)
1. Install Visual Studio 2022 (Desktop development with C++) and CMake 3.22+.
2. From this folder:
   ```
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
   (CMake downloads JUCE 8.0.4 automatically; needs internet once.)
3. The VST3 is copied to `C:\Program Files\Common Files\VST3\JUDGETUNE.vst3`. In FL Studio: Options > Manage plugins > Find installed plugins.
4. Insert it as an effect on a vocal mixer track. For MIDI note targeting, route a MIDI Out
   (FL "Fruity Wrapper" -> MIDI > Input port) to the plugin.

## Offline DSP tests (no JUCE needed)
```
cd Tests && g++ -O2 -std=c++17 -I../Source engine_test.cpp -o engine_test && ./engine_test
```

## Signal flow
input gain -> mono sum -> **TunerEngine** (YIN detect -> scale/MIDI target -> retune dynamics -> PSOLA pitch+formant)
-> **Doubler** (2 detuned/delayed/panned layers) -> **Widener** (M/S width, balance, spread, micro-delay, mono-compat)
-> wet/dry against a latency-aligned dry path -> bypass -> output gain.
Latency is reported to the host (about 24 ms at 48 kHz) so FL's delay compensation lines everything up.

## Retune Speed (-10 .. 100)
* **100 .. 0**: first-order glide to the target note. 100 = instant (hard tune, ~0.4 ms), 0 = 400 ms.
* **0 .. -10**: under-damped spring. The pitch overshoots the note and rings around it; -10 wobbles at ~6.5 Hz
  with very little damping (warbly/unstable effect). Humanize slows both ranges on held notes.

## Other controls
* **Humanize** slows retune the longer a note is held. **Flex-Tune** is a soft dead-zone around the note.
* **Tracking** = detector smoothing + voicing sensitivity. **Correction Amount** = blend of the correction.
* **Naturalness** keeps part of the singer's slow intonation drift. **Note Transition** glides between target notes.
* **Vibrato** 0% flattens vibrato, 100% keeps the original, 200% exaggerates it.
* **Pitch / Fine** shift the corrected result; **Formant** is independent; **Preserve Formants** off makes formants follow pitch.
* Click piano keys to build a **Custom** scale. **Detect Key** analyses the pitch-class histogram of what you've sung.
* **A/B** swaps two full snapshots. **Undo/Redo** cover every parameter change. User presets live in
  `%APPDATA%\JUDGETUNE\Presets` as `.jtpreset` files.

## Honest status
* The DSP core (`TunerEngine.h`, `PitchDetector.h`) is unit-tested offline (see Tests): hard-tune accuracy < 0.1 cent on test
  voices, scale snapping, negative-retune overshoot, +12 st shift, formant independence, vibrato control, 1% CPU.
* The JUCE glue, GUI, Doubler and Widener were written without being compiled or listened to. Expect a few compile
  errors / tweaks on the first build. Send them back and they get fixed.
* Mono processing: the tuner works on the L+R sum (typical for lead vocals); stereo comes from the Doubler/Widener.
* Auto key detection runs on demand (Detect Key button) and updates the readout live.
* Plugin icon: draw `Assets/gavel` into an .ico and add `ICON_BIG` to `juce_add_plugin` if you want it in the host.

## GitHub Actions
`.github/workflows/build.yml` runs the DSP tests, then builds Windows (VST3 + Standalone) and macOS (universal VST3).
Push the folder to a GitHub repo, open the **Actions** tab, and download the build from the run's **Artifacts**.
Copy `JUDGETUNE.vst3` into `C:\Program Files\Common Files\VST3\` and rescan in FL Studio.
