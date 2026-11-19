#pragma once

#include <istream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace Core::Utilities
{
// Every byte of `input` from where it stands to its end, in one read where the stream can tell its
// size (a file): an istreambuf_iterator loop takes it a byte at a time and grows the container as it
// goes, about 10 ns a byte on a phone for a map's 8-12 MB of textures. Same bytes either way.
// `Bytes` is a contiguous container of one-byte values (std::vector<unsigned char>, std::string).
template <typename Bytes> Bytes ReadRemaining(std::istream& input)
{
    static_assert(sizeof(typename Bytes::value_type) == 1 && std::is_trivial_v<typename Bytes::value_type>);
    Bytes bytes;
    const std::istream::pos_type start = input.tellg();
    if (start != std::istream::pos_type(-1) && input.seekg(0, std::ios::end))
    {
        const std::istream::pos_type end = input.tellg();
        input.seekg(start);
        if (end != std::istream::pos_type(-1) && end >= start && input)
        {
            bytes.resize(static_cast<std::size_t>(end - start));
            input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            bytes.resize(static_cast<std::size_t>(input.gcount()));
            // The size was right unless the file changed meanwhile: then the rest, if any, as below.
            if (input.eof() || input.peek() == std::istream::traits_type::eof())
            {
                return bytes;
            }
        }
    }
    input.clear();
    bytes.insert(bytes.end(), std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return bytes;
}

template <typename Byte> std::vector<Byte> ReadRemainingBytes(std::istream& input)
{
    return ReadRemaining<std::vector<Byte>>(input);
}

// The same as text: a whole text file in one read.
inline std::string ReadRemainingText(std::istream& input)
{
    return ReadRemaining<std::string>(input);
}
} // namespace Core::Utilities
