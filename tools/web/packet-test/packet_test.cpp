// Checks the web build's generated packet builders and Send functions (tools/web/gen_web_packets.py)
// against the vectors GeneratePacketVectors.cs writes with the desktop library's own Send functions
// (see README.md for the format and how to run it).
// Usage: node packet_test.js <vector file>   (built with -sNODERAWFS=1, so it reads host files)
//
// Each vector is checked twice: through Build<Packet> called directly, and through the Send<Packet>
// export the client finds by name in mu::web::GeneratedPacketFunctions(), called through a pointer
// of the desktop function's type, with SendPacket below catching what it sends.

#include "Core/Platform/web/WebNetwork.h"
#include "WebPacketBuilders.generated.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t MaxFailuresShown = 30;
constexpr std::int32_t TestHandle = 7;

// What the generated Send functions sent since the last clear.
std::vector<std::pair<std::int32_t, Bytes>> SentPackets;

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

std::string ToHex(const Bytes& bytes)
{
    static const char Digits[] = "0123456789ABCDEF";
    std::string text;
    for (const std::uint8_t b : bytes)
    {
        text += Digits[b >> 4];
        text += Digits[b & 0xF];
    }
    return text;
}

// One argument of a vector (see GeneratePacketVectors.cs for the notation).
struct Arg
{
    char kind = 'u';
    std::uint64_t number = 0;
    std::u16string text;
    Bytes bytes;
};

// The arguments of a vector, handed to the generated calls by parameter index.
class Args
{
public:
    bool Parse(const std::vector<std::string>& tokens, std::string& error);

    std::size_t Count() const
    {
        return m_args.size();
    }

    std::uint8_t U8(std::size_t i) const
    {
        return static_cast<std::uint8_t>(m_args[i].number);
    }

    std::uint16_t U16(std::size_t i) const
    {
        return static_cast<std::uint16_t>(m_args[i].number);
    }

    std::uint32_t U32(std::size_t i) const
    {
        return static_cast<std::uint32_t>(m_args[i].number);
    }

    std::uint64_t U64(std::size_t i) const
    {
        return m_args[i].number;
    }

    float F32(std::size_t i) const
    {
        return static_cast<float>(m_args[i].number);
    }

    double F64(std::size_t i) const
    {
        return static_cast<double>(m_args[i].number);
    }

    // A null-terminated char16_t string, as the client passes it; nullptr for z.
    const char16_t* Str(std::size_t i) const
    {
        return m_args[i].kind == 'z' ? nullptr : m_args[i].text.c_str();
    }

    // The binary data; nullptr for z, a valid pointer for empty data.
    const std::uint8_t* Bin(std::size_t i) const
    {
        static const std::uint8_t Empty = 0;
        if (m_args[i].kind == 'z')
        {
            return nullptr;
        }
        return m_args[i].bytes.empty() ? &Empty : m_args[i].bytes.data();
    }

private:
    std::vector<Arg> m_args;
};

bool Args::Parse(const std::vector<std::string>& tokens, std::string& error)
{
    m_args.clear();
    for (const std::string& token : tokens)
    {
        Arg arg;
        arg.kind = token.empty() ? '?' : token[0];
        const std::string value = token.empty() ? std::string() : token.substr(1);
        if (arg.kind == 'u')
        {
            arg.number = std::stoull(value);
        }
        else if (arg.kind == 's')
        {
            Bytes units;
            if (value.size() % 4 != 0 || !ParseHex(value, units))
            {
                error = "bad string argument " + token;
                return false;
            }
            for (std::size_t c = 0; c < units.size(); c += 2)
            {
                arg.text += static_cast<char16_t>(units[c] << 8 | units[c + 1]);
            }
        }
        else if (arg.kind == 'b')
        {
            if (!ParseHex(value, arg.bytes))
            {
                error = "bad binary argument " + token;
                return false;
            }
        }
        else if (arg.kind != 'z')
        {
            error = "bad argument " + token;
            return false;
        }
        m_args.push_back(std::move(arg));
    }
    return true;
}

// The calls generated for one Send function of the desktop library.
struct PacketCalls
{
    const char* exportName;
    std::size_t parameterCount;
    Bytes (*build)(const Args&);
    void (*send)(void* function, std::int32_t handle, const Args& args);
};

#include "packet_calls.generated.inc"

// Reads the vector file and checks every vector in it.
class VectorChecker
{
public:
    explicit VectorChecker(std::string path);

    bool Run();

private:
    struct Current
    {
        const PacketCalls* calls = nullptr;
        void* sendFunction = nullptr;
        std::string line;
        Args args;
    };

    void HandleLine(const std::string& key, const std::string& value);
    void BeginVector(const std::string& value);
    void CheckBytes(const Bytes& expected);
    void CheckSentNothing(const std::string& reason);
    bool CallSend(Bytes& sent);
    void CheckCoverage();
    void Fail(const std::string& message);

    std::string m_path;
    std::size_t m_lineNumber = 0;
    std::map<std::string, const PacketCalls*> m_calls;
    std::map<std::string, void*> m_exports;
    Current m_current;
    std::set<std::string> m_checkedPackets;
    std::map<std::string, std::size_t> m_sentNothing;
    std::map<std::string, std::size_t> m_failedPackets;
    std::size_t m_vectors = 0;
    std::size_t m_failures = 0;
};

