// Writes test vectors for the web build's generated packet builders (tools/web/gen_web_packets.py)
// with the desktop client's own Send<Packet> functions: GenerateExtensionsDotNet.xslt turns OpenMU's
// packet definitions into them (see PacketVectorGen.csproj), and they fill OpenMU's <Packet>Ref
// structs. See README.md.
//
// Usage: PacketVectorGen <packet XML folder> <vector file> <C++ call file>
//
// The parameters of each Send function are read from the compiled function itself, so the vectors
// and the C++ calls (packet_calls.generated.inc, included by packet_test.cpp) pass the arguments in
// the desktop library's order. The vector file has, one line each:
//   send <export name> <argument> ...   the arguments, one per parameter after the handle:
//                                        u<decimal> a number (enums as their 32-bit pattern),
//                                        s<UTF-16 code units, 4 hex digits each> a string,
//                                        b<hex bytes> binary data, z a null pointer
//   bytes <hex>                         the packet the desktop library sends for them
//   none <reason>                       it sends nothing (the Send function caught an exception)
// The process exits with 1 when a Send function did something this generator didn't expect.

using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using System.Text;
using System.Xml.Linq;
using MUnique.Client.Library;

if (args.Length != 3)
{
    Console.Error.WriteLine("usage: PacketVectorGen <packet XML folder> <vector file> <C++ call file>");
    return 2;
}

var definitions = PacketDefinitions.Load(args[0]);
var generator = new VectorGenerator(definitions, args[0]);
generator.Run();
File.WriteAllText(args[1], generator.Vectors);
File.WriteAllText(args[2], generator.CppCalls);
Console.WriteLine(generator.Summary);
foreach (var problem in generator.Problems)
{
    Console.WriteLine("problem: " + problem);
}

return generator.Problems.Count == 0 ? 0 : 1;

/// <summary>What a parameter of a Send function carries.</summary>
internal enum ParameterKind
{
    Byte,
    Boolean,
    UInt16,
    UInt32,
    UInt64,
    Enum,
    Float,
    Double,
    String,
    BinaryData,
    BinaryLength,
}

/// <summary>A field of a packet definition, as far as the vectors need it.</summary>
/// <param name="Type">The XML type (Byte, Boolean, String, Binary, ...).</param>
/// <param name="Length">The field's length (bytes, or bits for Byte and Enum fields); null if none.</param>
internal sealed record FieldDefinition(int Index, string Name, string Type, int? Length);

/// <summary>A packet definition: its fixed length (null if variable) and fields by parameter name.</summary>
internal sealed record PacketDefinition(string Name, int? Length, IReadOnlyDictionary<string, FieldDefinition> FieldsByParameter);

/// <summary>The packet definitions of the XML files the Send functions are generated from.</summary>
internal static class PacketDefinitions
{
    private static readonly XNamespace Ns = "http://www.munique.net/OpenMU/PacketDefinitions";
    private static readonly string[] Files = { "ClientToServer", "ChatServer", "ConnectServer" };

    /// <summary>Packet name (as in Send<Name>) -> definition.</summary>
    public static Dictionary<string, PacketDefinition> Load(string xmlDirectory)
    {
        var result = new Dictionary<string, PacketDefinition>();
        foreach (var file in Files)
        {
            var document = XDocument.Load(Path.Combine(xmlDirectory, file, file + "Packets.xml"));
            foreach (var packet in document.Descendants(Ns + "Packet"))
            {
                var name = CleanName((string)packet.Element(Ns + "Name")!);
                var fields = packet.Element(Ns + "Fields")?.Elements(Ns + "Field")
                    .Select(f => new FieldDefinition((int)f.Element(Ns + "Index")!, (string)f.Element(Ns + "Name")!, (string)f.Element(Ns + "Type")!, (int?)f.Element(Ns + "Length")))
                    .ToDictionary(f => ParameterName(f.Name)) ?? new Dictionary<string, FieldDefinition>();
                result[name] = new PacketDefinition(name, (int?)packet.Element(Ns + "Length"), fields);
            }
        }

        return result;
    }

    // Common.xslt: the Name template and LowerCaseName.
    private static string CleanName(string name) => new(name.Where(c => !" ()/-:".Contains(c)).ToArray());

    private static string ParameterName(string fieldName)
    {
        var result = char.ToLowerInvariant(fieldName[0]) + fieldName[1..];
        return result == "class" ? "class_" : result;
    }
}

