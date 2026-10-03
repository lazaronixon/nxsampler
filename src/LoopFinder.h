#pragma once

#include "AudioOps.h"

#include <cstdint>
#include <optional>

struct LoopPoints
{
    int64_t start = 0; // first frame of the loop
    int64_t end = 0;   // last frame of the loop (inclusive, as in the WAV `smpl` chunk)
};

namespace LoopFinder {

// Finds the most seamless sustain loop in a rendered note.
// The end sits near the end of the file and the start after the attack, both on upward
// zero crossings. Among the candidates it picks the pair whose waveforms around the
// jump match best and whose loudness is closest, so the loop neither clicks nor pulses.
// Returns nothing for silence or audio too short to loop.
std::optional<LoopPoints> find(const Channels& audio, int sampleRate);

// Blends the last `frames` frames of the loop into the audio just before the loop start,
// so that when playback jumps from the end back to the start it carries on exactly as
// the audio did originally. The length is shortened when there is not enough audio
// before the start or inside the loop. Returns the number of frames crossfaded.
int64_t crossfade(Channels& audio, const LoopPoints& loop, int64_t frames);

} // namespace LoopFinder
