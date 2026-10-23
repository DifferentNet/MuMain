// Writes test vectors for src/source/Core/Platform/web/MuPacketCrypto.cpp with OpenMU's own network
// classes (MUnique.OpenMU.Network), wired up like the desktop client's ClientLibrary
// (ClientLibrary/ConnectionManager.cs, ConnectInner) and like OpenMU's game server
// (DefaultTcpGameServerListener: PipelinedEncryptor out, PipelinedDecryptor in). See README.md.
//
// Usage: VectorGen <output file>
//
// Output, one line each, bytes in hex:
//   encrypt <name>                      a new client connection
//   send <plain packet>                 as passed to ConnectionManager_Send
//   wire <bytes>                        everything the client wrote to the socket for the sends above
//   session <name> <encrypted|plain>    a new client connection; plain is the connect server's
//   feed <bytes>                        bytes received from the server
//   packet <bytes>                      a packet handed to the native callback after that feed
//   end <ok|error>                      error: the client disconnected (malformed stream)
// The process exits with 1 when OpenMU didn't behave as this generator expected; the vectors still
// record what OpenMU did.

using System.Buffers;
using System.IO.Pipelines;
using System.Reflection;
using System.Text;
using Microsoft.Extensions.Logging.Abstractions;
using MUnique.OpenMU.Network;
using MUnique.OpenMU.Network.SimpleModulus;
using MUnique.OpenMU.Network.Xor;

if (args.Length != 1)
{
    Console.Error.WriteLine("usage: VectorGen <output file>");
    return 2;
}

var generator = new VectorGenerator();
await generator.RunAsync();
File.WriteAllText(args[0], generator.Output);
Console.WriteLine(generator.Summary);
return generator.Problems == 0 ? 0 : 1;

/// <summary>A chunk of received bytes, with how many packets the client should have passed on after it.</summary>
internal sealed record Feed(byte[] Bytes, int ExpectedPackets, bool ExpectDisconnect = false);

/// <summary>Creates the sessions and records what OpenMU does with them.</summary>
internal sealed class VectorGenerator
{
    private const byte C1 = 0xC1;
    private const byte C2 = 0xC2;
    private const byte C3 = 0xC3;
    private const byte C4 = 0xC4;
    private const int EncryptedBlockSize = 11;
    private const byte BlockSizeXorKey = 0x3D;

    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(5);
    private static readonly TimeSpan Settle = TimeSpan.FromMilliseconds(200);

    private readonly StringBuilder _output = new();
    private readonly Random _random = new(20261022);
    private int _sends;
    private int _encryptSessions;
    private int _sessions;
    private int _feeds;
    private int _packets;

    public string Output => this._output.ToString();

    public int Problems { get; private set; }

    public string Summary =>
        $"{this._encryptSessions} encrypt sessions with {this._sends} packets; "
        + $"{this._sessions} receive sessions with {this._feeds} feeds and {this._packets} packets; "
        + $"{this.Problems} problems";

    public async Task RunAsync()
    {
        var assembly = typeof(Connection).Assembly;
        var version = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion
                      ?? assembly.GetName().Version?.ToString();
        this._output.AppendLine($"# MUnique.OpenMU.Network {version} from {assembly.Location}");
        Console.WriteLine($"MUnique.OpenMU.Network {version} from {assembly.Location}");

        await this.WriteClientEncryptionAsync();
        await this.WriteServerStreamsAsync();
        await this.WriteBrokenServerStreamsAsync();
        await this.WriteOddServerStreamsAsync();
        await this.WriteConnectServerStreamsAsync();
    }

    private static string Hex(byte[] bytes) => Convert.ToHexString(bytes);

    private static int HeaderSize(byte type) => type is C1 or C3 ? 2 : 3;

    private static List<byte[]> SplitPackets(byte[] stream)
    {
        var packets = new List<byte[]>();
        for (var offset = 0; offset < stream.Length;)
        {
            var size = stream.AsSpan(offset).GetPacketSize();
            if (size == 0 || offset + size > stream.Length)
            {
                throw new InvalidOperationException($"Can't split the stream at {offset}");
            }

            packets.Add(stream[offset..(offset + size)]);
            offset += size;
        }

        return packets;
    }

