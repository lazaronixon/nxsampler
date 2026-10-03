#pragma once

#include <QString>

#include <memory>

class Vst3Host;

// Plays a loaded instrument live through the computer's default output device, so notes
// played in the plugin's editor can be heard.
// The platform part lives in AudioMonitorMac.cpp (Core Audio) and AudioMonitorQt.cpp
// (Qt Multimedia, used on Windows and Linux).
class AudioMonitor
{
public:
    AudioMonitor();
    ~AudioMonitor();

    AudioMonitor(const AudioMonitor&) = delete;
    AudioMonitor& operator=(const AudioMonitor&) = delete;

    bool isAvailable() const;
    const QString& errorString() const { return error; }

    // Rate the host must be prepared at (Vst3Host::Mode::Realtime) before start().
    double sampleRate() const { return rate; }

    void start(Vst3Host* host);
    // Returns only once the audio thread has stopped calling the host.
    void stop();
    bool isRunning() const { return running; }

private:
    struct Backend;
    std::unique_ptr<Backend> backend;
    double rate = 48000.0;
    bool running = false;
    QString error;
};
