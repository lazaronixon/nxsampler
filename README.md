# NXSampler

Renders a VST3 instrument (Kontakt or any other) one note at a time and saves one WAV
file per key, ready to load into a hardware keyboard's sampler.

## Build

```sh
brew install cmake qt
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
ctest --test-dir build        # unit tests
open build/NXSampler.app
```

The first configure downloads the Steinberg VST3 SDK (MIT license).

## Use

1. Pick an instrument, click **Load**, then click **Open Editor** and choose a sound. The
   instrument plays live through your Mac's audio output, so you can hear it while you
   play it in the editor. Live audio pauses during an extraction.
   Notes are sent on MIDI channel 1. In Kontakt, make sure the instrument you want is on
   channel 1 (or Omni) and is the only one in the rack.
   Instruments come from `/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/VST3`.
   Click **Rescan** after installing new plugins.
2. Select keys on the keyboard. Click toggles a key, dragging paints over several keys,
   and Shift+click selects a range.
3. Pick the dynamics marking (ppp = velocity 16 up to fff = 127), then set the duration, channels, bit depth, sample rate, normalize,
   name and output folder.
4. Click **Extract**. Each key is saved as `<Name><key>.wav` with a three-digit key, for
   example `RealStrF047.wav`, so the files sort by key when sorted by name.

## What is in each file

- The note is held for the whole duration and the file is cut at exactly that length,
  with no release and no fade.
- **Normalize on:** each file is peak-normalized to 0 dBFS on its own, like Logic Pro.
  **Off:** the instrument's own level is kept, and anything above full scale is clipped.
- **Loop on:** the app finds the most seamless sustain loop and stores it as a forward loop
  in the `smpl` chunk.
  - The loop end is near the end of the file; the start comes after the attack.
  - Both points sit on upward zero crossings, and the pair is chosen so the waveform and
    loudness match across the jump.
  - **Crossfade** (off by default; tick it to use it, length default 50%): blends the end
    of the loop into the audio just before the loop start. The length is a percentage of
    the loop length, so 50% of a 200 ms loop is 100 ms. The jump back is then seamless, and any volume difference fades out
    gradually. It is shortened automatically if there isn't enough audio before the loop
    start. With Crossfade off, the audio is not changed.
- Mono is the average of the left and right channels.
- The WAV has a standard 44-byte header. A `smpl` chunk after the audio stores the key as
  the root note, which many samplers use to map the sample automatically.