    private static async Task<byte[]> ReadToEndAsync(PipeReader reader)
    {
        using var bytes = new MemoryStream();
        while (true)
        {
            var result = await reader.ReadAsync();
            foreach (var segment in result.Buffer)
            {
                bytes.Write(segment.Span);
            }

            reader.AdvanceTo(result.Buffer.End);
            if (result.IsCompleted || result.IsCanceled)
            {
                break;
            }
        }

        await reader.CompleteAsync();
        return bytes.ToArray();
    }

    /// <summary>The server's encryption (Season6Episode3NetworkEncryptionFactoryPlugIn: PipelinedEncryptor).</summary>
    private static async Task<List<byte[]>> SendLikeServerAsync(IEnumerable<byte[]> packets)
    {
        var socket = new Pipe();
        var encryptor = new PipelinedEncryptor(socket.Writer);
        var wire = ReadToEndAsync(socket.Reader);
        foreach (var packet in packets)
        {
            await encryptor.Writer.WriteAsync(packet);
        }

        await encryptor.Writer.CompleteAsync();
        return SplitPackets(await wire.WaitAsync(Timeout));
    }

    /// <summary>The server's decryption (PipelinedDecryptor: SimpleModulus, then Xor32).</summary>
    private static async Task<byte[]> ReceiveLikeServerAsync(byte[] wire)
    {
        var socket = new Pipe();
        var decryptor = new PipelinedDecryptor(socket.Reader);
        var plain = ReadToEndAsync(decryptor.Reader);
        await socket.Writer.WriteAsync(wire);
        await socket.Writer.CompleteAsync();
        return await plain.WaitAsync(Timeout);
    }

    private static List<Feed> OneFeedPerPacket(IEnumerable<byte[]> packets, int alreadyPassedOn = 0)
    {
        var feeds = new List<Feed>();
        var count = alreadyPassedOn;
        foreach (var packet in packets)
        {
            feeds.Add(new Feed(packet, ++count));
        }

        return feeds;
    }

    private void Problem(string message)
    {
        this.Problems++;
        Console.Error.WriteLine("PROBLEM: " + message);
    }

    private byte[] MakePacket(byte type, int size)
    {
        var packet = new byte[size];
        this._random.NextBytes(packet);
        packet[0] = type;
        packet.AsSpan().SetPacketSize();
        return packet;
    }

    private byte[] MakeRandomPacket(int maxLongSize)
    {
        var type = (byte)(C1 + this._random.Next(4));
        var maxSize = type switch
        {
            C1 => 255,
            C3 => 185, // a bigger C3 doesn't fit its length field once encrypted
            _ => this._random.Next(10) == 0 ? maxLongSize : 600,
        };
        return this.MakePacket(type, this._random.Next(3, maxSize + 1));
    }

    /// <summary>Every packet kind and size up to 40 bytes, the size limits, and random ones; over 256 C3/C4, so the counter wraps.</summary>
    private List<byte[]> MakePacketMix(int randomCount, int maxLongSize)
    {
        var packets = new List<byte[]>();
        for (var size = 3; size <= 40; size++)
        {
            packets.Add(this.MakePacket(C1, size));
            packets.Add(this.MakePacket(C3, size));
            packets.Add(this.MakePacket(C2, size));
            packets.Add(this.MakePacket(C4, size));
        }

        foreach (var size in new[] { 255, 254, 248, 247 })
        {
            packets.Add(this.MakePacket(C1, size));
        }

        foreach (var size in new[] { 185, 184, 178, 177, 176 })
        {
            packets.Add(this.MakePacket(C3, size));
        }

        foreach (var size in new[] { 255, 256, 257, 258, 1000, 4096, 4097, 9000 })
        {
            packets.Add(this.MakePacket(C2, size));
            packets.Add(this.MakePacket(C4, size));
        }

        for (var i = 0; i < randomCount; i++)
        {
            packets.Add(this.MakeRandomPacket(maxLongSize));
        }

        return packets;
    }

