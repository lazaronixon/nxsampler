#include "WavWriter.h"

#include <QSaveFile>

namespace {

void putU16(QByteArray& out, uint16_t value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
}

void putU32(QByteArray& out, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.append(static_cast<char>((value >> shift) & 0xff));
}

void putTag(QByteArray& out, const char (&tag)[5])
{
    out.append(tag, 4);
}

constexpr uint32_t kFmtChunkSize = 16;
constexpr uint32_t kSmplChunkSize = 36; // no loops

} // namespace

namespace WavWriter {

QByteArray build(const std::vector<uint8_t>& pcm, const WavFormat& format, int rootKey)
{
    const auto dataSize = static_cast<uint32_t>(pcm.size());
    const uint32_t pad = dataSize % 2; // RIFF chunks are word-aligned
    const uint32_t blockAlign = static_cast<uint32_t>(format.channels * format.bitsPerSample / 8);
    const uint32_t byteRate = static_cast<uint32_t>(format.sampleRate) * blockAlign;

    const uint32_t riffSize = 4                          // "WAVE"
                              + 8 + kFmtChunkSize        // fmt
                              + 8 + dataSize + pad       // data
                              + 8 + kSmplChunkSize;      // smpl

    QByteArray out;
    out.reserve(static_cast<qsizetype>(riffSize + 8));

    putTag(out, "RIFF");
    putU32(out, riffSize);
    putTag(out, "WAVE");

    putTag(out, "fmt ");
    putU32(out, kFmtChunkSize);
    putU16(out, 1); // PCM
    putU16(out, static_cast<uint16_t>(format.channels));
    putU32(out, static_cast<uint32_t>(format.sampleRate));
    putU32(out, byteRate);
    putU16(out, static_cast<uint16_t>(blockAlign));
    putU16(out, static_cast<uint16_t>(format.bitsPerSample));

    putTag(out, "data");
    putU32(out, dataSize);
    out.append(reinterpret_cast<const char*>(pcm.data()), static_cast<qsizetype>(pcm.size()));
    if (pad)
        out.append('\0');

    putTag(out, "smpl");
    putU32(out, kSmplChunkSize);
    putU32(out, 0);                                                    // manufacturer
    putU32(out, 0);                                                    // product
    putU32(out, static_cast<uint32_t>(1000000000.0 / format.sampleRate)); // sample period (ns)
    putU32(out, static_cast<uint32_t>(rootKey));                       // MIDI unity note
    putU32(out, 0);                                                    // pitch fraction
    putU32(out, 0);                                                    // SMPTE format
    putU32(out, 0);                                                    // SMPTE offset
    putU32(out, 0);                                                    // number of loops
    putU32(out, 0);                                                    // sampler data

    return out;
}

bool write(const QString& path, const std::vector<uint8_t>& pcm, const WavFormat& format,
           int rootKey, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (error)
            *error = file.errorString();
        return false;
    }

    const QByteArray bytes = build(pcm, format, rootKey);
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace WavWriter
