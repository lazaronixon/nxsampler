#pragma once

#include <cstdint>
#include <vector>

// Planar float audio: one vector per channel, all the same length.
using Channels = std::vector<std::vector<float>>;

namespace AudioOps {

// Averages all channels into one: (L + R) / 2 for stereo.
Channels toMono(const Channels& input);

// Highest absolute sample value across all channels.
float peak(const Channels& audio);

// Peak-normalizes to full scale (0 dBFS), like Logic Pro's normalize.
// Silent audio is left untouched. Returns the gain that was applied.
float normalize(Channels& audio);

// Limits every sample to [-1, 1] so the integer conversion cannot wrap around.
void clamp(Channels& audio);

// Cuts the silent tail: everything after the last sample louder than `thresholdDb`
// below the peak, plus `fadeFrames` of tail that is faded out to zero.
// Returns the new length in frames. Silent audio is left unchanged.
int64_t trimEnd(Channels& audio, float thresholdDb, int64_t fadeFrames);

// Interleaves and converts to little-endian PCM as stored in a WAV file:
// 16-bit signed or 8-bit unsigned (silence = 128).
std::vector<uint8_t> toPcm(const Channels& audio, int bitsPerSample);

} // namespace AudioOps
