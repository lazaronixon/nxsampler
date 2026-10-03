#include "AudioOps.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace AudioOps {

Channels toMono(const Channels& input)
{
    if (input.empty())
        return {};

    const size_t frames = input.front().size();
    std::vector<float> mono(frames, 0.0f);
    for (const auto& channel : input)
        for (size_t i = 0; i < frames; ++i)
            mono[i] += channel[i];

    const float scale = 1.0f / static_cast<float>(input.size());
    for (float& sample : mono)
        sample *= scale;

    return {std::move(mono)};
}

float peak(const Channels& audio)
{
    float result = 0.0f;
    for (const auto& channel : audio)
        for (float sample : channel)
            result = std::max(result, std::fabs(sample));
    return result;
}

float normalize(Channels& audio)
{
    const float maxValue = peak(audio);
    if (maxValue <= 0.0f)
        return 1.0f;

    const float gain = 1.0f / maxValue;
    for (auto& channel : audio)
        for (float& sample : channel)
            sample *= gain;
    return gain;
}

void clamp(Channels& audio)
{
    for (auto& channel : audio)
        for (float& sample : channel)
            sample = std::clamp(sample, -1.0f, 1.0f);
}

std::vector<uint8_t> toPcm(const Channels& audio, int bitsPerSample)
{
    if (bitsPerSample != 8 && bitsPerSample != 16)
        throw std::invalid_argument("bitsPerSample must be 8 or 16");
    if (audio.empty())
        return {};

    const size_t channelCount = audio.size();
    const size_t frames = audio.front().size();
    const size_t bytesPerSample = static_cast<size_t>(bitsPerSample / 8);

    std::vector<uint8_t> pcm;
    pcm.reserve(frames * channelCount * bytesPerSample);

    for (size_t frame = 0; frame < frames; ++frame)
    {
        for (size_t ch = 0; ch < channelCount; ++ch)
        {
            const float sample = std::clamp(audio[ch][frame], -1.0f, 1.0f);
            if (bitsPerSample == 16)
            {
                const auto value = static_cast<int16_t>(std::lrint(sample * 32767.0f));
                pcm.push_back(static_cast<uint8_t>(value & 0xff));
                pcm.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
            }
            else
            {
                const long value = std::lrint(sample * 127.0f) + 128;
                pcm.push_back(static_cast<uint8_t>(std::clamp(value, 0L, 255L)));
            }
        }
    }
    return pcm;
}

} // namespace AudioOps
