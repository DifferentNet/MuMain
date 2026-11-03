"""tools/web/ima_adpcm.py and its use in make_startup_pack.py: a PCM .wav comes out as an IMA ADPCM .wav
that a decoder written like dr_wav's (miniaudio) reads back to the same length and close to the original,
and the pack keeps a sound as it is when ADPCM would make it rough."""
import importlib.util
import io
import math
import struct
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True  # loading the tools leaves no __pycache__ in tools/web
ROOT = Path(sys.argv[1])
sys.path.insert(0, str(ROOT / "tools/web"))
spec = importlib.util.spec_from_file_location("make_startup_pack", ROOT / "tools/web/make_startup_pack.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
adpcm = pack.ima_adpcm


def pcm_wav(channels, rate, per_channel, bits=16):
    frames = len(per_channel[0])
    if bits == 16:
        body = struct.pack(f"<{frames * channels}h", *(per_channel[c][i] for i in range(frames) for c in range(channels)))
    else:
        body = bytes((per_channel[c][i] >> 8) + 128 for i in range(frames) for c in range(channels))
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * channels * bits // 8, channels * bits // 8, bits)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(body)) + body
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks


def decode_like_dr_wav(data):
    """IMA ADPCM as dr_wav decodes it: per block, each channel's header sample, then its codes in
    turns of 8 a channel, low nibble first; every byte of the data chunk is read."""
    fmt_at = data.index(b"fmt ") + 8
    tag, channels, _, _, block_align, bits = struct.unpack_from("<HHIIHH", data, fmt_at)
    assert tag == adpcm.WAVE_FORMAT_DVI_ADPCM and bits == 4
    body_at = data.index(b"data") + 8
    body = data[body_at:body_at + struct.unpack_from("<I", data, body_at - 4)[0]]
    out = [[] for _ in range(channels)]
    for block_start in range(0, len(body), block_align):
        block = body[block_start:block_start + block_align]
        state = []
        for c in range(channels):
            predictor, index, _ = struct.unpack_from("<hBB", block, c * 4)
            out[c].append(predictor)
            state.append([predictor, index])
        for position in range(channels * 4, len(block), 4 * channels):
            for c in range(channels):
                for byte in block[position + 4 * c:position + 4 * c + 4]:
                    for code in (byte & 15, byte >> 4):
                        predictor, index = state[c]
                        step = adpcm.STEPS[index]
                        change = step >> 3
                        change += step if code & 4 else 0
                        change += step >> 1 if code & 2 else 0
                        change += step >> 2 if code & 1 else 0
                        predictor = max(-32768, min(32767, predictor - change if code & 8 else predictor + change))
                        state[c] = [predictor, max(0, min(88, index + adpcm.INDEX_STEP[code & 7]))]
                        out[c].append(predictor)
    return out


def snr(original, decoded):
    signal = sum(v * v for channel in original for v in channel)
    noise = sum((a - b) ** 2 for co, cd in zip(original, decoded) for a, b in zip(co, cd))
    return 10 * math.log10(signal / noise)


def tone(frames, rate, frequency, volume):
    return [int(volume * math.sin(2 * math.pi * frequency * i / rate)) for i in range(frames)]


# A stereo tone across several blocks and a short last one: as long as the original but for under a
# turn of 8, as clean as encode() says, and above the pack's limit.
for channels, frames, bits in ((1, 5000, 16), (2, 3 * adpcm.FRAMES_PER_BLOCK + 10, 16), (1, 777, 8), (2, 1, 16)):
    per_channel = [tone(frames, 22050, 200 + 50 * c, 8000) for c in range(channels)]
    source = pcm_wav(channels, 22050, per_channel, bits)
    original = adpcm.read_pcm_wav(source)[2]
    encoded, reported = adpcm.encode(source)
    decoded = decode_like_dr_wav(encoded)
    assert 0 <= len(decoded[0]) - frames < adpcm.GROUP, (channels, frames, len(decoded[0]))
    assert struct.unpack_from("<I", encoded, encoded.index(b"fact") + 8)[0] == frames
    if frames > 1:
        measured = snr(original, [channel[:frames] for channel in decoded])
        assert abs(measured - reported) < 0.01, (measured, reported)
        assert measured > adpcm.MIN_SNR_DB, measured
    assert len(encoded) < len(source) // (2 if bits == 8 else 3) + 200, (len(encoded), len(source))

# Not a PCM .wav, or empty: nothing.
assert adpcm.encode(b"OggS" + b"\0" * 60) is None
assert adpcm.encode(pcm_wav(1, 22050, [[]])) is None

with tempfile.TemporaryDirectory() as folder:
    pack.sound_cache_dir = str(Path(folder, "cache"))
    clean = Path(folder, "aWind.wav")
    clean.write_bytes(pcm_wav(1, 22050, [tone(8000, 22050, 300, 9000)]))
    # A click: silence, then full volume at once, which ADPCM follows too slowly.
    click = Path(folder, "iButtonClick.wav")
    click.write_bytes(pcm_wav(1, 22050, [[0] * 400 + [30000 if i % 6 < 3 else -30000 for i in range(300)] + [0] * 400]))
    for _ in range(2):  # the second time from the cache
        out = io.BytesIO()
        pack.copy_packed(str(clean), out)
        assert out.getvalue()[20:22] == struct.pack("<H", adpcm.WAVE_FORMAT_DVI_ADPCM)
        assert pack.packed_size(str(clean)) == len(out.getvalue()) < clean.stat().st_size // 3
        out = io.BytesIO()
        pack.copy_packed(str(click), out)
        assert out.getvalue() == click.read_bytes()
        assert pack.packed_size(str(click)) == click.stat().st_size
    assert len(list(Path(folder, "cache").iterdir())) == 2

print("ima_adpcm: OK")
