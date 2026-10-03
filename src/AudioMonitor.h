#pragma once

#include <QString>

#include <AudioToolbox/AudioToolbox.h>

class Vst3Host;

// Plays a loaded instrument live through the Mac's default output device, so notes
// played in the plugin's editor can be heard.
class AudioMonitor
{
public:
    AudioMonitor();
    ~AudioMonitor();

    AudioMonitor(const AudioMonitor&) = delete;
    AudioMonitor& operator=(const AudioMonitor&) = delete;

    bool isAvailable() const { return unit != nullptr; }
    const QString& errorString() const { return error; }

    // Rate the host must be prepared at (Vst3Host::Mode::Realtime) before start().
    double sampleRate() const { return rate; }

    void start(Vst3Host* host);
    // Returns only once the audio thread has stopped calling the host.
    void stop();
    bool isRunning() const { return running; }

private:
    static OSStatus renderCallback(void* refCon, AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* timeStamp, UInt32 bus, UInt32 frames,
                                   AudioBufferList* data);

    AudioComponentInstance unit = nullptr;
    Vst3Host* host = nullptr;
    double rate = 48000.0;
    bool running = false;
    QString error;
};
