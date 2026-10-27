# Vocal Clarity — De-esser + Vocal Mixer (VST3) for FL Studio

One plugin that cleans up and balances R&B vocals.

Signal chain (in order):
1. **Low Cut** (20–250 Hz) — removes rumble and plosive thumps
2. **Mud Cut** (300 Hz, 0 to -8 dB) — clears boxiness
3. **Warmth** (low shelf 180 Hz, ±6 dB) — body and chest
4. **Smooth Comp** (0–100 %) — one-knob, slow-attack leveler (soft knee, 1.5:1 up to 4:1)
5. **Presence** (3.2 kHz bell, ±6 dB) — forward, clear lead vocal
6. **Air** (11 kHz high shelf, 0 to +8 dB) — silky top end
7. **De-esser** (last, so it catches sibilance that Presence/Air bring out)
   - Frequency 3–12 kHz, Threshold, Range (max reduction)
   - Split-band (only cuts the "s" band, most natural) or Wide-band
   - **Listen** plays the sidechain band so you can find the exact "s" frequency
8. **Output** and **Mix** (dry/wet)

Presets: Smooth R&B, Airy Falsetto, Warm Intimate, Bright Pop R&B, Harsh Vocal Rescue.
Double-click any knob to reset it. Stereo or mono, zero latency.

## Get the plugin file (pick one)

### A) Free cloud build with GitHub (no tools to install)
1. Create a GitHub repo and upload this whole folder (keep the `.github` folder).
2. Open the repo's **Actions** tab, run **Build Vocal Clarity** (it also runs on every push).
3. When it finishes, download the artifact **VocalClarity-Windows-VST3** (or **-macOS-**).
4. Unzip it. You get `Vocal Clarity.vst3`.

### B) Build locally
Windows: install Visual Studio 2022 (Desktop C++ workload) and CMake, then in this folder:

    cmake -B build
    cmake --build build --config Release --target VocalClarity_VST3

Result: `build/VocalClarity_artefacts/Release/VST3/Vocal Clarity.vst3`
macOS: install Xcode command line tools and CMake, run the same two commands.
(The first configure downloads JUCE from GitHub automatically.)

## Install in FL Studio
1. Copy `Vocal Clarity.vst3` to
   - Windows: `C:\Program Files\Common Files\VST3\`
   - macOS: `/Library/Audio/Plug-Ins/VST3/`
2. FL Studio: **Options → Manage plugins → Find installed plugins** (Verify plugins on).
3. Put it on a vocal Mixer track: click an effect slot → **Vocal Clarity**
   (or find it in the Plugin database under Installed → Effects).

## Quick start for a vocal
1. Load **Smooth R&B**.
2. Hit **Listen** on the de-esser and sweep **Frequency** until the "s" sounds are loudest, then turn Listen off.
3. Lower **Threshold** until the gain-reduction meter moves only on "s" and "sh" (about 3–6 dB).
4. Adjust Presence and Air to taste, and use Output to match levels.
5. Put it before reverb/delay sends so effects don't get extra sibilance.

## Project layout
- `Source/DSP.h` — EQ, compressor and de-esser (plain C++, no JUCE)
- `Source/PluginProcessor.*` — parameters, presets, audio processing
- `Source/PluginEditor.*` — the interface
- `Tests/dsp_test.cpp` — standalone test of the DSP
- `.github/workflows/build.yml` — cloud build for Windows and macOS
