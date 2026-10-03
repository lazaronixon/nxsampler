#include "WavWriter.h"

#include <QTemporaryDir>
#include <QTest>

namespace {

uint32_t u32(const QByteArray& b, int at)
{
    return static_cast<uint8_t>(b[at]) | (static_cast<uint8_t>(b[at + 1]) << 8) |
           (static_cast<uint8_t>(b[at + 2]) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(b[at + 3])) << 24);
}

uint16_t u16(const QByteArray& b, int at)
{
    return static_cast<uint16_t>(static_cast<uint8_t>(b[at]) | (static_cast<uint8_t>(b[at + 1]) << 8));
}

} // namespace

class TestWavWriter : public QObject
{
    Q_OBJECT

private slots:
    void headerFields_data()
    {
        QTest::addColumn<int>("channels");
        QTest::addColumn<int>("bits");
        QTest::addColumn<int>("rate");
        for (int rate : {11025, 22050, 32000, 44100, 48000})
            for (int channels : {1, 2})
                for (int bits : {8, 16})
                    QTest::addRow("%dch-%dbit-%d", channels, bits, rate) << channels << bits << rate;
    }

    void headerFields()
    {
        QFETCH(int, channels);
        QFETCH(int, bits);
        QFETCH(int, rate);

        const int frames = 101; // odd, to exercise the pad byte for 8-bit mono
        const std::vector<uint8_t> pcm(static_cast<size_t>(frames * channels * bits / 8), 0x80);
        const QByteArray wav = WavWriter::build(pcm, {channels, rate, bits}, 47);

        const auto dataSize = static_cast<uint32_t>(pcm.size());
        const uint32_t pad = dataSize % 2;

        QCOMPARE(wav.mid(0, 4), QByteArray("RIFF"));
        QCOMPARE(u32(wav, 4), static_cast<uint32_t>(wav.size() - 8));
        QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));

        QCOMPARE(wav.mid(12, 4), QByteArray("fmt "));
        QCOMPARE(u32(wav, 16), 16u);
        QCOMPARE(u16(wav, 20), uint16_t(1));
        QCOMPARE(int(u16(wav, 22)), channels);
        QCOMPARE(int(u32(wav, 24)), rate);
        QCOMPARE(int(u32(wav, 28)), rate * channels * bits / 8);
        QCOMPARE(int(u16(wav, 32)), channels * bits / 8);
        QCOMPARE(int(u16(wav, 34)), bits);

        // Canonical 44-byte header: data starts right after fmt.
        QCOMPARE(wav.mid(36, 4), QByteArray("data"));
        QCOMPARE(u32(wav, 40), dataSize);

        const int smpl = 44 + static_cast<int>(dataSize + pad);
        QCOMPARE(wav.mid(smpl, 4), QByteArray("smpl"));
        QCOMPARE(u32(wav, smpl + 4), 36u);
        QCOMPARE(u32(wav, smpl + 8 + 12), 47u); // MIDI unity note
        QCOMPARE(wav.size(), smpl + 8 + 36);
    }

    void writesFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("RealStr2P47.wav");
        QString error;
        QVERIFY(WavWriter::write(path, {0, 0, 1, 0}, {1, 44100, 16}, 47, std::nullopt, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.size(), qint64(44 + 4 + 8 + 36));
    }

    void loopIsStoredInSmplChunk()
    {
        const std::vector<uint8_t> pcm(2000, 0);
        const QByteArray wav = WavWriter::build(pcm, {1, 44100, 16}, 60, LoopPoints {200, 950});

        const int smpl = 44 + 2000;
        QCOMPARE(wav.mid(smpl, 4), QByteArray("smpl"));
        QCOMPARE(u32(wav, smpl + 4), 60u);           // 36-byte header + one 24-byte loop
        QCOMPARE(u32(wav, smpl + 8 + 12), 60u);      // MIDI unity note
        QCOMPARE(u32(wav, smpl + 8 + 28), 1u);       // number of loops
        const int loop = smpl + 8 + 36;
        QCOMPARE(u32(wav, loop + 4), 0u);            // forward loop
        QCOMPARE(u32(wav, loop + 8), 200u);          // start frame
        QCOMPARE(u32(wav, loop + 12), 950u);         // end frame (inclusive)
        QCOMPARE(u32(wav, loop + 20), 0u);           // play forever
        QCOMPARE(u32(wav, 4), static_cast<uint32_t>(wav.size() - 8));
        QCOMPARE(wav.size(), loop + 24);
    }
};

QTEST_APPLESS_MAIN(TestWavWriter)
#include "test_wavwriter.moc"
