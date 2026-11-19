#include "doctest.h"

#include "Core/Utilities/StreamBytes.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using Core::Utilities::ReadRemainingBytes;

namespace
{
std::string Pattern(std::size_t size)
{
    std::string text(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
    {
        text[i] = static_cast<char>((i * 131u + 7u) & 0xFFu);
    }
    return text;
}
} // namespace

TEST_CASE("ReadRemainingBytes returns a file's bytes from where the stream stands")
{
    const auto path = std::filesystem::temp_directory_path() / "mu_stream_bytes_test.bin";
    const std::string content = Pattern(70000); // more than a stream buffer, with zero and high bytes
    {
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    {
        std::ifstream input(path, std::ios::binary);
        const auto bytes = ReadRemainingBytes<unsigned char>(input);
        REQUIRE(bytes.size() == content.size());
        CHECK(std::equal(bytes.begin(), bytes.end(), content.begin(),
                         [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); }));
    }
    {
        std::ifstream input(path, std::ios::binary);
        input.seekg(24);
        const auto bytes = ReadRemainingBytes<char>(input);
        CHECK(std::string(bytes.begin(), bytes.end()) == content.substr(24));
    }
    std::filesystem::remove(path);
}

TEST_CASE("ReadRemainingBytes reads an empty file and an in-memory stream")
{
    const auto path = std::filesystem::temp_directory_path() / "mu_stream_bytes_empty.bin";
    {
        std::ofstream out(path, std::ios::binary);
    }
    {
        std::ifstream input(path, std::ios::binary);
        CHECK(ReadRemainingBytes<unsigned char>(input).empty());
    }
    std::filesystem::remove(path);

    std::istringstream text(Pattern(5000));
    const auto bytes = ReadRemainingBytes<char>(text);
    CHECK(std::string(bytes.begin(), bytes.end()) == Pattern(5000));
}

TEST_CASE("ReadRemainingBytes falls back to reading on where the stream can't seek")
{
    // A stream buffer without seeking, like a pipe.
    struct NoSeek : std::stringbuf
    {
        using std::stringbuf::stringbuf;
        pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override
        {
            return pos_type(off_type(-1));
        }
    };
    NoSeek buffer(Pattern(3000));
    std::istream input(&buffer);
    const auto bytes = ReadRemainingBytes<char>(input);
    CHECK(std::string(bytes.begin(), bytes.end()) == Pattern(3000));
}