/// <summary>A parameter of a Send function (after the handle).</summary>
/// <param name="Size">For text and binary data, the field's size in bytes; null when the field ends a variable-length packet.</param>
internal sealed record SendParameter(string Name, Type Type, ParameterKind Kind, FieldDefinition? Field, int? Size);

/// <summary>A generated Send function of the desktop library.</summary>
internal sealed class SendFunction
{
    public SendFunction(MethodInfo method, string exportName, PacketDefinition packet, List<SendParameter> parameters)
    {
        this.Method = method;
        this.ExportName = exportName;
        this.Packet = packet;
        this.Parameters = parameters;
    }

    public MethodInfo Method { get; }

    /// <summary>Gets the name the client looks the function up by.</summary>
    public string ExportName { get; }

    public PacketDefinition Packet { get; }

    public List<SendParameter> Parameters { get; }
}

/// <summary>Calls the generated functions and records what they send.</summary>
internal sealed class VectorGenerator
{
    private const byte FirstFill = 0x00;
    private const byte SecondFill = 0xFF;

    private readonly Dictionary<string, PacketDefinition> _definitions;
    private readonly string _xmlDirectory;
    private readonly Random _random = new(20261022);
    private readonly StringBuilder _vectors = new();
    private readonly StringBuilder _cpp = new();
    private int _functions;
    private int _vectorCount;
    private int _sentNothing;
    private int _packetsWithUnwrittenBytes;

    public VectorGenerator(Dictionary<string, PacketDefinition> definitions, string xmlDirectory)
    {
        this._definitions = definitions;
        this._xmlDirectory = xmlDirectory;
    }

    public string Vectors => this._vectors.ToString();

    public string CppCalls => this._cpp.ToString();

    public List<string> Problems { get; } = new();

    public string Summary =>
        $"{this._functions} Send functions, {this._vectorCount} vectors, {this._sentNothing} of them send nothing; "
        + $"{this._packetsWithUnwrittenBytes} packets have bytes the functions never write";

    public void Run()
    {
        var assembly = typeof(MUnique.OpenMU.Network.Packets.ClientToServer.PingRef).Assembly;
        // The package's assembly version lags behind the package version (0.9.10.0 in 0.9.11).
        var source = $"Send functions from ClientLibrary/GenerateExtensionsDotNet.xslt on {this._xmlDirectory}, "
                     + $"MUnique.OpenMU.Network.Packets.dll {assembly.GetName().Version}";
        this._vectors.AppendLine("# " + source);
        Console.WriteLine(source);
        this._cpp.AppendLine("// Generated by tools/web/packet-test/GeneratePacketVectors.cs: the desktop library's Send functions,");
        this._cpp.AppendLine("// with their parameters in its order. Included by packet_test.cpp.");
        this._cpp.AppendLine();

        var entries = new List<string>();
        foreach (var function in this.FindFunctions())
        {
            this._functions++;
            this.WriteCppCalls(function, entries);
            this.WriteVectors(function);
        }

        this._cpp.AppendLine("const PacketCalls AllPacketCalls[] = {");
        entries.ForEach(e => this._cpp.AppendLine(e));
        this._cpp.AppendLine("};");
    }

    private IEnumerable<SendFunction> FindFunctions()
    {
        var methods = typeof(ConnectionManager).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Select(m => (Method: m, Export: m.GetCustomAttribute<UnmanagedCallersOnlyAttribute>()?.EntryPoint))
            .Where(m => m.Export is not null)
            .OrderBy(m => m.Method.MetadataToken);
        foreach (var (method, export) in methods)
        {
            var packetName = method.Name["Send".Length..];
            if (!this._definitions.TryGetValue(packetName, out var packet))
            {
                this.Problems.Add($"{method.Name}: no packet definition {packetName}");
                continue;
            }

            var parameters = method.GetParameters().Skip(1).ToList(); // the handle
            var result = new List<SendParameter>();
            for (var i = 0; i < parameters.Count; i++)
            {
                result.Add(this.Describe(method.Name, parameters[i], i > 0 ? parameters[i - 1] : null, packet));
            }

            yield return new SendFunction(method, export!, packet, result);
        }
    }

