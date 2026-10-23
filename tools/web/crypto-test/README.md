# MuPacketCrypto test

Checks the web build's packet encryption (`src/source/Core/Platform/web/MuPacketCrypto.*`) byte for
byte against OpenMU's own network library, the one the desktop client's C# ClientLibrary uses.

```bash
tools/web/crypto-test/run.sh                 # against MUnique.OpenMU.Network 0.9.10 (NuGet) and,
                                             # in the MU Moldova checkout, ../server/addons/ref (0.9.12)
tools/web/crypto-test/run.sh <DLL folder>    # against another OpenMU's DLLs
```

It needs the .NET SDK, Emscripten's `em++` and Node on PATH (`scripts/build-web.sh` in MU Moldova
shows how to set up emsdk in Git Bash). `CRYPTO_TEST_OUT` picks the scratch folder.

1. `GenerateVectors.cs` (built through `VectorGen.csproj`, once per OpenMU version) drives OpenMU's
   classes the way the ClientLibrary does: Xor32 then SimpleModulus out, SimpleModulus in, for
   encrypted connections; split only for the connect server. It records random and edge-case packets
   (C1-C4, all sizes, packets split across and joined in reads, broken headers, broken checksums)
   and what OpenMU did with them.
2. `crypto_test.cpp`, built with `em++` and run in Node, replays them through MuPacketCrypto and
   prints every difference.

Vector format, one line each, bytes in hex:

| Line | Meaning |
| --- | --- |
| `encrypt <name>` | a new client connection |
| `send <plain packet>` | a packet as passed to `ConnectionManager_Send` |
| `wire <bytes>` | everything the client wrote to the socket for the sends above |
| `session <name> <encrypted\|plain>` | a new receiving connection; plain is the connect server's |
| `feed <bytes>` | bytes received from the server |
| `packet <bytes>` | a packet handed to the client after that feed |
| `end <ok\|error>` | error: the client disconnected (malformed stream) |

Last run (2026-10-22): 1,049 sent and 4,601 received packets per version, 0 failures, for 0.9.10
and 0.9.12.
