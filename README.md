# NXSampler

Renders a VST3 instrument (Kontakt or any other) one note at a time and saves one WAV
file per key, ready to load into a hardware keyboard's sampler.

<img width="781" height="903" alt="image" src="https://github.com/user-attachments/assets/fe9086e2-c3a1-4999-b8d6-6c93c641b8e2" />


## Download

Get the latest version from the
[Releases page](https://github.com/lazaronixon/nxsampler/releases/latest).

- **macOS** (Apple Silicon and Intel, macOS 12 or later): open `NXSampler-macOS.dmg` and
  drag NXSampler to Applications. The app is not signed by Apple, so the first time macOS
  blocks it. Open **System Settings → Privacy & Security** and click **Open Anyway**.
- **Windows** (64-bit): unzip `NXSampler-Windows-x64.zip` and run
  `NXSampler\NXSampler.exe`. If SmartScreen warns about an unrecognized app, click
  **More info → Run anyway**.

## Build

macOS:

```sh
brew install cmake qt
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
ctest --test-dir build        # unit tests
open build/NXSampler.app
```

Windows (Visual Studio 2022 and Qt 6 for MSVC with the Qt Multimedia module):

```sh
cmake -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
ctest --test-dir build -C Release
```

The first configure downloads the Steinberg VST3 SDK (MIT license).

To publish a release, set the version in `CMakeLists.txt` (`project(NXSampler VERSION …)`)
and push a matching tag (`git tag v0.0.1 && git push origin v0.0.1`).
GitHub Actions builds and tests both platforms and attaches the `.dmg` and `.zip` to the
release.

## Use

1. Pick an instrument, click **Select**, then click **Open** and choose a sound. The
   instrument plays live through your computer's audio output, so you can hear it while
   you play it in the editor. Live audio pauses during an extraction.
   Notes are sent on MIDI channel 1. In Kontakt, make sure the instrument you want is on
   channel 1 (or Omni) and is the only one in the rack.
   Instruments come from the standard VST3 folders: `/Library/Audio/Plug-Ins/VST3` and
   `~/Library/Audio/Plug-Ins/VST3` on macOS, `C:\Program Files\Common Files\VST3` on
   Windows. The folders are scanned each time NXSampler starts, so restart it after
   installing new plugins.
2. In **Presets**, pick the sound's category. This selects the keys to sample and fills in
   Duration, Auto Loop, Crossfade and Auto Trim with good values for that kind of sound
   (see the table below).
3. Adjust the keys on the keyboard if needed. Click toggles a key, dragging paints over
   several keys, and Shift+click selects a range. Picking another preset replaces the
   key selection.
4. Pick the dynamics marking (ppp = velocity 16 up to fff = 127), then check the sample
   rate, bit depth, channels (Mono by default) and normalize, and set the name and output
   folder.
5. Click **Extract**. Each key is saved as `<Name><key>.wav` with a three-digit key, for
   example `RealStrF047.wav`, so the files sort by key when sorted by name.

## Presets

The 16 factory sound categories of the Korg Pa3X keyboards:

| Category | Duration | Auto Loop | Crossfade | Auto Trim | Sample every | ≈ Size (mono) |
|---|---|---|---|---|---|---|
| Piano | 3 s | on | on | off | 3rd key | 5.6 MB |
| E. Piano | 3 s | on | on | off | 3rd key | 5.6 MB |
| Mallet & Bell | 4 s | off | off | on | 3rd key | ~2–4 MB |
| Accordion | 3 s | on | on | off | 3rd key | 5.6 MB |
| Organ | 2 s | on | on | off | 4th key | 2.8 MB |
| Guitar | 3 s | on | on | off | 3rd key | 5.6 MB |
| Strings & Vocal | 3 s | on | on | off | 3rd key | 5.6 MB |
| Trumpet & Trbn. | 2 s | on | on | off | 3rd key | 3.7 MB |
| Brass | 2 s | on | on | off | 3rd key | 3.7 MB |
| Sax | 2 s | on | on | off | 3rd key | 3.7 MB |
| Woodwind | 2 s | on | on | off | 3rd key | 3.7 MB |
| Synth Pad | 4 s | on | on | off | 3rd key | 7.4 MB |
| Synth Lead | 2 s | on | on | off | 4th key | 2.8 MB |
| Ethnic | 3 s | on | on | off | 3rd key | 5.6 MB |
| Bass | 2 s | on | on | off | 4th key | 2.8 MB |
| Drum & SFX | 5 s | off | off | on | every key, 35–81 | ~2–5 MB |

The presets select keys across the Pa3X Le's 61 keys, C2–C7 (MIDI 36–96): every 3rd
key is 36, 39, 42 … 96 (21 samples), and every 4th key is 36, 40, 44 … 96 (16 samples).
Drum & SFX selects every key of the General MIDI drum map, 35–81 (47 keys), because in a
kit every key is a different drum.

The values are chosen to fit the Pa3X's 192 MB of sample memory: about 25–30 sounds come
to roughly 110–130 MB. Sizes assume Mono, 44.1 kHz and 16-bit.

**Before sampling, turn off the plugin's modulation effects and reverb** (rotary, phaser,
chorus, tremolo, LFO vibrato, delay, reverb) and use the Pa3X's own effects instead. The
sound then stays steady while held, so the short durations above still loop cleanly.

Short, percussive sounds inside a looped category (muted guitars, pizzicato and spiccato
strings, harp, scat voices, brass falls and hits, synth stabs, sequences and arps) usually
work better with the **Drum & SFX** preset.

Crossfade, where on, uses 30% of the loop. Presets don't change Channels, which starts
as Mono. NXSampler starts with the Piano preset.

## What is in each file

- The note is held for the whole duration and the file is cut at exactly that length,
  with no release and no fade.
- **Normalize on:** each file is peak-normalized to 0 dBFS on its own, like Logic Pro.
  **Off:** the instrument's own level is kept, and anything above full scale is clipped.
- **Auto loop on:** the app finds the most seamless sustain loop and stores it as a forward loop
  in the `smpl` chunk.
  - The loop end is near the end of the file; the start comes after the attack.
  - Both points sit on upward zero crossings, and the pair is chosen so the waveform and
    loudness match across the jump.
  - **Crossfade** (off by default; tick it to use it, length default 30%): blends the end
    of the loop into the audio just before the loop start. The length is a percentage of
    the loop length, so 50% of a 200 ms loop is 100 ms. The jump back is then seamless, and any volume difference fades out
    gradually. It is shortened automatically if there isn't enough audio before the loop
    start. With Crossfade off, the audio is not changed.
- **Auto Trim** (off by default), for one-shots like drums and effects: cuts the silence at
  the end of each file.
  - **Threshold** (default -60 dB) is measured from each sample's own peak: the file is cut
    where the sound falls that far below its loudest point, so the result is the same with
    Normalize on or off.
  - **Fade out** (default 10 ms) is added after the cut point and fades to exactly zero, so
    the end never clicks.
  - If Auto Loop is also on, only Auto Trim is applied and no loop is written.
  - Set a generous **Duration** (for example 3 s); each file is then cut where its sound
    has died away.
- Mono is the average of the left and right channels.
- The WAV has a standard 44-byte header. A `smpl` chunk after the audio stores the key as
  the root note, which many samplers use to map the sample automatically.