    private SendParameter Describe(string function, ParameterInfo parameter, ParameterInfo? previous, PacketDefinition packet)
    {
        var name = parameter.Name!;
        var type = parameter.ParameterType;
        if (previous?.ParameterType == typeof(byte*) && name == previous.Name + "ByteLength" && type == typeof(uint))
        {
            var data = packet.FieldsByParameter[previous.Name!];
            return new SendParameter(name, type, ParameterKind.BinaryLength, data, FieldSize(data, packet));
        }

        packet.FieldsByParameter.TryGetValue(name, out var field);
        if (field is null)
        {
            this.Problems.Add($"{function}: parameter {name} matches no field");
        }

        var kind = type switch
        {
            _ when type.IsEnum => ParameterKind.Enum,
            _ when type == typeof(byte) => field?.Type == "Boolean" ? ParameterKind.Boolean : ParameterKind.Byte,
            _ when type == typeof(ushort) => ParameterKind.UInt16,
            _ when type == typeof(uint) => ParameterKind.UInt32,
            _ when type == typeof(ulong) => ParameterKind.UInt64,
            _ when type == typeof(float) => ParameterKind.Float,
            _ when type == typeof(double) => ParameterKind.Double,
            _ when type == typeof(IntPtr) => ParameterKind.String,
            _ when type == typeof(byte*) => ParameterKind.BinaryData,
            _ => throw new NotSupportedException($"{function}: parameter {name} has type {type}"),
        };
        if (type.IsEnum && Enum.GetUnderlyingType(type) != typeof(int))
        {
            this.Problems.Add($"{function}: enum parameter {name} isn't an int enum");
        }

        return new SendParameter(name, type, kind, field, field is null ? null : FieldSize(field, packet));
    }

    // A text or binary field without a length takes the rest of a fixed-length packet (OpenMU's Ref
    // structs slice it to the end), or ends a variable-length one.
    private static int? FieldSize(FieldDefinition field, PacketDefinition packet)
    {
        return field.Length ?? packet.Length - field.Index;
    }

    private void WriteCppCalls(SendFunction function, List<string> entries)
    {
        var name = function.Method.Name["Send".Length..];
        var arguments = string.Join(", ", function.Parameters.Select((p, i) => CppArgument(p.Kind, i)));
        var types = string.Join(", ", new[] { "std::int32_t" }.Concat(function.Parameters.Select(p => CppType(p.Kind))));
        this._cpp.AppendLine($"Bytes Build_{name}([[maybe_unused]] const Args& a)");
        this._cpp.AppendLine("{");
        this._cpp.AppendLine($"    return mu::web::packets::Build{name}({arguments});");
        this._cpp.AppendLine("}");
        this._cpp.AppendLine();
        this._cpp.AppendLine($"void Send_{name}(void* function, std::int32_t handle, [[maybe_unused]] const Args& a)");
        this._cpp.AppendLine("{");
        this._cpp.AppendLine($"    reinterpret_cast<void (*)({types})>(function)(handle{(arguments.Length > 0 ? ", " : string.Empty)}{arguments});");
        this._cpp.AppendLine("}");
        this._cpp.AppendLine();
        entries.Add($"    {{\"{function.ExportName}\", {function.Parameters.Count}, &Build_{name}, &Send_{name}}},");
    }

    private static string CppType(ParameterKind kind) => kind switch
    {
        ParameterKind.Byte or ParameterKind.Boolean => "std::uint8_t",
        ParameterKind.UInt16 => "std::uint16_t",
        ParameterKind.UInt32 or ParameterKind.Enum or ParameterKind.BinaryLength => "std::uint32_t",
        ParameterKind.UInt64 => "std::uint64_t",
        ParameterKind.Float => "float",
        ParameterKind.Double => "double",
        ParameterKind.String => "const char16_t*",
        ParameterKind.BinaryData => "const std::uint8_t*",
        _ => throw new ArgumentOutOfRangeException(nameof(kind)),
    };

    private static string CppArgument(ParameterKind kind, int index) => kind switch
    {
        ParameterKind.Byte or ParameterKind.Boolean => $"a.U8({index})",
        ParameterKind.UInt16 => $"a.U16({index})",
        ParameterKind.UInt32 or ParameterKind.Enum or ParameterKind.BinaryLength => $"a.U32({index})",
        ParameterKind.UInt64 => $"a.U64({index})",
        ParameterKind.Float => $"a.F32({index})",
        ParameterKind.Double => $"a.F64({index})",
        ParameterKind.String => $"a.Str({index})",
        ParameterKind.BinaryData => $"a.Bin({index})",
        _ => throw new ArgumentOutOfRangeException(nameof(kind)),
    };

