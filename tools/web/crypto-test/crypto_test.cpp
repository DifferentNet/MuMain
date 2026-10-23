// Checks MuPacketCrypto against the vectors GenerateVectors.cs writes with OpenMU's own classes
// (see README.md for the format and how to run it).
// Usage: node crypto_test.js <vector file> ...   (built with -sNODERAWFS=1, so it reads host files)

#include "Core/Platform/web/MuPacketCrypto.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t MaxFailuresShown = 20;
constexpr std::size_t ContextBytes = 16;

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    return -1;
}

bool ParseHex(const std::string& text, Bytes& bytes)
{
    bytes.clear();
    if (text.size() % 2 != 0)
    {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); i += 2)
    {
        const int high = HexDigit(text[i]);
        const int low = HexDigit(text[i + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        bytes.push_back(static_cast<std::uint8_t>(high << 4 | low));
    }
    return true;
}

std::string ToHex(const std::uint8_t* bytes, std::size_t size)
{
    static const char Digits[] = "0123456789ABCDEF";
    std::string text;
    for (std::size_t i = 0; i < size; ++i)
    {
        text += Digits[bytes[i] >> 4];
        text += Digits[bytes[i] & 0xF];
    }
    return text;
}

// The bytes around offset, for failure messages.
std::string Around(const Bytes& bytes, std::size_t offset)
{
    const std::size_t start = offset > ContextBytes ? offset - ContextBytes : 0;
    const std::size_t end = std::min(bytes.size(), offset + ContextBytes);
    return "@" + std::to_string(start) + ": " + ToHex(bytes.data() + start, end - start);
}

// Reads one vector file and checks every session in it.
class VectorChecker
{
public:
    explicit VectorChecker(std::string path) : m_path(std::move(path))
    {
    }

    bool Run();

private:
    void HandleLine(const std::string& key, const std::string& value);
    void BeginEncrypt(const std::string& name);
    void Send(const Bytes& packet);
    void CheckWire(const Bytes& wire);
    void BeginSession(const std::string& nameAndMode);
    void FeedBytes(const Bytes& bytes);
    void ExpectPacket(const Bytes& packet);
    void EndSession(const std::string& result);
    void CheckNoExtraPackets();
    void Fail(const std::string& message);

    std::string m_path;
    std::size_t m_line = 0;
    std::string m_session;
    std::size_t m_failures = 0;

    std::unique_ptr<mu::web::MuPacketEncryptor> m_encryptor;
    Bytes m_wire;
    std::vector<std::size_t> m_sendEnds;

    std::unique_ptr<mu::web::MuPacketDecryptor> m_decryptor;
    std::deque<Bytes> m_passedOn;
    bool m_feedFailed = false;

    std::size_t m_encryptSessions = 0;
    std::size_t m_sends = 0;
    std::size_t m_wireBytes = 0;
    std::size_t m_sessions = 0;
    std::size_t m_feeds = 0;
    std::size_t m_packets = 0;
};

bool VectorChecker::Run()
{
    std::ifstream file(m_path);
    if (!file)
    {
        std::printf("can't read %s\n", m_path.c_str());
        return false;
    }

    std::printf("%s\n", m_path.c_str());
    std::string line;
    while (std::getline(file, line))
    {
        ++m_line;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        if (line[0] == '#')
        {
            std::printf("  %s\n", line.c_str());
            continue;
        }

        const std::size_t space = line.find(' ');
        const std::string key = line.substr(0, space);
        const std::string value = space == std::string::npos ? std::string() : line.substr(space + 1);
        HandleLine(key, value);
    }

    std::printf("  %zu encrypt sessions, %zu packets sent, %zu bytes on the wire\n", m_encryptSessions, m_sends,
                m_wireBytes);
    std::printf("  %zu receive sessions, %zu feeds, %zu packets passed on\n", m_sessions, m_feeds, m_packets);
    std::printf("  %s: %zu failures\n", m_failures == 0 ? "OK" : "FAILED", m_failures);
    return m_failures == 0 && (m_encryptSessions > 0 || m_sessions > 0);
}

void VectorChecker::HandleLine(const std::string& key, const std::string& value)
{
    if (key == "encrypt")
    {
        BeginEncrypt(value);
        return;
    }
    if (key == "session")
    {
        BeginSession(value);
        return;
    }
    if (key == "end")
    {
        EndSession(value);
        return;
    }

    Bytes bytes;
    if (!ParseHex(value, bytes))
    {
        Fail("bad hex");
        return;
    }
    if (key == "send")
    {
        Send(bytes);
    }
    else if (key == "wire")
    {
        CheckWire(bytes);
    }
    else if (key == "feed")
    {
        FeedBytes(bytes);
    }
    else if (key == "packet")
    {
        ExpectPacket(bytes);
    }
    else
    {
        Fail("unknown line " + key);
    }
}

void VectorChecker::BeginEncrypt(const std::string& name)
{
    m_session = name;
    m_encryptor = std::make_unique<mu::web::MuPacketEncryptor>();
    m_wire.clear();
    m_sendEnds.clear();
    ++m_encryptSessions;
}

void VectorChecker::Send(const Bytes& packet)
{
    if (!m_encryptor)
    {
        Fail("send outside an encrypt session");
        return;
    }
    const Bytes encrypted = m_encryptor->Encrypt(packet.data(), packet.size());
    if (encrypted.empty())
    {
        Fail("Encrypt refused packet " + std::to_string(m_sendEnds.size()));
    }
    m_wire.insert(m_wire.end(), encrypted.begin(), encrypted.end());
    m_sendEnds.push_back(m_wire.size());
    ++m_sends;
}

void VectorChecker::CheckWire(const Bytes& wire)
{
    m_wireBytes += wire.size();
    std::size_t offset = 0;
    while (offset < wire.size() && offset < m_wire.size() && wire[offset] == m_wire[offset])
    {
        ++offset;
    }
    if (offset == wire.size() && offset == m_wire.size())
    {
        return;
    }

    std::size_t send = 0;
    while (send < m_sendEnds.size() && m_sendEnds[send] <= offset)
    {
        ++send;
    }
    Fail("wire differs at byte " + std::to_string(offset) + " (packet " + std::to_string(send) + "; C# "
         + std::to_string(wire.size()) + " bytes, C++ " + std::to_string(m_wire.size()) + ")\n      C#:  "
         + Around(wire, offset) + "\n      C++: " + Around(m_wire, offset));
}

void VectorChecker::BeginSession(const std::string& nameAndMode)
{
    const std::size_t space = nameAndMode.find(' ');
    m_session = nameAndMode.substr(0, space);
    const std::string mode = space == std::string::npos ? std::string() : nameAndMode.substr(space + 1);
    if (mode != "encrypted" && mode != "plain")
    {
        Fail("unknown mode " + mode);
    }
    m_decryptor = std::make_unique<mu::web::MuPacketDecryptor>(mode == "encrypted");
    m_passedOn.clear();
    m_feedFailed = false;
    ++m_sessions;
}

void VectorChecker::FeedBytes(const Bytes& bytes)
{
    if (!m_decryptor)
    {
        Fail("feed outside a session");
        return;
    }
    CheckNoExtraPackets();
    if (m_feedFailed)
    {
        Fail("C++ reported a malformed stream before the last feed");
    }

    const bool ok = m_decryptor->Feed(bytes.data(), bytes.size(), [this](const std::uint8_t* packet, std::size_t size) {
        m_passedOn.emplace_back(packet, packet + size);
    });
    m_feedFailed = m_feedFailed || !ok;
    ++m_feeds;
}

void VectorChecker::ExpectPacket(const Bytes& packet)
{
    ++m_packets;
    if (m_passedOn.empty())
    {
        Fail("C# passed on a packet C++ didn't: " + ToHex(packet.data(), std::min(packet.size(), ContextBytes * 2)));
        return;
    }

    const Bytes passedOn = std::move(m_passedOn.front());
    m_passedOn.pop_front();
    if (passedOn != packet)
    {
        Fail("packet differs\n      C#:  " + ToHex(packet.data(), packet.size()) + "\n      C++: "
             + ToHex(passedOn.data(), passedOn.size()));
    }
}

void VectorChecker::EndSession(const std::string& result)
{
    CheckNoExtraPackets();
    const bool expectFailure = result == "error";
    if (expectFailure != m_feedFailed)
    {
        Fail(std::string("C# ended with ") + result + ", C++ " + (m_feedFailed ? "reported a malformed stream" : "didn't"));
    }
    m_decryptor.reset();
}

// Packets C++ passed on after the previous feed that C# didn't.
void VectorChecker::CheckNoExtraPackets()
{
    while (!m_passedOn.empty())
    {
        Fail("C++ passed on a packet C# didn't: " + ToHex(m_passedOn.front().data(), m_passedOn.front().size()));
        m_passedOn.pop_front();
    }
}

void VectorChecker::Fail(const std::string& message)
{
    ++m_failures;
    if (m_failures <= MaxFailuresShown)
    {
        std::printf("  FAIL %s line %zu: %s\n", m_session.c_str(), m_line, message.c_str());
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: node crypto_test.js <vector file> ...\n");
        return 2;
    }

    bool ok = true;
    for (int i = 1; i < argc; ++i)
    {
        VectorChecker checker(argv[i]);
        ok = checker.Run() && ok;
    }
    std::printf(ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
