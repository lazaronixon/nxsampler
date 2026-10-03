#include "LoopFinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr double kMatchWindowSec = 0.025;  // waveform compared on each side of the jump
constexpr double kSeamWindowSec = 0.001;   // samples right at the jump, where a click would be
constexpr double kSeamWeight = 4.0;
constexpr double kLevelWindowSec = 0.1;    // loudness compared on each side of the jump
constexpr double kEndSearchFraction = 0.1; // loop end lies in the last 10% of the file
constexpr double kMinLoopSec = 0.1;
constexpr double kMinLoopFraction = 0.2;
constexpr double kEarliestStartFraction = 0.2;
constexpr int kEndCandidates = 40;
constexpr float kSilence = 1e-4f;

std::vector<float> mixDown(const Channels& audio)
{
    std::vector<float> mix(audio.front().size(), 0.0f);
    for (const auto& channel : audio)
        for (size_t i = 0; i < mix.size(); ++i)
            mix[i] += channel[i];
    return mix;
}

bool isUpwardCrossing(const std::vector<float>& x, int64_t i)
{
    return x[static_cast<size_t>(i)] <= 0.0f && x[static_cast<size_t>(i + 1)] > 0.0f;
}

// Mean squared difference between the audio around `a` and around `b`, relative to
// their energy, so loud and quiet notes are judged the same way.
double waveformMismatch(const Channels& audio, int64_t a, int64_t b, int64_t window)
{
    double diff = 0.0;
    double energy = 0.0;
    for (const auto& channel : audio)
    {
        for (int64_t k = -window; k < window; ++k)
        {
            const double va = channel[static_cast<size_t>(a + k)];
            const double vb = channel[static_cast<size_t>(b + k)];
            diff += (va - vb) * (va - vb);
            energy += va * va + vb * vb;
        }
    }
    return energy > 0.0 ? diff / energy : std::numeric_limits<double>::max();
}

// Relative loudness difference between the stretches just before `a` and `b`.
double levelMismatch(const std::vector<double>& cumulativeEnergy, int64_t a, int64_t b, int64_t window)
{
    auto rms = [&](int64_t at) {
        const int64_t from = std::max<int64_t>(0, at - window);
        const double sum = cumulativeEnergy[static_cast<size_t>(at)] - cumulativeEnergy[static_cast<size_t>(from)];
        return std::sqrt(sum / static_cast<double>(std::max<int64_t>(1, at - from)));
    };
    const double ra = rms(a);
    const double rb = rms(b);
    const double louder = std::max(ra, rb);
    return louder > 0.0 ? std::fabs(ra - rb) / louder : 1.0;
}

} // namespace

namespace LoopFinder {

std::optional<LoopPoints> find(const Channels& audio, int sampleRate)
{
    if (audio.empty() || sampleRate <= 0)
        return std::nullopt;

    const auto frames = static_cast<int64_t>(audio.front().size());
    const auto window = static_cast<int64_t>(kMatchWindowSec * sampleRate);
    const auto seamWindow = std::max<int64_t>(4, static_cast<int64_t>(kSeamWindowSec * sampleRate));
    const auto levelWindow = static_cast<int64_t>(kLevelWindowSec * sampleRate);
    const auto minLoop = std::max(static_cast<int64_t>(kMinLoopSec * sampleRate),
                                  static_cast<int64_t>(kMinLoopFraction * static_cast<double>(frames)));

    // The jump goes from `end` to `start`, so the audio after `end` must resemble the
    // audio at `start`. Both need a full window of audio on each side.
    const int64_t lastEnd = frames - window - 2;
    const int64_t firstEnd = std::max<int64_t>(window, frames - window - 2 -
                                                           static_cast<int64_t>(kEndSearchFraction * static_cast<double>(frames)));
    if (lastEnd <= firstEnd || AudioOps::peak(audio) < kSilence)
        return std::nullopt;

    const std::vector<float> mix = mixDown(audio);

    // Start after the attack: past the loudest point and past the first part of the note.
    int64_t peakAt = 0;
    for (int64_t i = 0; i < frames; ++i)
        if (std::fabs(mix[static_cast<size_t>(i)]) > std::fabs(mix[static_cast<size_t>(peakAt)]))
            peakAt = i;
    const int64_t firstStart = std::max({window, peakAt,
                                         static_cast<int64_t>(kEarliestStartFraction * static_cast<double>(frames))});

    std::vector<double> cumulativeEnergy(static_cast<size_t>(frames) + 1, 0.0);
    for (int64_t i = 0; i < frames; ++i)
        cumulativeEnergy[static_cast<size_t>(i + 1)] =
            cumulativeEnergy[static_cast<size_t>(i)] + double(mix[static_cast<size_t>(i)]) * mix[static_cast<size_t>(i)];

    std::vector<int64_t> ends;
    for (int64_t i = lastEnd; i >= firstEnd && static_cast<int>(ends.size()) < kEndCandidates; --i)
        if (isUpwardCrossing(mix, i))
            ends.push_back(i);

    std::optional<LoopPoints> best;
    double bestScore = std::numeric_limits<double>::max();

    for (int64_t end : ends)
    {
        // The loop wraps from `end` to `start`: sample end+1 is replaced by sample start.
        const int64_t jumpTarget = end + 1;
        for (int64_t start = firstStart; start + minLoop <= end; ++start)
        {
            if (!isUpwardCrossing(mix, start - 1))
                continue;
            const double score = waveformMismatch(audio, start, jumpTarget, window) +
                                 kSeamWeight * waveformMismatch(audio, start, jumpTarget, seamWindow) +
                                 levelMismatch(cumulativeEnergy, start, jumpTarget, levelWindow);
            if (score < bestScore)
            {
                bestScore = score;
                best = LoopPoints {start, end};
            }
        }
    }
    return best;
}

int64_t crossfade(Channels& audio, const LoopPoints& loop, int64_t frames)
{
    const int64_t loopLength = loop.end - loop.start + 1;
    const int64_t length = std::min({frames, loop.start, loopLength});
    if (length <= 0)
        return 0;

    // Linear (constant-gain) fade: the two sides are similar by construction, and the
    // blend never exceeds the louder of them, so normalized audio stays within full scale.
    const int64_t fadeStart = loop.end - length + 1;
    const int64_t sourceStart = loop.start - length;
    for (auto& channel : audio)
    {
        for (int64_t i = 0; i < length; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(length);
            float& target = channel[static_cast<size_t>(fadeStart + i)];
            const float source = channel[static_cast<size_t>(sourceStart + i)];
            target = target * (1.0f - t) + source * t;
        }
    }
    return length;
}

} // namespace LoopFinder