    private void WriteVectors(SendFunction function)
    {
        var invoker = SendInvoker.Create(function.Method);
        var values = new ArgumentValues(this._random);
        var unwritten = new SortedSet<int>();
        foreach (var set in values.Sets(function))
        {
            this._vectorCount++;
            this._vectors.Append("send ").Append(function.ExportName);
            foreach (var argument in set)
            {
                this._vectors.Append(' ').Append(ArgumentValues.Format(argument));
            }

            this._vectors.AppendLine();
            var first = invoker.Send(set, FirstFill);
            var second = invoker.Send(set, SecondFill);
            this.WriteResult(function, first, second, unwritten);
        }

        if (unwritten.Count > 0)
        {
            this._packetsWithUnwrittenBytes++;
            this._vectors.AppendLine($"# {function.ExportName} never writes bytes {string.Join(",", unwritten)} (zero here)");
        }
    }

    private void WriteResult(SendFunction function, SendResult first, SendResult second, SortedSet<int> unwritten)
    {
        if (first.Packet is null)
        {
            this._sentNothing++;
            this._vectors.AppendLine("none " + first.Error);
            if (second.Packet is not null)
            {
                this.Problems.Add($"{function.ExportName}: sends something only with a 0x{SecondFill:X2}-filled buffer");
            }

            return;
        }

        this._vectors.AppendLine("bytes " + Convert.ToHexString(first.Packet));
        if (second.Packet is null || second.Packet.Length != first.Packet.Length)
        {
            this.Problems.Add($"{function.ExportName}: the packet depends on the buffer's old content");
            return;
        }

        for (var i = 0; i < first.Packet.Length; i++)
        {
            if (first.Packet[i] != second.Packet[i])
            {
                unwritten.Add(i);
            }
        }
    }
}

/// <summary>What one call of a Send function sent: a packet, or nothing and why.</summary>
internal sealed record SendResult(byte[]? Packet, string? Error);

/// <summary>Calls a Send function the way the native client does, through its unmanaged entry point.</summary>
internal sealed class SendInvoker
{
    private readonly IntPtr _entryPoint;
    private readonly Action<IntPtr, object[]> _call;
    private readonly ParameterInfo[] _parameters;

    private SendInvoker(MethodInfo method)
    {
        this._entryPoint = method.MethodHandle.GetFunctionPointer();
        this._parameters = method.GetParameters();
        this._call = CreateCall(this._parameters);
    }

    public static SendInvoker Create(MethodInfo method) => new(method);

    /// <summary>Calls the function with the arguments, the packet buffer filled with <paramref name="fill"/> first.</summary>
    public unsafe SendResult Send(object?[] arguments, byte fill)
    {
        var connection = ConnectionManager.TestConnection;
        connection.Clear();
        connection.BufferFill = fill;
        var native = new List<IntPtr>();
        try
        {
            var values = new object[this._parameters.Length];
            values[0] = ConnectionManager.TestHandle;
            for (var i = 1; i < values.Length; i++)
            {
                values[i] = this.ToNative(this._parameters[i].ParameterType, arguments[i - 1], native);
            }

            this._call(this._entryPoint, values);
        }
        finally
        {
            native.ForEach(Marshal.FreeHGlobal);
        }

        if (connection.Packets.Count > 1)
        {
            throw new InvalidOperationException("a Send function sent more than one packet");
        }

        return connection.Packets.Count == 1
            ? new SendResult(connection.Packets[0], null)
            : new SendResult(null, connection.Errors.Count > 0 ? Describe(connection.Errors[0]) : "no packet");
    }

    private static string Describe(Exception exception) =>
        exception.GetType().Name + ": " + exception.Message.Split('\n')[0].Trim();

    private object ToNative(Type type, object? argument, List<IntPtr> native)
    {
        switch (argument)
        {
            case null:
                return IntPtr.Zero;
            case string text:
                {
                    // char16_t*, null-terminated, as the client passes it.
                    var pointer = Marshal.AllocHGlobal((text.Length + 1) * sizeof(char));
                    native.Add(pointer);
                    Marshal.Copy(text.ToCharArray(), 0, pointer, text.Length);
                    Marshal.WriteInt16(pointer, text.Length * sizeof(char), 0);
                    return pointer;
                }

            case byte[] bytes:
                {
                    var pointer = Marshal.AllocHGlobal(Math.Max(bytes.Length, 1));
                    native.Add(pointer);
                    Marshal.Copy(bytes, 0, pointer, bytes.Length);
                    return pointer;
                }

            case ulong number when type.IsEnum:
                return Enum.ToObject(type, unchecked((int)(uint)number));
            case ulong number:
                return Convert.ChangeType(number, type);
            default:
                return argument;
        }
    }