    /// <summary>The received stream in chunks of random size; a packet is passed on once complete.</summary>
    private List<Feed> ChunkRandomly(IReadOnlyList<byte[]> packets, int minChunk, int maxChunk, int alreadyPassedOn = 0)
    {
        var stream = packets.SelectMany(p => p).ToArray();
        var ends = new List<int>();
        var end = 0;
        foreach (var packet in packets)
        {
            end += packet.Length;
            ends.Add(end);
        }

        var feeds = new List<Feed>();
        for (var offset = 0; offset < stream.Length;)
        {
            var size = Math.Min(this._random.Next(minChunk, maxChunk + 1), stream.Length - offset);
            offset += size;
            var complete = ends.Count(e => e <= offset);
            feeds.Add(new Feed(stream[(offset - size)..offset], alreadyPassedOn + complete));
        }

        return feeds;
    }

    private async Task WriteClientEncryptionAsync()
    {
        var packets = this.MakePacketMix(600, 9000);
        var wire = await this.RecordEncryptionAsync("client-mix", packets);

        // The server must get the same packets back.
        var decrypted = await ReceiveLikeServerAsync(wire);
        if (!decrypted.AsSpan().SequenceEqual(packets.SelectMany(p => p).ToArray()))
        {
            this.Problem("client-mix: the server's PipelinedDecryptor doesn't give back the sent packets");
        }

        // Lengths in the header that disagree with the size (ConnectionManager_Send overwrites them),
        // and packets too big for their length field once encrypted (the length wraps).
        var odd = new List<byte[]>();
        var wrongLength = this.MakePacket(C1, 20);
        wrongLength[1] = 0x10;
        odd.Add(wrongLength);
        wrongLength = this.MakePacket(C4, 300);
        wrongLength[1] = 0x00;
        wrongLength[2] = 0x07;
        odd.Add(wrongLength);
        for (var size = 186; size <= 255; size++)
        {
            odd.Add(this.MakePacket(C3, size));
        }

        odd.Add(this.MakePacket(C4, 47658));
        odd.Add(this.MakePacket(C4, 47659));
        odd.Add(this.MakePacket(C4, 65535));
        odd.AddRange(this.MakePacketMix(20, 600));
        await this.RecordEncryptionAsync("client-odd", odd);
    }

    /// <summary>Sends the packets through a client connection like ConnectionWrapper.Send and records the wire.</summary>
    private async Task<byte[]> RecordEncryptionAsync(string name, IReadOnlyList<byte[]> packets)
    {
        await using var client = new ClientConnection(encrypted: true);
        var wire = ReadToEndAsync(client.Wire);
        this._output.AppendLine($"encrypt {name}");
        foreach (var packet in packets)
        {
            this._output.AppendLine("send " + Hex(packet));
            var bytes = (byte[])packet.Clone();
            bytes.AsSpan().SetPacketSize(); // ConnectionManager.Send
            bytes.CopyTo(client.Output.GetSpan(bytes.Length)); // ConnectionWrapper.Send
            client.Output.Advance(bytes.Length);

            // OutboundFlushLoop flushes asynchronously, so a flush sometimes covers several sends.
            if (this._random.Next(3) == 0)
            {
                await client.Output.FlushAsync();
            }
        }

        await client.Output.FlushAsync();
        await client.Output.CompleteAsync();
        var bytesOnWire = await wire.WaitAsync(Timeout);
        this._output.AppendLine("wire " + Hex(bytesOnWire));
        this._encryptSessions++;
        this._sends += packets.Count;
        return bytesOnWire;
    }

    private async Task WriteServerStreamsAsync()
    {
        var packets = this.MakePacketMix(700, 6000);
        var wire = await SendLikeServerAsync(packets);
        if (wire.Count != packets.Count)
        {
            this.Problem("server stream: unexpected packet count");
        }

        await this.RecordSessionAsync("server-small-chunks", true, this.ChunkRandomly(wire, 1, 40), packets);
        await this.RecordSessionAsync("server-big-chunks", true, this.ChunkRandomly(wire, 200, 9000), packets);
        await this.RecordSessionAsync("server-one-chunk", true, this.ChunkRandomly(wire, int.MaxValue - 1, int.MaxValue - 1), packets);
        await this.RecordSessionAsync("server-one-packet-per-feed", true, OneFeedPerPacket(wire), packets);
        var start = wire.Take(80).ToList();
        await this.RecordSessionAsync("server-byte-by-byte", true, this.ChunkRandomly(start, 1, 1), packets.Take(80).ToList());
    }

