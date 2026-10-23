# Packet function test

Checks the web build's generated packet functions (`tools/web/gen_web_packets.py`:
`Build<Packet>` in `WebPacketBuilders.generated.h`, the `Send<Packet>` exports in
`WebPacketFunctions.generated.cpp`, and the helpers in `src/source/Core/Platform/web/WebPacketWriting.h`)
byte for byte against the desktop client's own `Send<Packet>` functions in `MUnique.Client.Library`.

```bash
tools/web/packet-test/run.sh
```

It needs Python, the .NET SDK, Emscripten's `em++` and Node on PATH (`scripts/build-web.sh` in MU
Moldova shows how to set up emsdk in Git Bash). `PACKET_TEST_OUT` picks the scratch folder,
`OPENMU_PACKETS_VERSION` the packet definitions (default 0.9.11, as in `src/CMakeLists.txt` and
`ClientLibrary`). NuGet packages come from `client-src/.nuget`, like the CMake build's
(`NUGET_PACKAGES` overrides it); nothing is downloaded when the client has been built once.

1. `gen_web_packets.py` writes the web functions into the scratch folder, from the same XML and
   bindings as the CMake build.
2. `PacketVectorGen.csproj` runs `ClientLibrary/GenerateExtensionsDotNet.xslt` on OpenMU's packet
   definitions, as `ClientLibrary/MUnique.Client.Library.csproj` does, and compiles the resulting
   Send functions with `ClientLibrary/NativeInterop.cs` and `CapturingConnection.cs`, which keeps the
   packets instead of sending them. `GeneratePacketVectors.cs` reads each function's parameters from
   the compiled function, calls it through its unmanaged entry point with edge-case and pseudo-random
   arguments, and writes the arguments and the packet to `vectors.txt`. It also writes
   `packet_calls.generated.inc`: for each function, a call of `Build<Packet>` and one of the export
   through a pointer of the desktop function's type, with the arguments in the desktop library's
   order. So a parameter the web functions take in another order shows up, as a compile error or as
   wrong bytes.
3. `packet_test.cpp`, built with `em++` (and AddressSanitizer) and run in Node, makes both calls for
   every vector and prints every difference. The exports are found by name in
   `mu::web::GeneratedPacketFunctions()`, as the client finds them; every export needs vectors.

The arguments cover every parameter type: bytes and bit fields, Booleans as 0, 1 and other values,
enums up to 32 bits, 16/32-bit integers of both byte orders, text (empty, one character, the field's
length and one more, Cyrillic, 3- and 4-byte UTF-8, lone and reversed surrogates, characters across
the end of the field, variable-length text past 255 bytes in C1 packets) and binary data (empty,
null, the field's length and one more, long variable-length data).

Vector format, one line each:

| Line | Meaning |
| --- | --- |
| `send <export> <argument> ...` | one argument per parameter after the handle: `u<decimal>` a number (enums as their 32-bit pattern), `s<UTF-16 code units, 4 hex digits each>` text, `b<hex>` binary data, `z` a null pointer |
| `bytes <hex>` | the packet the desktop library sends for them |
| `none <reason>` | it sends nothing: OpenMU threw, and the Send function caught it |
| `# ...` | a comment, e.g. the bytes a function never writes |

Known differences, not counted as failures:

- Text or binary data longer than its fixed-size field: OpenMU throws and the desktop library sends
  nothing (`none`); the web functions cut it off (see `WebPacketWriting.h`). The cut can end in the
  middle of a UTF-8 character.
- Bytes the desktop functions never write (padding, and the rest of a fixed binary field after
  shorter data) hold whatever was in the pipe's buffer; the web functions send zeros, and the test
  expects zeros. The generator fills the buffer with 0x00 and 0xFF to find them, and lists them as
  comments in `vectors.txt`.

The hand-written sends of `WebNetwork.cpp` (login, chat server authentication and messages) aren't
covered.

Last run (2026-10-23 on the deploy 11 client, MUnique.OpenMU.Network.Packets 0.9.11, the same as on
2026-10-22): 208 packets, 3,016 vectors, each through `Build<Packet>` and the export; 244 vectors
where the desktop library sends nothing; 0 failures. The first run found three bugs, fixed in `gen_web_packets.py` and `WebPacketWriting.h`:
a text field without a length in a fixed-size packet (DeleteCharacter's security code) overflowed
the packet buffer; Booleans other than 0 and 1 were sent as true (the desktop library sends true
only for 1); and lone surrogates became invalid UTF-8 instead of U+FFFD.