    // (entry point, arguments) => calli unmanaged entry point(arguments...)
    private static Action<IntPtr, object[]> CreateCall(ParameterInfo[] parameters)
    {
        var types = parameters.Select(p => p.ParameterType).ToArray();
        var method = new DynamicMethod("Call", typeof(void), new[] { typeof(IntPtr), typeof(object[]) }, typeof(SendInvoker).Module, true);
        var il = method.GetILGenerator();
        for (var i = 0; i < types.Length; i++)
        {
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldc_I4, i);
            il.Emit(OpCodes.Ldelem_Ref);
            il.Emit(OpCodes.Unbox_Any, types[i].IsPointer ? typeof(IntPtr) : types[i]);
        }

        il.Emit(OpCodes.Ldarg_0);
        il.EmitCalli(OpCodes.Calli, CallingConvention.Winapi, typeof(void), types);
        il.Emit(OpCodes.Ret);
        return method.CreateDelegate<Action<IntPtr, object[]>>();
    }
}

/// <summary>The argument sets of the vectors: edge cases and pseudo-random values.</summary>
internal sealed class ArgumentValues
{
    private const int RandomSets = 6;
    private const int VariableFieldLength = 40; // the usual size of a variable-length field
    private static readonly uint[] EnumValues = { 0, 1, 2, 0x7F, 0xFF, 0x100, 0x1234_5678, 0xFFFF_FFFF };
    private static readonly byte[] BooleanValues = { 0, 1, 2, 0x7F, 0xFF };
    private static readonly int[] VariableBinaryLengths = { 0, 1, 2, 7, 100, 250, 300, 1000 };
    private static readonly int[] VariableStringLengths = { 0, 1, 2, 59, 200, 248, 260, 1000 };

    // Text a player might type, and UTF-16 the conversion to UTF-8 has to get right.
    private static readonly string[] SpecialStrings =
    {
        "Привет, мир", // Cyrillic, 2 bytes each (the players write Russian)
        "Ăîșț", // Romanian
        "日本語", // 3 bytes each
        "😀", // a surrogate pair, 4 bytes
        "a😀b😀",
        "\u007F\u0080߿ࠀ￿", // the UTF-8 length boundaries
        "\uD800", // a lone high surrogate
        "\uDC00", // a lone low surrogate
        "x\uD83Dy", // a high surrogate followed by no low one
        "\uDE00\uD83D", // a pair in the wrong order
        "\uD83D", // a high surrogate at the end
    };

    private readonly Random _random;

    public ArgumentValues(Random random)
    {
        this._random = random;
    }

    /// <summary>A vector argument as written to the vector file.</summary>
    public static string Format(object? argument) => argument switch
    {
        null => "z",
        string text => "s" + string.Concat(text.Select(c => ((int)c).ToString("X4"))),
        byte[] bytes => "b" + Convert.ToHexString(bytes),
        ulong number => "u" + number,
        _ => throw new NotSupportedException(argument.GetType().Name),
    };

    /// <summary>Every argument set for the function: one value per parameter after the handle.</summary>
    public IEnumerable<object?[]> Sets(SendFunction function)
    {
        var parameters = function.Parameters;
        yield return this.Build(parameters, (p, _) => this.Minimum(p));
        yield return this.Build(parameters, (p, _) => this.Maximum(p));
        for (var i = 0; i < RandomSets; i++)
        {
            yield return this.Build(parameters, (p, _) => this.RandomValue(p));
        }

        for (var index = 0; index < parameters.Count; index++)
        {
            foreach (var value in this.EdgeValues(parameters[index]))
            {
                var target = index;
                yield return this.Build(parameters, (p, i) => i == target ? value : this.RandomValue(p));
            }
        }
    }

    // Builds a set; a binary length always follows its data.
    private object?[] Build(List<SendParameter> parameters, Func<SendParameter, int, object?> value)
    {
        var result = new object?[parameters.Count];
        for (var i = 0; i < parameters.Count; i++)
        {
            result[i] = parameters[i].Kind == ParameterKind.BinaryLength
                ? (ulong)((result[i - 1] as byte[])?.Length ?? 0)
                : value(parameters[i], i);
        }

        return result;
    }

