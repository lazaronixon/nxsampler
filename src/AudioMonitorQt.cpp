// Qt Multimedia implementation of AudioMonitor (Windows, Linux).
#include "AudioMonitor.h"

#include "Vst3Host.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

const std::vector<Steinberg::Vst::Event> kNoEvents;
constexpr qint64 kChunkFrames = 1024;

// Read-only device the audio sink pulls interleaved stereo float samples from.
class RenderDevice : public QIODevice
{
public:
    RenderDevice() : left(kChunkFrames), right(kChunkFrames) { open(QIODevice::ReadOnly); }

    void setHost(Vst3Host* newHost)
    {
        std::lock_guard<std::mutex> lock(mutex);
        host = newHost;
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return kChunkFrames * 2 * qint64(sizeof(float)); }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const qint64 frames = maxSize / qint64(2 * sizeof(float));
        auto* out = reinterpret_cast<float*>(data);

        std::lock_guard<std::mutex> lock(mutex);
        for (qint64 done = 0; done < frames;)
        {
            const qint64 chunk = std::min(kChunkFrames, frames - done);
            if (host)
                host->render(chunk, kNoEvents, left.data(), right.data());
            else
            {
                std::fill_n(left.begin(), chunk, 0.0f);
                std::fill_n(right.begin(), chunk, 0.0f);
            }
            for (qint64 i = 0; i < chunk; ++i)
            {
                out[(done + i) * 2] = left[size_t(i)];
                out[(done + i) * 2 + 1] = right[size_t(i)];
            }
            done += chunk;
        }
        return frames * qint64(2 * sizeof(float));
    }

    qint64 writeData(const char*, qint64) override { return -1; }

private:
    std::mutex mutex; // the host is never rendered after setHost(nullptr) returns
    Vst3Host* host = nullptr;
    std::vector<float> left;
    std::vector<float> right;
};

} // namespace

struct AudioMonitor::Backend
{
    QAudioDevice device;
    QAudioFormat format;
    std::unique_ptr<QAudioSink> sink;
    RenderDevice source;
    Vst3Host* host = nullptr;
};

AudioMonitor::AudioMonitor()
: backend(std::make_unique<Backend>())
{
    backend->device = QMediaDevices::defaultAudioOutput();
    if (backend->device.isNull())
    {
        error = QStringLiteral("No audio output device was found.");
        return;
    }

    // Run the instrument at the device's own rate so no resampling is needed.
    const int deviceRate = backend->device.preferredFormat().sampleRate();
    rate = deviceRate > 0 ? deviceRate : 48000;

    backend->format.setSampleRate(static_cast<int>(rate));
    backend->format.setChannelCount(2);
    backend->format.setSampleFormat(QAudioFormat::Float);
    if (!backend->device.isFormatSupported(backend->format))
    {
        backend->device = QAudioDevice();
        error = QStringLiteral("The audio output device does not support 32-bit float stereo.");
    }
}

AudioMonitor::~AudioMonitor()
{
    stop();
}

bool AudioMonitor::isAvailable() const
{
    return !backend->device.isNull();
}

void AudioMonitor::start(Vst3Host* host)
{
    if (!isAvailable() || running || !host || !host->isLoaded())
        return;

    host->startProcessing();
    backend->source.setHost(host);
    backend->sink = std::make_unique<QAudioSink>(backend->device, backend->format);
    backend->sink->setBufferSize(static_cast<qsizetype>(rate / 50) * 2 * qsizetype(sizeof(float))); // ~20 ms
    backend->sink->start(&backend->source);
    if (backend->sink->error() == QAudio::NoError)
    {
        running = true;
        backend->host = host;
        return;
    }
    backend->sink.reset();
    backend->source.setHost(nullptr);
    host->stopProcessing();
}

void AudioMonitor::stop()
{
    if (!running)
        return;
    backend->sink->stop();
    backend->sink.reset();
    backend->source.setHost(nullptr); // waits for any render in progress
    running = false;
    backend->host->stopProcessing();
    backend->host = nullptr;
}