    /// <summary>Malformed streams: OpenMU disconnects at the broken packet.</summary>
    private async Task WriteBrokenServerStreamsAsync()
    {
        var good = new List<byte[]> { this.MakePacket(C1, 10), this.MakePacket(C3, 30), this.MakePacket(C2, 300), this.MakePacket(C4, 20) };
        var goodWire = await SendLikeServerAsync(good);

        async Task RecordBrokenAsync(string name, params byte[][] broken)
        {
            var feeds = this.ChunkRandomly(goodWire, 1, 25);
            feeds.AddRange(broken.Select((bytes, i) => new Feed(bytes, good.Count, i == broken.Length - 1)));
            await this.RecordSessionAsync(name, true, feeds);
        }

        // The C3/C4 packets after the good ones are the 3rd and later of the connection: counter 2.
        var next = await SendLikeServerAsync(good.Concat(new[] { this.MakePacket(C3, 40), this.MakePacket(C3, 12) }));
        var nextC3 = next[4];

        await RecordBrokenAsync("broken-first-byte", new byte[] { 0xA1, 0x05, 0x01, 0x02, 0x03 });

        var notMultiple = new byte[nextC3.Length + 1];
        nextC3.CopyTo(notMultiple, 0);
        notMultiple.AsSpan().SetPacketSize();
        await RecordBrokenAsync("broken-not-whole-blocks", notMultiple);

        var badData = (byte[])nextC3.Clone();
        badData[2] ^= 0x01;
        await RecordBrokenAsync("broken-block-data", badData);

        // Only the checksum wrong: the length byte changes along, so the block's length stays valid.
        var lastBlock = ((nextC3.Length - 2) / EncryptedBlockSize) - 1;
        foreach (var (name, block) in new[] { ("broken-checksum-first-block", 0), ("broken-checksum-last-block", lastBlock) })
        {
            var badChecksum = (byte[])nextC3.Clone();
            var suffix = 2 + (block * EncryptedBlockSize) + EncryptedBlockSize - 2;
            badChecksum[suffix] ^= 0x40;
            badChecksum[suffix + 1] ^= 0x40;
            await RecordBrokenAsync(name, badChecksum);
        }

        var blockTooBig = (byte[])nextC3.Clone();
        blockTooBig[2 + EncryptedBlockSize - 2] = (byte)(9 ^ BlockSizeXorKey ^ blockTooBig[2 + EncryptedBlockSize - 1]);
        await RecordBrokenAsync("broken-block-size", blockTooBig);

        // The packet with counter 2 left out: the next one has 3.
        await RecordBrokenAsync("broken-counter", next[5]);

        await RecordBrokenAsync("broken-empty-c4", new byte[] { C4, 0x00, 0x03 });

        // A 2 byte packet is read once a third byte arrives; the decryptor can't take it.
        await RecordBrokenAsync("broken-two-byte-c1", new byte[] { C1, 0x02 }, new byte[] { C1, 0x03, 0x00 });
    }

    /// <summary>Crafted blocks OpenMU accepts: a block shorter than 8 before the last one, a C3 holding only the counter.</summary>
    private async Task WriteOddServerStreamsAsync()
    {
        // A short first block: the next block's bytes go right behind its content.
        var c4 = await SendLikeServerAsync(new[] { this.MakePacket(C4, 30) });
        var shortBlock = c4[0];
        SetBlockSize(shortBlock, 3, 0, 5);
        await this.RecordSessionAsync("odd-short-first-block", true, OneFeedPerPacket(new[] { shortBlock }));

        // A first block of length 0: OpenMU then checks the counter in the second block.
        foreach (var (name, eighthByte, ok) in new[] { ("odd-empty-first-block-counter-match", (byte)0, true), ("odd-empty-first-block-counter-mismatch", (byte)0x55, false) })
        {
            var plain = this.MakePacket(C4, 30);
            plain[3 + 7] = eighthByte; // counted byte 8 = content byte 7; the first packet's counter is 0
            var encrypted = (await SendLikeServerAsync(new[] { plain }))[0];
            SetBlockSize(encrypted, 3, 0, 0);
            await this.RecordSessionAsync(name, true, new List<Feed> { new(encrypted, ok ? 1 : 0, !ok) });
        }

        // A C3 holding only the counter decrypts to 2 bytes, which wait for the next packet.
        var packets = new[] { this.MakePacket(C1, 5), this.MakePacket(C3, 3), this.MakePacket(C1, 6), this.MakePacket(C3, 3), this.MakePacket(C3, 9) };
        var wire = await SendLikeServerAsync(packets);
        SetBlockSize(wire[1], 2, 0, 1);
        SetBlockSize(wire[3], 2, 0, 1);
        var feeds = new List<Feed> { new(wire[0], 1), new(wire[1], 1), new(wire[2], 3), new(wire[3], 3), new(wire[4], 5) };
        await this.RecordSessionAsync("odd-counter-only-c3", true, feeds);
        await this.RecordSessionAsync("odd-counter-only-c3-at-end", true, new List<Feed> { new(wire[0], 1), new(wire[1], 1) });
    }

