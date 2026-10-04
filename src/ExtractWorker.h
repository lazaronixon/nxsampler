#pragma once

#include "AudioOps.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

class Vst3Host;

struct ExtractSettings
{
    QList<int> keys;
    int velocity = 100;       // 1–127
    double durationSec = 1.0;
    int channels = 1;         // 1 = mono, 2 = stereo
    int bitsPerSample = 16;   // 8 or 16
    int sampleRate = 44100;
    bool normalize = true;
    bool loop = true;         // find a sustain loop and store it in the file
    int crossfadePercent = 0; // loop crossfade length as % of the loop length; 0 = off
    bool trim = false;        // cut the silent tail (one-shots); when on, no loop is written
    int trimThresholdDb = -60; // relative to each sample's own peak
    int trimFadeMs = 10;
    QString name;             // file prefix, e.g. RealStr2P
    QString folder;

    QString filePath(int key) const;
};

// Renders each selected key and writes one WAV per key. Lives on its own QThread.
// The host must already be prepared at settings.sampleRate.
class ExtractWorker : public QObject
{
    Q_OBJECT

public:
    ExtractWorker(Vst3Host* host, ExtractSettings settings);

    // Safe to call from any thread; stops after the key being rendered.
    void cancel() { cancelled = true; }

public slots:
    void run();

signals:
    void progress(int done, int total, int key);
    void finished(const QStringList& written, const QStringList& errors, bool wasCancelled);

private:
    void waitForSilence();
    Channels renderKey(int key);

    Vst3Host* host;
    ExtractSettings settings;
    std::atomic_bool cancelled {false};
};
