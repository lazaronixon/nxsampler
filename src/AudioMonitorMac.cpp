// Core Audio implementation of AudioMonitor (macOS).
#include "AudioMonitor.h"

#include "Vst3Host.h"

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>

namespace {

const std::vector<Steinberg::Vst::Event> kNoEvents;

} // namespace

struct AudioMonitor::Backend
{
    AudioComponentInstance unit = nullptr;
    Vst3Host* host = nullptr;

    static OSStatus render(void* refCon, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32,
                           UInt32 frames, AudioBufferList* data)
    {
        auto* self = static_cast<Backend*>(refCon);
        auto* left = static_cast<float*>(data->mBuffers[0].mData);
        auto* right = data->mNumberBuffers > 1 ? static_cast<float*>(data->mBuffers[1].mData) : nullptr;

        if (self->host)
        {
            self->host->render(frames, kNoEvents, left, right);
        }
        else
        {
            for (UInt32 i = 0; i < data->mNumberBuffers; ++i)
                std::fill_n(static_cast<float*>(data->mBuffers[i].mData), frames, 0.0f);
        }
        return noErr;
    }
};

AudioMonitor::AudioMonitor()
: backend(std::make_unique<Backend>())
{
    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponentInstance& unit = backend->unit;
    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component || AudioComponentInstanceNew(component, &unit) != noErr)
    {
        unit = nullptr;
        error = QStringLiteral("No audio output device was found.");
        return;
    }

    // Run the instrument at the device's own rate so no resampling is needed.
    AudioStreamBasicDescription deviceFormat {};
    UInt32 size = sizeof(deviceFormat);
    if (AudioUnitGetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0,
                             &deviceFormat, &size) == noErr &&
        deviceFormat.mSampleRate > 0)
        rate = deviceFormat.mSampleRate;

    AudioStreamBasicDescription format {};
    format.mSampleRate = rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
                          kAudioFormatFlagIsNonInterleaved;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 32;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(float);
    format.mBytesPerPacket = sizeof(float);

    AURenderCallbackStruct callback {&Backend::render, backend.get()};

    if (AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                             &format, sizeof(format)) != noErr ||
        AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &callback, sizeof(callback)) != noErr ||
        AudioUnitInitialize(unit) != noErr)
    {
        AudioComponentInstanceDispose(unit);
        unit = nullptr;
        error = QStringLiteral("The audio output device could not be opened.");
    }
}

AudioMonitor::~AudioMonitor()
{
    stop();
    if (backend->unit)
    {
        AudioUnitUninitialize(backend->unit);
        AudioComponentInstanceDispose(backend->unit);
    }
}

bool AudioMonitor::isAvailable() const
{
    return backend->unit != nullptr;
}

void AudioMonitor::start(Vst3Host* host)
{
    if (!backend->unit || running || !host || !host->isLoaded())
        return;

    backend->host = host;
    host->startProcessing();
    if (AudioOutputUnitStart(backend->unit) == noErr)
    {
        running = true;
        return;
    }
    host->stopProcessing();
    backend->host = nullptr;
}

void AudioMonitor::stop()
{
    if (!running)
        return;
    AudioOutputUnitStop(backend->unit); // synchronous: no render callback runs after this returns
    running = false;
    backend->host->stopProcessing();
    backend->host = nullptr;
}