    /// <summary>Changes the length a block states, keeping its checksum.</summary>
    private static void SetBlockSize(byte[] packet, int headerSize, int block, int size)
    {
        var suffix = headerSize + (block * EncryptedBlockSize) + EncryptedBlockSize - 2;
        packet[suffix] = (byte)(size ^ BlockSizeXorKey ^ packet[suffix + 1]);
    }

    /// <summary>The connect server's stream: packets are only split (C3/C4 too).</summary>
    private async Task WriteConnectServerStreamsAsync()
    {
        var packets = this.MakePacketMix(300, 3000);
        await this.RecordSessionAsync("connect-small-chunks", false, this.ChunkRandomly(packets, 1, 30), packets);
        await this.RecordSessionAsync("connect-one-chunk", false, this.ChunkRandomly(packets, int.MaxValue - 1, int.MaxValue - 1), packets);

        // A 2 byte packet waits until a third byte arrives.
        var feeds = new List<Feed>
        {
            new(new byte[] { C1, 0x02 }, 0),
            new(new byte[] { C1, 0x03, 0x00 }, 2),
            new(new byte[] { C1, 0x02 }, 2),
            new(new byte[] { C1, 0x02 }, 3),
            new(new byte[] { C1, 0x04, 0x01, 0x02 }, 5),
            new(new byte[] { C1, 0x02 }, 5),
        };
        await this.RecordSessionAsync("connect-two-byte-packets", false, feeds);

        var good = packets.Take(10).ToList();
        feeds = this.ChunkRandomly(good, 1, 30);
        feeds.Add(new Feed(new byte[] { 0x00, 0x05, 0x01, 0x02, 0x03 }, good.Count, true));
        await this.RecordSessionAsync("connect-broken-first-byte", false, feeds);
    }

    /// <summary>Feeds the chunks to a client connection and records the packets it passes on.</summary>
    private async Task RecordSessionAsync(string name, bool encrypted, IReadOnlyList<Feed> feeds, IReadOnlyList<byte[]>? expectedPackets = null)
    {
        await using var client = new ClientConnection(encrypted);
        this._output.AppendLine($"session {name} {(encrypted ? "encrypted" : "plain")}");
        var recorded = 0;
        var expectDisconnect = false;
        foreach (var feed in feeds)
        {
            this._output.AppendLine("feed " + Hex(feed.Bytes));
            await client.ReceiveAsync(feed.Bytes);
            if (!client.WaitFor(feed.ExpectedPackets, feed.ExpectDisconnect, Timeout))
            {
                this.Problem($"{name}: after feed {this._feeds} expected {feed.ExpectedPackets} packets{(feed.ExpectDisconnect ? " and a disconnect" : string.Empty)}, got {client.PacketCount}{(client.IsDisconnected ? " and a disconnect" : string.Empty)}");
            }

            recorded = this.RecordPackets(client, recorded);
            this._feeds++;
            expectDisconnect = feed.ExpectDisconnect;
        }

        await Task.Delay(Settle);
        if (client.PacketCount != recorded)
        {
            this.Problem($"{name}: {client.PacketCount - recorded} packets arrived late");
            this.RecordPackets(client, recorded);
        }

        var disconnected = client.IsDisconnected;
        this._output.AppendLine($"end {(disconnected ? "error" : "ok")}");
        if (disconnected != expectDisconnect)
        {
            this.Problem($"{name}: disconnected {disconnected}, expected {expectDisconnect}");
        }

        if (expectedPackets is not null && !client.Packets.SequenceEqual(expectedPackets, ByteArrayComparer.Instance))
        {
            this.Problem($"{name}: the client didn't pass on the packets the server sent");
        }

        this._sessions++;
    }

