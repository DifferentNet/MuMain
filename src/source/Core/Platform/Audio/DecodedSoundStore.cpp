#include "stdafx.h"

#include "DecodedSoundStore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{
constexpr ma_format StoredFormat = ma_format_s16;
constexpr ma_uint32 KeepChannels = 0;
// Decoded as floats a piece at a time, then stored in 16 bits: the decoder's own 16-bit output
// resamples in whole numbers, a few steps away from what miniaudio plays.
constexpr ma_uint64 PieceFrames = 4096;

// The decoder miniaudio's own decoding uses (ma_resource_manager__init_decoder_config): floats at
// the engine's rate, the same resampling.
ma_decoder_config DecoderConfig(ma_engine* engine, const ma_resource_manager& resources)
{
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, KeepChannels, ma_engine_get_sample_rate(engine));
    config.allocationCallbacks = resources.config.allocationCallbacks;
    config.ppCustomBackendVTables = resources.config.ppCustomDecodingBackendVTables;
    config.customBackendCount = resources.config.customDecodingBackendCount;
    config.pCustomBackendUserData = resources.config.pCustomDecodingBackendUserData;
    config.resampling = resources.config.resampling;
    return config;
}

// The engine plays a 16-bit sample as sample / 32768 (ma_pcm_s16_to_f32).
constexpr float SixteenBitScale = 32768.0f;
constexpr float LowestSample = -32768.0f;
constexpr float HighestSample = 32767.0f;

// Floats to the nearest 16-bit step the engine reads back. miniaudio's ma_pcm_f32_to_s16 scales by
// 32767 and cuts the fraction off, up to two steps away.
void StoreSamples(const float* floats, ma_int16* samples, ma_uint64 count)
{
    for (ma_uint64 i = 0; i < count; ++i)
    {
        const float scaled = std::clamp(floats[i] * SixteenBitScale, LowestSample, HighestSample);
        samples[i] = static_cast<ma_int16>(std::lrint(scaled));
    }
}

// Decodes the next `count` frames into 16-bit `samples`; returns how many came.
ma_uint64 ReadPiece(ma_decoder& decoder, std::vector<float>& piece, ma_int16* samples, ma_uint64 count)
{
    ma_uint64 read = 0;
    ma_decoder_read_pcm_frames(&decoder, piece.data(), count, &read);
    StoreSamples(piece.data(), samples, read * decoder.outputChannels);
    return read;
}

// Every frame of the decoder into `samples`, padded with silence as miniaudio pads its own buffers.
// Returns the frame count, 0 when the length isn't known up front or nothing could be read.
ma_uint64 ReadAllFrames(ma_decoder& decoder, std::unique_ptr<ma_int16[]>& samples)
{
    ma_uint64 length = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) != MA_SUCCESS || length == 0)
    {
        return 0;
    }
    const ma_uint64 channels = decoder.outputChannels;
    if (channels == 0 || length > SIZE_MAX / (channels * sizeof(float)))
    {
        return 0;
    }

    samples = std::make_unique<ma_int16[]>(static_cast<std::size_t>(length * channels));
    std::vector<float> piece(static_cast<std::size_t>(PieceFrames * channels));
    ma_uint64 done = 0;
    while (done < length)
    {
        const ma_uint64 count = std::min(PieceFrames, length - done);
        const ma_uint64 read = ReadPiece(decoder, piece, samples.get() + done * channels, count);
        done += read;
        if (read < count)
        {
            break;
        }
    }
    if (done == 0)
    {
        samples.reset();
        return 0;
    }
    return length;
}
} // namespace

namespace mu
{

void DecodedSoundStore::Attach(ma_engine* engine)
{
    m_engine = engine;
}

bool DecodedSoundStore::Acquire(const std::string& path)
{
    if (m_engine == nullptr)
    {
        return false;
    }
    const auto held = m_sounds.find(path);
    if (held != m_sounds.end())
    {
        ++held->second.holders;
        return true;
    }

    DecodedSound sound;
    if (!Decode(path, sound))
    {
        return false;
    }
    sound.holders = 1;
    m_sounds.emplace(path, std::move(sound));
    return true;
}

void DecodedSoundStore::Release(const std::string& path)
{
    const auto held = m_sounds.find(path);
    if (held == m_sounds.end() || --held->second.holders > 0)
    {
        return;
    }
    ma_resource_manager_unregister_data(ma_engine_get_resource_manager(m_engine), path.c_str());
    m_sounds.erase(held);
}

void DecodedSoundStore::ReleaseAll()
{
    for (const auto& [path, sound] : m_sounds)
    {
        ma_resource_manager_unregister_data(ma_engine_get_resource_manager(m_engine), path.c_str());
    }
    m_sounds.clear();
}

bool DecodedSoundStore::Decode(const std::string& path, DecodedSound& sound) const
{
    ma_resource_manager* resources = ma_engine_get_resource_manager(m_engine);
    if (resources == nullptr)
    {
        return false;
    }

    const ma_decoder_config config = DecoderConfig(m_engine, *resources);
    ma_decoder decoder;
    if (ma_decoder_init_vfs(resources->config.pVFS, path.c_str(), &config, &decoder) != MA_SUCCESS)
    {
        return false;
    }
    const ma_uint64 frames = ReadAllFrames(decoder, sound.samples);
    const ma_uint32 channels = decoder.outputChannels;
    const ma_uint32 sampleRate = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);
    if (frames == 0)
    {
        return false;
    }

    return ma_resource_manager_register_decoded_data(resources, path.c_str(), sound.samples.get(), frames, StoredFormat,
                                                     channels, sampleRate) == MA_SUCCESS;
}

} // namespace mu
