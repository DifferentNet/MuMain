"""tools/web/make_startup_pack.py: the web packs hold version 12 models decrypted, byte for byte what the
client's own MapFileDecrypt gives, and every other file as it is."""
import gzip
import importlib.util
import io
import json
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

# The sounds and music are parts of their own, however small, which the page doesn't wait for.
parts = pack.startup_parts([["Data/Sound/a.wav", 10], ["Data/Music/b.mp3", 10], ["fonts/c.ttf", 10],
                            ["Data/Logo/d.ozt", 10]])
assert parts == {"startup-data-sound": [["Data/Sound/a.wav", 10]], "startup-data-music": [["Data/Music/b.mp3", 10]],
                 "startup-misc": [["fonts/c.ttf", 10], ["Data/Logo/d.ozt", 10]]}, parts
assert pack.is_deferred("Data/Sound/a.wav") and pack.is_deferred("Data/Music/b.mp3")
assert not pack.is_deferred("Data/Logo/d.ozt") and not pack.is_deferred("Data/SoundX/e.wav")

# The Modern theme is a part of its own, however small, which phones and tablets leave out; the files
# the theme list reads stay with the interface.
modern = "Data/Interface/RmlUi/themes/modern/"
parts = pack.startup_parts([[modern + "theme.ini", 10], [modern + "login.rml", 10], [modern + "img/a.png", 10]])
assert parts == {pack.DESKTOP_PART: [[modern + "login.rml", 10], [modern + "img/a.png", 10]],
                 "startup-misc": [[modern + "theme.ini", 10]]}, parts
assert not pack.is_desktop_only(modern + "base.rcss") and not pack.is_desktop_only(modern + "tokens.ini")

# startup.json's index marks that part "desktop" (index.html leaves it out on a phone), also when its
# pack is kept from the last build; no other part carries the mark.
with tempfile.TemporaryDirectory() as folder:
    Path(folder, pack.PACKS).mkdir()
    contents = {modern + "theme.ini": b"[theme]\n", modern + "login.rml": b"<rml></rml>", "fonts/c.ttf": b"font"}
    for path, data in contents.items():
        Path(folder, path).parent.mkdir(parents=True, exist_ok=True)
        Path(folder, path).write_bytes(data)
    listed = [[path, pack.packed_size(str(Path(folder, path)))] for path in contents]
    desktop_name = f"{pack.PACKS}/{pack.DESKTOP_PART}.pack"
    previous_index = None
    for build in ("first", "again"):
        entries, written = pack.write_startup_parts(folder, listed, previous_index)
        index = json.loads(json.dumps({"parts": entries}))  # as main() writes startup.json
        assert written == (2 if build == "first" else 0), (build, written)
        assert [part["name"] for part in index["parts"] if part.get("desktop") is True] == [desktop_name], index
        assert all("desktop" not in part for part in index["parts"] if part["name"] != desktop_name), index
        previous_index = index
    desktop_part = next(part for part in index["parts"] if part["name"] == desktop_name)
    assert desktop_part["files"] == [[modern + "login.rml", len(contents[modern + "login.rml"])]], desktop_part
    unpacked = gzip.decompress(Path(folder, pack.PACKS, pack.DESKTOP_PART + ".pack.gz").read_bytes())
    assert unpacked == contents[modern + "login.rml"]

print("startup pack models: ok")