    private int RecordPackets(ClientConnection client, int recorded)
    {
        var packets = client.Packets;
        for (; recorded < packets.Count; recorded++)
        {
            this._output.AppendLine("packet " + Hex(packets[recorded]));
            this._packets++;
        }

        return recorded;
    }
}

/// <summary>A client connection set up like ConnectionManager.ConnectInner, on an in-memory socket.</summary>
internal sealed class ClientConnection : IAsyncDisposable
{
    private readonly TestSocket _socket = new();
    private readonly Connection _connection;
    private readonly object _gate = new();
    private readonly List<byte[]> _packets = new();
    private bool _disconnected;

    public ClientConnection(bool encrypted)
    {
        var encryptor = encrypted ? new PipelinedXor32Encryptor(new PipelinedSimpleModulusEncryptor(this._socket.Output, PipelinedSimpleModulusEncryptor.DefaultClientKey).Writer) : null;
        var decryptor = encrypted ? new PipelinedSimpleModulusDecryptor(this._socket.Input, PipelinedSimpleModulusDecryptor.DefaultClientKey) : null;
        this._connection = new Connection(this._socket, decryptor, encryptor, new NullLogger<Connection>());
        this._connection.PacketReceived += this.OnPacketReceivedAsync;
        this._connection.Disconnected += this.OnDisconnectedAsync;
        _ = Task.Run(this._connection.BeginReceiveAsync); // ConnectionWrapper.BeginReceive
    }

    public PipeWriter Output => this._connection.Output;

    public PipeReader Wire => this._socket.Wire;

    public int PacketCount
    {
        get
        {
            lock (this._gate)
            {
                return this._packets.Count;
            }
        }
    }

    public List<byte[]> Packets
    {
        get
        {
            lock (this._gate)
            {
                return this._packets.ToList();
            }
        }
    }

    public bool IsDisconnected
    {
        get
        {
            lock (this._gate)
            {
                return this._disconnected;
            }
        }
    }

    public async Task ReceiveAsync(byte[] bytes)
    {
        await this._socket.FromServer.WriteAsync(bytes);
    }

    /// <summary>Waits until the packets were passed on (and the connection dropped, if expected).</summary>
    public bool WaitFor(int packetCount, bool disconnect, TimeSpan timeout)
    {
        var deadline = DateTime.UtcNow + timeout;
        lock (this._gate)
        {
            while (disconnect ? !this._disconnected : this._packets.Count < packetCount && !this._disconnected)
            {
                var remaining = deadline - DateTime.UtcNow;
                if (remaining <= TimeSpan.Zero)
                {
                    break;
                }

                Monitor.Wait(this._gate, remaining);
            }

            return this._packets.Count == packetCount && this._disconnected == disconnect;
        }
    }

    /// <summary>Ends the server's side of the stream, which stops the connection's readers.</summary>
    public async ValueTask DisposeAsync()
    {
        await this._socket.FromServer.CompleteAsync();
    }

    private ValueTask OnPacketReceivedAsync(ReadOnlySequence<byte> packet)
    {
        lock (this._gate)
        {
            this._packets.Add(packet.ToArray());
            Monitor.PulseAll(this._gate);
        }

        return ValueTask.CompletedTask;
    }

    private ValueTask OnDisconnectedAsync()
    {
        lock (this._gate)
        {
            this._disconnected = true;
            Monitor.PulseAll(this._gate);
        }

        return ValueTask.CompletedTask;
    }
}

/// <summary>Stands in for the TCP socket: the server's bytes go in, the client's come out.</summary>
internal sealed class TestSocket : IDuplexPipe
{
    private readonly Pipe _fromServer = new();
    private readonly Pipe _toServer = new();

    public PipeReader Input => this._fromServer.Reader;

    public PipeWriter Output => this._toServer.Writer;

    public PipeWriter FromServer => this._fromServer.Writer;

    public PipeReader Wire => this._toServer.Reader;
}

/// <summary>Compares byte arrays by content.</summary>
internal sealed class ByteArrayComparer : IEqualityComparer<byte[]>
{
    public static readonly ByteArrayComparer Instance = new();

    public bool Equals(byte[]? x, byte[]? y) => x is not null && y is not null && x.AsSpan().SequenceEqual(y);

    public int GetHashCode(byte[] obj) => obj.Length;
}
