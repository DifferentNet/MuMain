"""tools/web/make_startup_pack.py: the web packs hold version 12 models decrypted, byte for byte what the
client's own MapFileDecrypt gives, and every other file as it is."""
import gzip
import importlib.util
import io
import random
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True  # loading the tool leaves no __pycache__ in tools/web
ROOT = Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("make_startup_pack", ROOT / "tools/web/make_startup_pack.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


def decrypt_like_the_client(data):
    """ZzzLodTerrain.h MapFileDecrypt, a byte at a time."""
    out, key = bytearray(), pack.MAP_FIRST_KEY
    for i, value in enumerate(data):
        out.append(((value ^ pack.MAP_XOR_KEY[i % 16]) - key) % 256)
        key = (value + pack.MAP_KEY_STEP) % 256
    return bytes(out)


def encrypt_like_the_client(data):
    """ZzzLodTerrain.h MapFileEncrypt, a byte at a time."""
    out, key = bytearray(), pack.MAP_FIRST_KEY
    for i, value in enumerate(data):
        out.append(((value + key) % 256) ^ pack.MAP_XOR_KEY[i % 16])
        key = (out[i] + pack.MAP_KEY_STEP) % 256
    return bytes(out)


rng = random.Random(12)
for length in (0, 1, 15, 16, 17, 4099):
    data = bytes(rng.randrange(256) for _ in range(length))
    assert pack.decrypt_map_file(data) == decrypt_like_the_client(data), length
extremes = b"\x00\xff\x80\x7f" * 64
assert pack.decrypt_map_file(extremes) == decrypt_like_the_client(extremes)

with tempfile.TemporaryDirectory() as folder:
    body = b"model.smd".ljust(32, b"\0") + bytes(rng.randrange(256) for _ in range(500))
    encrypted = Path(folder, "Player.bmd")
    encrypted.write_bytes(b"BMD\x0c" + len(body).to_bytes(4, "little") + encrypt_like_the_client(body))
    plain = Path(folder, "Plain.bmd")
    plain.write_bytes(b"BMD\x0a" + body)
    # A data table with the same suffix and no model header (Data/Local/Item.bmd).
    table = Path(folder, "Item.bmd")
    table.write_bytes(b"\x12\x34" * 40)
    for path, expected in ((encrypted, b"BMD\x0a" + body), (plain, plain.read_bytes()), (table, table.read_bytes())):
        out = io.BytesIO()
        pack.copy_packed(str(path), out)
        assert out.getvalue() == expected, path.name
        assert pack.packed_size(str(path)) == len(expected), path.name

    # A pack's index sizes cut its unpacked bytes back into the files (WebFileSystem.cpp).
    Path(folder, pack.PACKS).mkdir()
    files = [[name, pack.packed_size(str(Path(folder, name)))] for name in ("Player.bmd", "Item.bmd")]
    entry, written = pack.write_pack(folder, "test", files, None)
    assert written and entry["files"] == files
    unpacked = gzip.decompress(Path(folder, pack.PACKS, "test.pack.gz").read_bytes())
    assert unpacked == b"BMD\x0a" + body + table.read_bytes()

print("startup pack models: ok")