VectorChecker::VectorChecker(std::string path) : m_path(std::move(path))
{
    for (const PacketCalls& calls : AllPacketCalls)
    {
        m_calls[calls.exportName] = &calls;
    }
    for (const mu::web::NetworkSymbol& symbol : mu::web::GeneratedPacketFunctions())
    {
        m_exports[symbol.name] = symbol.function;
    }
}

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
        ++m_lineNumber;
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
            continue;
        }
        const std::size_t space = line.find(' ');
        HandleLine(line.substr(0, space), space == std::string::npos ? std::string() : line.substr(space + 1));
    }

    CheckCoverage();
    std::printf("  %zu packets, %zu vectors, each through Build<Packet> and the Send<Packet> export\n",
                m_checkedPackets.size(), m_vectors);
    std::size_t sentNothing = 0;
    for (const auto& [reason, count] : m_sentNothing)
    {
        sentNothing += count;
    }
    std::printf("  %zu vectors where the desktop library sends nothing (not compared):\n", sentNothing);
    for (const auto& [reason, count] : m_sentNothing)
    {
        std::printf("    %zu x %s\n", count, reason.c_str());
    }
    for (const auto& [name, count] : m_failedPackets)
    {
        std::printf("  %zu failures in %s\n", count, name.c_str());
    }
    std::printf("  %s: %zu failures\n", m_failures == 0 ? "OK" : "FAILED", m_failures);
    return m_failures == 0 && m_vectors > 0;
}

void VectorChecker::HandleLine(const std::string& key, const std::string& value)
{
    if (key == "send")
    {
        BeginVector(value);
        return;
    }
    if (m_current.calls == nullptr)
    {
        Fail(key + " without a send line");
        return;
    }
    if (key == "none")
    {
        CheckSentNothing(value);
    }
    else if (key == "bytes")
    {
        Bytes expected;
        if (!ParseHex(value, expected))
        {
            Fail("bad hex");
            return;
        }
        CheckBytes(expected);
    }
    else
    {
        Fail("unknown line " + key);
    }
    m_current = Current();
}

void VectorChecker::BeginVector(const std::string& value)
{
    m_current = Current();
    m_current.line = value;
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t end = value.find(' ', start);
        tokens.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? value.size() + 1 : end + 1;
    }

    const auto calls = m_calls.find(tokens[0]);
    if (calls == m_calls.end())
    {
        Fail("no generated call for " + tokens[0]);
        return;
    }
    std::string error;
    tokens.erase(tokens.begin());
    if (!m_current.args.Parse(tokens, error) || m_current.args.Count() != calls->second->parameterCount)
    {
        Fail(error.empty() ? "wrong number of arguments" : error);
        return;
    }
    m_current.calls = calls->second;
    const auto exported = m_exports.find(calls->first);
    m_current.sendFunction = exported == m_exports.end() ? nullptr : exported->second;
    m_checkedPackets.insert(calls->first);
    ++m_vectors;
}

void VectorChecker::CheckBytes(const Bytes& expected)
{
    const Bytes built = m_current.calls->build(m_current.args);
    if (built != expected)
    {
        Fail("Build: expected " + ToHex(expected) + "\n      got      " + ToHex(built));
    }

    Bytes sent;
    if (CallSend(sent) && sent != expected)
    {
        Fail("Send export: expected " + ToHex(expected) + "\n      got        " + ToHex(sent));
    }
}

// The desktop library sends nothing for these (OpenMU throws); the web build sends something anyway
// (see WebPacketWriting.h), so there's nothing to compare. Only counted.
void VectorChecker::CheckSentNothing(const std::string& reason)
{
    m_sentNothing[reason]++;
    Bytes sent;
    CallSend(sent);
}

bool VectorChecker::CallSend(Bytes& sent)
{
    if (m_current.sendFunction == nullptr)
    {
        Fail(std::string(m_current.calls->exportName) + " isn't in GeneratedPacketFunctions()");
        return false;
    }
    SentPackets.clear();
    m_current.calls->send(m_current.sendFunction, TestHandle, m_current.args);
    if (SentPackets.size() != 1 || SentPackets[0].first != TestHandle)
    {
        Fail("Send export: sent " + std::to_string(SentPackets.size()) + " packets, or on another handle");
        return false;
    }
    sent = SentPackets[0].second;
    return true;
}

// Every export should have vectors, and every desktop function an export.
void VectorChecker::CheckCoverage()
{
    m_current = Current();
    for (const auto& [name, function] : m_exports)
    {
        if (m_checkedPackets.count(name) == 0)
        {
            Fail("export " + name + " has no vectors (not a function of the desktop library?)");
        }
    }
}

void VectorChecker::Fail(const std::string& message)
{
    ++m_failures;
    m_failedPackets[m_current.calls != nullptr ? m_current.calls->exportName : "(no packet)"]++;
    if (m_failures > MaxFailuresShown)
    {
        return;
    }
    std::printf("  line %zu: %s\n", m_lineNumber, message.c_str());
    if (!m_current.line.empty())
    {
        std::printf("      send %s\n", m_current.line.substr(0, 300).c_str());
    }
}
} // namespace

// Stands in for WebNetwork.cpp's SendPacket: the generated Send functions end here.
void mu::web::SendPacket(std::int32_t handle, const std::uint8_t* data, std::size_t size)
{
    SentPackets.emplace_back(handle, Bytes(data, data + size));
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: node packet_test.js <vector file> ...\n");
        return 2;
    }
    bool ok = true;
    for (int i = 1; i < argc; ++i)
    {
        ok = VectorChecker(argv[i]).Run() && ok;
    }
    return ok ? 0 : 1;
}
