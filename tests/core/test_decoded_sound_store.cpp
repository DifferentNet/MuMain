#include "App/stdafx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "doctest.h"

#include "Core/Platform/Audio/DecodedSoundStore.h"

namespace
{
constexpr ma_uint32 EngineRate = 48000;
constexpr ma_uint32 FileRate = 22050;  // most of the game's sounds
constexpr ma_uint32 FileFrames = 2205; // 0.1 s
constexpr double ToneHz = 440.0;
constexpr double ToneLevel = 0.5;
constexpr float SixteenBitStep = 1.0f / 32768.0f;
// Rounding the resampled floats to 16 bits moves each sample by half a step at most (−96 dB).
constexpr float MostStepsApart = SixteenBitStep / 2.0f;

void AppendLittleEndian(std::vector<char>& bytes, std::uint32_t value, int size)
{
    for (int i = 0; i < size; ++i)
    {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

// A mono 16-bit WAV file holding a tone.
std::vector<char> ToneWav()
{
    constexpr std::uint32_t DataBytes = FileFrames * 2;
    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    AppendLittleEndian(bytes, 36 + DataBytes, 4);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    AppendLittleEndian(bytes, 16, 4);
    AppendLittleEndian(bytes, 1, 2); // PCM
    AppendLittleEndian(bytes, 1, 2); // mono
    AppendLittleEndian(bytes, FileRate, 4);
    AppendLittleEndian(bytes, FileRate * 2, 4);
    AppendLittleEndian(bytes, 2, 2);
    AppendLittleEndian(bytes, 16, 2);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    AppendLittleEndian(bytes, DataBytes, 4);
    for (ma_uint32 frame = 0; frame < FileFrames; ++frame)
    {
        const double phase = 2.0 * 3.14159265358979323846 * ToneHz * frame / FileRate;
        const auto sample = static_cast<std::int16_t>(std::lround(std::sin(phase) * ToneLevel * 32767.0));
        AppendLittleEndian(bytes, static_cast<std::uint16_t>(sample), 2);
    }
    return bytes;
}

// The same tone under two names: one for the store, one miniaudio decodes itself.
struct ToneFiles
{
    std::filesystem::path directory;
    std::string stored;
    std::string ownDecoding;

    ToneFiles()
    {
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory = std::filesystem::temp_directory_path() / ("mu_sound_store_" + std::to_string(timestamp));
        std::filesystem::create_directories(directory);
        stored = Write("stored.wav");
        ownDecoding = Write("own.wav");
    }

    ~ToneFiles()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    std::string Write(const char* name) const
    {
        const auto path = directory / name;
        const std::vector<char> wav = ToneWav();
        std::ofstream file(path, std::ios::binary);
        file.write(wav.data(), static_cast<std::streamsize>(wav.size()));
        return path.string();
    }
};

// An engine without a device, as the game's at 48 kHz.
struct SilentEngine
{
    ma_engine engine{};
    bool ready = false;

    SilentEngine()
    {
        ma_engine_config config = ma_engine_config_init();
        config.noDevice = MA_TRUE;
        config.channels = 2;
        config.sampleRate = EngineRate;
        ready = ma_engine_init(&config, &engine) == MA_SUCCESS;
    }

    ~SilentEngine()
    {
        if (ready)
        {
            ma_engine_uninit(&engine);
        }
    }
};

// A sound effect as MiniAudioBackend loads it.
struct LoadedSound
{
    ma_sound sound{};
    bool ready = false;

    LoadedSound(ma_engine& engine, const std::string& path)
    {
        ready = ma_sound_init_from_file(&engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &sound) ==
                MA_SUCCESS;
    }

    ~LoadedSound()
    {
        if (ready)
        {
            ma_sound_uninit(&sound);
        }
    }

    ma_format Format()
    {
        ma_format format = ma_format_unknown;
        ma_data_source_get_data_format(ma_sound_get_data_source(&sound), &format, nullptr, nullptr, nullptr, 0);
        return format;
    }

    ma_uint32 SampleRate()
    {
        ma_uint32 sampleRate = 0;
        ma_data_source_get_data_format(ma_sound_get_data_source(&sound), nullptr, nullptr, &sampleRate, nullptr, 0);
        return sampleRate;
    }

    // Every mono sample the sound holds, as floats.
    std::vector<float> Samples()
    {
        ma_data_source* source = ma_sound_get_data_source(&sound);
        ma_uint64 length = 0;
        ma_data_source_get_length_in_pcm_frames(source, &length);
        std::vector<float> samples(static_cast<std::size_t>(length));
        if (Format() == ma_format_f32)
        {
            ma_data_source_read_pcm_frames(source, samples.data(), length, nullptr);
            return samples;
        }
        std::vector<ma_int16> stored(samples.size());
        ma_data_source_read_pcm_frames(source, stored.data(), length, nullptr);
        ma_pcm_s16_to_f32(samples.data(), stored.data(), length, ma_dither_mode_none); // as the engine plays it
        return samples;
    }
};

float LargestDifference(const std::vector<float>& left, const std::vector<float>& right)
{
    float largest = 0.0f;
    for (std::size_t i = 0; i < std::min(left.size(), right.size()); ++i)
    {
        largest = std::max(largest, std::fabs(left[i] - right[i]));
    }
    return largest;
}
} // namespace

TEST_CASE("a stored sound plays as miniaudio's own decoding, in 16 bits [audio][store]")
{
    SilentEngine silent;
    REQUIRE(silent.ready);
    ToneFiles files;
    mu::DecodedSoundStore store;
    store.Attach(&silent.engine);
    REQUIRE(store.Acquire(files.stored));

    {
        LoadedSound stored(silent.engine, files.stored);
        LoadedSound own(silent.engine, files.ownDecoding);
        REQUIRE(stored.ready);
        REQUIRE(own.ready);

        CHECK(stored.Format() == ma_format_s16);
        CHECK(own.Format() == ma_format_f32);
        CHECK(stored.SampleRate() == EngineRate);
        const std::vector<float> storedSamples = stored.Samples();
        const std::vector<float> ownSamples = own.Samples();
        CHECK(storedSamples.size() == ownSamples.size());
        CHECK(storedSamples.size() > FileFrames * 2);
        CHECK(LargestDifference(storedSamples, ownSamples) <= MostStepsApart);
    }
    store.Release(files.stored);
}

TEST_CASE("stored samples stay while any slot holds them [audio][store]")
{
    SilentEngine silent;
    REQUIRE(silent.ready);
    ToneFiles files;
    mu::DecodedSoundStore store;
    store.Attach(&silent.engine);
    REQUIRE(store.Acquire(files.stored));
    REQUIRE(store.Acquire(files.stored));

    store.Release(files.stored);
    {
        LoadedSound stillStored(silent.engine, files.stored);
        CHECK(stillStored.Format() == ma_format_s16);
    }

    store.Release(files.stored);
    LoadedSound decodedAgain(silent.engine, files.stored);
    CHECK(decodedAgain.Format() == ma_format_f32);
}

TEST_CASE("letting every sound go hands the files back to miniaudio [audio][store]")
{
    SilentEngine silent;
    REQUIRE(silent.ready);
    ToneFiles files;
    mu::DecodedSoundStore store;
    store.Attach(&silent.engine);
    REQUIRE(store.Acquire(files.stored));

    store.ReleaseAll();
    LoadedSound decodedAgain(silent.engine, files.stored);
    CHECK(decodedAgain.Format() == ma_format_f32);
}

TEST_CASE("a file the store can't decode is left to miniaudio [audio][store]")
{
    ToneFiles files;
    mu::DecodedSoundStore detached;
    CHECK_FALSE(detached.Acquire(files.stored));

    SilentEngine silent;
    REQUIRE(silent.ready);
    mu::DecodedSoundStore store;
    store.Attach(&silent.engine);
    CHECK_FALSE(store.Acquire((files.directory / "missing.wav").string()));
    store.Release((files.directory / "missing.wav").string());
}
