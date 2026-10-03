#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

struct WavFormat
{
    int channels = 1;
    int sampleRate = 44100;
    int bitsPerSample = 16;
};

namespace WavWriter {

// Builds a complete WAV file in memory.
// Layout: RIFF header, `fmt ` chunk, `data` chunk, then a `smpl` chunk carrying the
// MIDI root key. The `smpl` chunk comes last so the first 44 bytes are the canonical
// header that simple hardware readers expect.
QByteArray build(const std::vector<uint8_t>& pcm, const WavFormat& format, int rootKey);

// Writes the file atomically. On failure returns false and fills `error`.
bool write(const QString& path, const std::vector<uint8_t>& pcm, const WavFormat& format,
           int rootKey, QString* error);

} // namespace WavWriter
