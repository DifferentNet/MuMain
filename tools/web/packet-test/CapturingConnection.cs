// Stands in for ClientLibrary/ConnectionManager.cs and ConnectionWrapper.cs in the packet test (see
// README.md): the generated Send<Packet> functions look the handle up in Connections and call
// CreateAndSend, which here keeps the bytes the packet factory wrote instead of sending them.

namespace MUnique.Client.Library;

/// <summary>The part of the desktop library's ConnectionManager the generated functions use.</summary>
public unsafe partial class ConnectionManager
{
    /// <summary>The handle the test passes to every Send function.</summary>
    public const int TestHandle = 1;

    private static readonly Dictionary<int, CapturingConnection> Connections = new()
    {
        [TestHandle] = new CapturingConnection(),
    };

    /// <summary>Gets the connection behind <see cref="TestHandle"/>.</summary>
    public static CapturingConnection TestConnection => Connections[TestHandle];
}

/// <summary>A connection that records the packets the generated functions create.</summary>
public sealed class CapturingConnection
{
    /// <summary>Gets or sets the value the packet buffer is filled with before a packet is written.</summary>
    public byte BufferFill { get; set; }

    /// <summary>Gets the packets created since the last <see cref="Clear"/>.</summary>
    public List<byte[]> Packets { get; } = new();

    /// <summary>Gets the exceptions thrown by packet factories since the last <see cref="Clear"/>.</summary>
    public List<Exception> Errors { get; } = new();

    /// <summary>Forgets the recorded packets and errors.</summary>
    public void Clear()
    {
        this.Packets.Clear();
        this.Errors.Clear();
    }

    /// <summary>Like ConnectionWrapper.CreateAndSend: lets the factory write a packet and keeps it.</summary>
    /// <remarks>
    /// ConnectionWrapper passes the exception on to the Send function, which catches it and sends
    /// nothing; it's recorded here too, so the vector can say why nothing was sent.
    /// </remarks>
    public void CreateAndSend(Func<CapturingWriter, int> packetFactory)
    {
        var writer = new CapturingWriter(this.BufferFill);
        try
        {
            var length = packetFactory(writer);
            this.Packets.Add(writer.Written(length));
        }
        catch (Exception ex)
        {
            this.Errors.Add(ex);
            throw;
        }
    }
}

/// <summary>The part of a PipeWriter the generated functions use.</summary>
public sealed class CapturingWriter
{
    private readonly byte _fill;
    private byte[] _buffer = Array.Empty<byte>();

    public CapturingWriter(byte fill)
    {
        this._fill = fill;
    }

    /// <summary>A buffer of exactly <paramref name="sizeHint"/> bytes, all set to the fill value.</summary>
    public Span<byte> GetSpan(int sizeHint)
    {
        this._buffer = new byte[sizeHint];
        Array.Fill(this._buffer, this._fill);
        return this._buffer;
    }

    /// <summary>The first <paramref name="length"/> bytes of the buffer.</summary>
    public byte[] Written(int length) => this._buffer[..length];
}
