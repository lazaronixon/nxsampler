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

| Category | Duration | Auto Loop | Crossfade | Auto Trim | Keys (MIDI) | Every | Samples | ≈ Size (mono) |
|---|---|---|---|---|---|---|---|---|
| Piano | 3 s | on | on | off | E1–G7 (28–103) | 3rd | 26 | 6.9 MB |
| E. Piano | 3 s | on | on | off | E1–G7 (28–103) | 3rd | 26 | 6.9 MB |
| Mallet & Bell | 6 s | off | off | on | C2–C7 (36–96) | 3rd | 21 | ~4 MB |
| Accordion | 3 s | on | on | off | F3–A6 (53–93) | 3rd | 15 | 4.0 MB |
| Organ | 2 s | on | on | off | E1–G7 (28–103) | 4th | 20 | 3.5 MB |
| Guitar | 3 s | on | on | off | E2–E6 (40–88) | 3rd | 17 | 4.5 MB |
| Strings & Vocal | 3 s | on | on | off | E1–C7 (28–96) | 3rd | 24 | 6.4 MB |
| Trumpet & Trbn. | 3 s | on | on | off | E2–C6 (40–84) | 3rd | 16 | 4.2 MB |
| Brass | 2 s | on | on | off | E2–C6 (40–84) | 3rd | 16 | 2.8 MB |
| Sax | 3 s | on | on | off | C#2–E6 (37–88) | 3rd | 18 | 4.8 MB |
| Woodwind | 3 s | on | on | off | D3–C7 (50–96) | 3rd | 17 | 4.5 MB |
| Synth Pad | 4 s | on | on | off | E1–G7 (28–103) | 4th | 20 | 7.1 MB |
| Synth Lead | 2 s | on | on | off | C2–G7 (36–103) | 4th | 18 | 3.2 MB |
| Ethnic | 3 s | on | on | off | E2–A6 (40–93) | 3rd | 19 | 5.0 MB |
| Bass | 2 s | on | on | off | C1–C5 (24–72) | 4th | 13 | 2.3 MB |
| Drum & SFX | 10 s | off | off | on | 27–87 | every | 61 | ~3–10 MB |

Picking a preset selects its keys: every 3rd or 4th key across the range, plus the top key
of the range when the spacing doesn't land on it. Sounds played across the whole keyboard
cover all 76 keys of the Pa3X Le (E1–G7). The others cover the core range of the real
instruments in that factory bank, for example guitar from the low E string to the 24th
fret, and woodwinds from clarinet to flute. Outliers such as bassoon, piccolo, tuba or
accordion bass are left out: for those sounds, select the extra keys by hand. Keys
outside a range still play on the keyboard, stretched from the nearest sample.

Drum & SFX selects every key from 27 to 87: the General MIDI drum map (35–81) plus the
extra GM2/Korg kit notes (high Q, slap, scratch, sticks, metronome, shaker, jingle bell,
castanets, surdo…). In a kit every key is a different drum, so an unsampled key would
play the wrong drum.

The values are chosen to fit the Pa3X's 192 MB of sample memory: one of each sound comes
to about 76 MB. Sizes assume Mono, 44.1 kHz and 16-bit.

Every 3rd key keeps each note within 1 semitone of a real sample, which acoustic
instruments and voices need. Every 4th key (up to 2 semitones) is enough for synthetic
and steady sounds: Organ, Synth Pad, Synth Lead and Bass.

For the one-shot presets (Mallet & Bell, Drum & SFX), Duration is only a limit: Auto Trim
removes the silence after each sound, so short hits stay small. It should be longer than
the longest ring (bells, crash cymbals, room tails), otherwise the sound is cut off.
Drum & SFX uses 10 s, so long effects such as applause or thunder fit too. Extraction
takes longer, since each key is held for the full Duration.

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
