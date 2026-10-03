#include "LoopFinder.h"

#include <QTest>

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

Channels tone(double frequency, int sampleRate, double seconds, double decayPerSecond = 0.0)
{
    const auto frames = static_cast<size_t>(seconds * sampleRate);
    std::vector<float> samples(frames);
    for (size_t i = 0; i < frames; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double attack = std::min(1.0, t / 0.02);
        samples[i] = static_cast<float>(0.8 * attack * std::exp(-decayPerSecond * t) *
                                        std::sin(2.0 * kPi * frequency * t));
    }
    return {samples};
}

// Largest step at the loop seam compared with the largest step inside the sound.
double seamRatio(const std::vector<float>& x, const LoopPoints& loop)
{
    double maxStep = 0.0;
    for (size_t i = 1; i < x.size(); ++i)
        maxStep = std::max(maxStep, double(std::fabs(x[i] - x[i - 1])));
    const double seam = std::fabs(x[static_cast<size_t>(loop.start)] - x[static_cast<size_t>(loop.end)]);
    return seam / maxStep;
}

} // namespace

class TestLoopFinder : public QObject
{
    Q_OBJECT

private slots:
    void loopsSteadyToneOnWholeCycles()
    {
        const int rate = 44100;
        const double frequency = 261.63; // middle C
        const Channels audio = tone(frequency, rate, 1.0);
        const auto loop = LoopFinder::find(audio, rate);
        QVERIFY(loop);

        const auto frames = static_cast<int64_t>(audio[0].size());
        QVERIFY(loop->start >= frames / 5);              // after the attack
        QVERIFY(loop->end >= frames * 85 / 100);          // near the end
        QVERIFY(loop->end - loop->start >= frames / 5);   // long enough to sound natural

        // The loop length should be a whole number of cycles.
        const double cycles = double(loop->end + 1 - loop->start) * frequency / rate;
        QVERIFY2(std::fabs(cycles - std::round(cycles)) < 0.05, qPrintable(QString::number(cycles)));
        QVERIFY(seamRatio(audio[0], *loop) <= 1.0);
    }

    void loopsDecayingToneWithoutLevelJump()
    {
        const int rate = 48000;
        const Channels audio = tone(110.0, rate, 1.0, 0.6);
        const auto loop = LoopFinder::find(audio, rate);
        QVERIFY(loop);
        QVERIFY(seamRatio(audio[0], *loop) <= 1.0);
    }

    void stereoUsesBothChannels()
    {
        Channels audio = tone(440.0, 44100, 1.0);
        audio.push_back(audio[0]);
        QVERIFY(LoopFinder::find(audio, 44100));
    }

    void silenceHasNoLoop()
    {
        const Channels audio = {std::vector<float>(44100, 0.0f)};
        QVERIFY(!LoopFinder::find(audio, 44100));
    }

    void tooShortHasNoLoop()
    {
        const Channels audio = tone(440.0, 44100, 0.03);
        QVERIFY(!LoopFinder::find(audio, 44100));
    }
};

QTEST_APPLESS_MAIN(TestLoopFinder)
#include "test_loopfinder.moc"
