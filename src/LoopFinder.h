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

} // namespace LoopFinder
