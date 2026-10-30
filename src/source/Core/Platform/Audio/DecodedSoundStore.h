#pragma once

// Sound effects for MiniAudioBackend, decoded once into 16-bit samples at the engine's sample rate
// and lent to the engine's resource manager under their file name, so ma_sound_init_from_file plays
// them as they are. miniaudio's own decoding keeps 32-bit floats, twice the memory: 115 MB for the
// sounds the game loads before its login screen (2026-10-30). The files hold 16 or 8 bits, and each
// stored sample is within half a 16-bit step of the float miniaudio would play.

#include "miniaudio.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace mu
{

class DecodedSoundStore
{
public:
    // The engine whose resource manager gets the sounds; set before Acquire.
    void Attach(ma_engine* engine);

    // Decodes the file at `path`, or counts one more holder of it. False when it can't (a file of
    // unknown length, say): miniaudio then decodes the file itself, as before.
    bool Acquire(const std::string& path);

    // One holder fewer, after its sounds were uninitialised; the samples go with the last one.
    void Release(const std::string& path);

    // Lets every sound go; before the engine is uninitialised.
    void ReleaseAll();

private:
    struct DecodedSound
    {
        std::unique_ptr<ma_int16[]> samples;
        int holders = 0;
    };

    bool Decode(const std::string& path, DecodedSound& sound) const;

    ma_engine* m_engine = nullptr;
    std::unordered_map<std::string, DecodedSound> m_sounds;
};

} // namespace mu
