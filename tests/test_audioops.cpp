#include "AudioOps.h"

#include <QTest>

class TestAudioOps : public QObject
{
    Q_OBJECT

private slots:
    void monoAveragesChannels()
    {
        const Channels stereo = {{1.0f, 0.5f, -1.0f}, {0.0f, 0.5f, 1.0f}};
        const Channels mono = AudioOps::toMono(stereo);
        QCOMPARE(mono.size(), size_t(1));
        QCOMPARE(mono[0][0], 0.5f);
        QCOMPARE(mono[0][1], 0.5f);
        QCOMPARE(mono[0][2], 0.0f);
    }

    void normalizeReachesFullScale()
    {
        Channels audio = {{0.25f, -0.5f}, {0.1f, 0.2f}};
        const float gain = AudioOps::normalize(audio);
        QCOMPARE(gain, 2.0f);
        QCOMPARE(AudioOps::peak(audio), 1.0f);
        QCOMPARE(audio[0][1], -1.0f);
        QCOMPARE(audio[1][1], 0.4f);
    }

    void normalizeLeavesSilenceAlone()
    {
        Channels audio = {{0.0f, 0.0f}};
        QCOMPARE(AudioOps::normalize(audio), 1.0f);
        QCOMPARE(AudioOps::peak(audio), 0.0f);
    }

    void clampLimitsOverloads()
    {
        Channels audio = {{1.5f, -2.0f, 0.3f}};
        AudioOps::clamp(audio);
        QCOMPARE(audio[0][0], 1.0f);
        QCOMPARE(audio[0][1], -1.0f);
        QCOMPARE(audio[0][2], 0.3f);
    }

    void trimCutsQuietTailAfterFade()
    {
        // Loud for 1000 frames, then a tail at -60 dB below the peak.
        Channels audio = {std::vector<float>(5000, 0.001f)};
        std::fill_n(audio[0].begin(), 1000, 1.0f);
        const int64_t length = AudioOps::trimEnd(audio, -50.0f, 100);
        QCOMPARE(length, int64_t(1100));
        QCOMPARE(audio[0].size(), size_t(1100));
        QCOMPARE(audio[0][999], 1.0f);                         // the sound itself is untouched
        QCOMPARE(audio[0][1099], 0.0f);                        // fades to exactly zero
        QVERIFY(audio[0][1050] < audio[0][1000]);              // and ramps down on the way
    }

    void trimThresholdIsRelativeToPeak()
    {
        Channels loud = {std::vector<float>(5000, 0.001f)};
        std::fill_n(loud[0].begin(), 1000, 1.0f);
        Channels quiet = loud;
        for (float& sample : quiet[0])
            sample *= 0.5f;
        QCOMPARE(AudioOps::trimEnd(loud, -50.0f, 10), AudioOps::trimEnd(quiet, -50.0f, 10));
    }

    void trimKeepsSoundThatLastsToTheEnd()
    {
        Channels audio = {std::vector<float>(2000, 0.8f)};
        QCOMPARE(AudioOps::trimEnd(audio, -50.0f, 100), int64_t(2000));
        QCOMPARE(audio[0].back(), 0.0f); // still faded so the end is clean
    }

    void trimLeavesSilenceAlone()
    {
        Channels audio = {std::vector<float>(2000, 0.0f)};
        QCOMPARE(AudioOps::trimEnd(audio, -50.0f, 100), int64_t(2000));
    }

    void trimUsesTheLongerChannel()
    {
        Channels audio = {std::vector<float>(5000, 0.0f), std::vector<float>(5000, 0.0f)};
        std::fill_n(audio[0].begin(), 500, 1.0f);
        std::fill_n(audio[1].begin(), 3000, 1.0f);
        QCOMPARE(AudioOps::trimEnd(audio, -50.0f, 0), int64_t(3000));
        QCOMPARE(audio[0].size(), size_t(3000));
    }

    void pcm16IsSignedLittleEndianInterleaved()
    {
        const Channels audio = {{1.0f, 0.0f}, {-1.0f, 0.5f}};
        const auto pcm = AudioOps::toPcm(audio, 16);
        QCOMPARE(pcm.size(), size_t(8));
        auto sampleAt = [&](size_t i) { return static_cast<int16_t>(pcm[i * 2] | (pcm[i * 2 + 1] << 8)); };
        QCOMPARE(sampleAt(0), int16_t(32767));  // L frame 0
        QCOMPARE(sampleAt(1), int16_t(-32767)); // R frame 0
        QCOMPARE(sampleAt(2), int16_t(0));      // L frame 1
        QCOMPARE(sampleAt(3), int16_t(16384));  // R frame 1
    }

    void pcm8IsUnsignedWithOffset()
    {
        const Channels audio = {{0.0f, 1.0f, -1.0f}};
        const auto pcm = AudioOps::toPcm(audio, 8);
        QCOMPARE(pcm.size(), size_t(3));
        QCOMPARE(int(pcm[0]), 128);
        QCOMPARE(int(pcm[1]), 255);
        QCOMPARE(int(pcm[2]), 1);
    }
};

QTEST_APPLESS_MAIN(TestAudioOps)
#include "test_audioops.moc"