    private object? Minimum(SendParameter parameter) => parameter.Kind switch
    {
        ParameterKind.String => string.Empty,
        ParameterKind.BinaryData => Array.Empty<byte>(),
        _ => 0UL,
    };

    private object? Maximum(SendParameter parameter) => parameter.Kind switch
    {
        ParameterKind.Byte => (ulong)byte.MaxValue,
        ParameterKind.Boolean => 1UL,
        ParameterKind.UInt16 => (ulong)ushort.MaxValue,
        ParameterKind.UInt32 => (ulong)uint.MaxValue,
        ParameterKind.UInt64 => ulong.MaxValue,
        ParameterKind.Enum => (ulong)byte.MaxValue,
        ParameterKind.Float or ParameterKind.Double => 0UL,
        ParameterKind.String => new string('Z', FieldLength(parameter) ?? VariableFieldLength),
        ParameterKind.BinaryData => Enumerable.Repeat((byte)0xFF, FieldLength(parameter) ?? VariableFieldLength).ToArray(),
        _ => throw new ArgumentOutOfRangeException(nameof(parameter)),
    };

    private object? RandomValue(SendParameter parameter) => parameter.Kind switch
    {
        ParameterKind.Byte or ParameterKind.Enum => (ulong)this._random.Next(256),
        ParameterKind.Boolean => (ulong)this._random.Next(2),
        ParameterKind.UInt16 => (ulong)this._random.Next(65536),
        ParameterKind.UInt32 => (ulong)(uint)this._random.NextInt64(),
        ParameterKind.UInt64 => (ulong)this._random.NextInt64() ^ ((ulong)this._random.Next() << 40),
        ParameterKind.Float or ParameterKind.Double => 0UL,
        ParameterKind.String => this.RandomString(this._random.Next((FieldLength(parameter) ?? VariableFieldLength) + 1)),
        ParameterKind.BinaryData => this.RandomBytes(FieldLength(parameter) is { } length ? this._random.Next(length + 1) : this._random.Next(VariableFieldLength)),
        _ => throw new ArgumentOutOfRangeException(nameof(parameter)),
    };

    private IEnumerable<object?> EdgeValues(SendParameter parameter)
    {
        switch (parameter.Kind)
        {
            case ParameterKind.Byte:
                return parameter.Field?.Length is not null ? new object?[] { 0UL, 0x0FUL, 0x10UL, 0xFFUL } : Array.Empty<object?>();
            case ParameterKind.Boolean:
                return BooleanValues.Select(v => (object?)(ulong)v);
            case ParameterKind.Enum:
                return EnumValues.Select(v => (object?)(ulong)v);
            case ParameterKind.String:
                return this.EdgeStrings(FieldLength(parameter));
            case ParameterKind.BinaryData:
                return this.EdgeBinaries(FieldLength(parameter));
            default:
                return Array.Empty<object?>();
        }
    }

    private IEnumerable<object?> EdgeStrings(int? fieldLength)
    {
        var lengths = fieldLength is { } size
            ? new[] { 0, 1, size - 1, size, size + 1, size + 5 }
            : VariableStringLengths;
        foreach (var length in lengths.Where(l => l >= 0).Distinct())
        {
            yield return new string((char)('a' + (length % 26)), length);
        }

        foreach (var text in SpecialStrings)
        {
            yield return text;
        }

        if (fieldLength is { } field && field > 3)
        {
            // A 2- or 4-byte character across the end of the field.
            yield return new string('k', field - 1) + "я";
            yield return new string('k', field - 2) + "я";
            yield return new string('k', field - 3) + "😀";
            yield return new string('k', field - 4) + "😀";
        }
    }

    private IEnumerable<object?> EdgeBinaries(int? fieldLength)
    {
        var lengths = fieldLength is { } size
            ? new[] { 0, 1, size - 1, size, size + 1 }
            : VariableBinaryLengths;
        foreach (var length in lengths.Where(l => l >= 0).Distinct())
        {
            yield return this.RandomBytes(length);
        }

        yield return null; // a null pointer, length 0
    }

    private static int? FieldLength(SendParameter parameter) => parameter.Size;

    private string RandomString(int length)
    {
        var builder = new StringBuilder(length);
        for (var i = 0; i < length; i++)
        {
            builder.Append((char)this._random.Next(0x20, 0x7F));
        }

        return builder.ToString();
    }

    private byte[] RandomBytes(int length)
    {
        var bytes = new byte[length];
        this._random.NextBytes(bytes);
        return bytes;
    }
}
