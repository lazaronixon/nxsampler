#include "ExtractWorker.h"

#include "LoopFinder.h"
#include "Vst3Host.h"
#include "WavWriter.h"

#include <QDir>

#include <cmath>

using Steinberg::Vst::Event;

namespace {

constexpr float kSilenceThreshold = 3.2e-5f;  // about -90 dBFS
constexpr double kMaxSilenceWaitSec = 3.0;
constexpr int kQuietBlocksNeeded = 4;
constexpr double kWarmUpSec = 0.5;

Event noteEvent(Event::EventTypes type, int key, float velocity)
{
    Event event {};
    event.busIndex = 0;
    event.sampleOffset = 0;
    event.type = static_cast<Steinberg::uint16>(type);
    if (type == Event::kNoteOnEvent)
    {
        event.noteOn.channel = 0;
        event.noteOn.pitch = static_cast<Steinberg::int16>(key);
        event.noteOn.tuning = 0.0f;
        event.noteOn.velocity = velocity;
        event.noteOn.length = 0;
        event.noteOn.noteId = -1;
    }
    else
    {
        event.noteOff.channel = 0;
        event.noteOff.pitch = static_cast<Steinberg::int16>(key);
        event.noteOff.velocity = 0.0f;
        event.noteOff.noteId = -1;
        event.noteOff.tuning = 0.0f;
    }
    return event;
}

} // namespace

QString ExtractSettings::filePath(int key) const
{
    return QDir(folder).filePath(QStringLiteral("%1%2.wav").arg(name).arg(key));
}

ExtractWorker::ExtractWorker(Vst3Host* vstHost, ExtractSettings extractSettings)
: host(vstHost), settings(std::move(extractSettings))
{
}

void ExtractWorker::run()
{
    QStringList written;
    QStringList errors;

    host->startProcessing();

    // Give the instrument a moment to settle (sample preloading, smoothing filters).
    host->process(static_cast<int64_t>(kWarmUpSec * settings.sampleRate), {}, nullptr);

    const auto total = static_cast<int>(settings.keys.size());
    for (int i = 0; i < total && !cancelled; ++i)
    {
        const int key = settings.keys[i];
        emit progress(i, total, key);

        Channels audio = renderKey(key);

        if (settings.channels == 1)
            audio = AudioOps::toMono(audio);

        if (settings.normalize)
            AudioOps::normalize(audio);
        else
            AudioOps::clamp(audio);

        const bool silent = AudioOps::peak(audio) == 0.0f;
        if (silent)
            errors.append(QStringLiteral("Key %1: the instrument produced silence.").arg(key));

        std::optional<LoopPoints> loop;
        if (settings.loop && !silent)
        {
            loop = LoopFinder::find(audio, settings.sampleRate);
            if (!loop)
                errors.append(QStringLiteral("Key %1: no loop point found; saved without a loop.").arg(key));
        }

        WavFormat format;
        format.channels = settings.channels;
        format.sampleRate = settings.sampleRate;
        format.bitsPerSample = settings.bitsPerSample;

        const QString path = settings.filePath(key);
        QString error;
        if (WavWriter::write(path, AudioOps::toPcm(audio, settings.bitsPerSample), format, key, loop, &error))
            written.append(path);
        else
            errors.append(QStringLiteral("Key %1: %2").arg(key).arg(error));
    }

    host->stopProcessing();
    emit progress(total, total, -1);
    emit finished(written, errors, cancelled);
}

void ExtractWorker::waitForSilence()
{
    const auto maxBlocks = static_cast<int>(kMaxSilenceWaitSec * settings.sampleRate / Vst3Host::kBlockSize);
    int quietBlocks = 0;
    Channels block;
    for (int i = 0; i < maxBlocks && quietBlocks < kQuietBlocksNeeded; ++i)
    {
        block.clear();
        host->process(Vst3Host::kBlockSize, {}, &block);
        quietBlocks = AudioOps::peak(block) < kSilenceThreshold ? quietBlocks + 1 : 0;
    }
}

Channels ExtractWorker::renderKey(int key)
{
    // Let the previous note's release die out so it does not bleed into this file.
    waitForSilence();

    const auto frames = static_cast<int64_t>(std::llround(settings.durationSec * settings.sampleRate));
    const int latency = host->latencySamples();
    const float velocity = static_cast<float>(settings.velocity) / 127.0f;

    Channels audio;
    host->process(frames + latency, {noteEvent(Event::kNoteOnEvent, key, velocity)}, &audio);

    // The note is held for the whole capture and released afterwards;
    // nothing after the release is recorded.
    host->process(Vst3Host::kBlockSize, {noteEvent(Event::kNoteOffEvent, key, 0.0f)}, nullptr);

    if (latency > 0)
        for (auto& channel : audio)
            channel.erase(channel.begin(), channel.begin() + latency);

    return audio;
}
