"""PCM .wav -> IMA ADPCM .wav (WAVE_FORMAT_DVI_ADPCM), for the web client's packs.

The game's sounds are plain PCM: the startup pack's 207 take 35 MB, 28 MB even gzipped. IMA ADPCM keeps
4 bits a sample (a quarter of 16-bit PCM), and miniaudio's wav decoder (dr_wav) reads it under the same
file name, so nothing in the game changes; decoding it costs a few operations a sample.

ADPCM follows a sudden loud start slowly: a click or a coin drop comes out noticeably rough (13-18 dB
signal to noise, 2026-11-02). encode() says how clean the result is, and make_startup_pack.py keeps a
sound as it is when it falls below MIN_SNR_DB.

    python -I tools/web/ima_adpcm.py <in.wav> <out.wav>    # one file: its sizes and signal to noise
"""
import math
import struct
import sys

STEPS = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
    97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
    4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX_STEP = [-1, -1, -1, -1, 2, 4, 6, 8]
LAST_INDEX = len(STEPS) - 1
WAVE_FORMAT_PCM = 1
WAVE_FORMAT_DVI_ADPCM = 0x11
# A sound cleaner than this (signal to noise over the whole file) is worth its quarter size.
MIN_SNR_DB = 27
# Bytes of one channel's block: a 4-byte header (the first sample, the step index) and 4-bit samples.
CHANNEL_BLOCK_BYTES = 512
FRAMES_PER_BLOCK = (CHANNEL_BLOCK_BYTES - 4) * 2 + 1
# After the headers, each channel's samples go in turns of 8 (4 bytes).
GROUP = 8


def read_pcm_wav(data):
    """(channels, rate, [[samples] per channel] as 16-bit ints) of a PCM .wav, or None for any other
    file (another format, a broken header)."""
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None
    position, fmt, samples = 12, None, None
    while position + 8 <= len(data):
        chunk, size = data[position:position + 4], struct.unpack_from("<I", data, position + 4)[0]
        body = data[position + 8:position + 8 + size]
        if chunk == b"fmt " and len(body) >= 16:
            fmt = struct.unpack_from("<HHIIHH", body)
        elif chunk == b"data":
            samples = body
        position += 8 + size + (size & 1)
    if fmt is None or samples is None:
        return None
    tag, channels, rate, _, _, bits = fmt
    if tag != WAVE_FORMAT_PCM or channels not in (1, 2) or bits not in (8, 16):
        return None
    if bits == 16:
        values = list(struct.unpack_from(f"<{len(samples) // 2}h", samples))
    else:
        values = [(value - 128) << 8 for value in samples]
    frames = len(values) // channels
    return channels, rate, [values[c:frames * channels:channels] for c in range(channels)]


def encode_channel(samples, start, count, index):
    """One channel's part of a block: samples[start] in the header, then count - 1 codes. Returns
    (header, codes, the step index after it, the squared error of the samples it stands for)."""
    predictor = samples[start]
    header = struct.pack("<hBB", predictor, index, 0)
    codes, error = [], 0
    for sample in samples[start + 1:start + count]:
        step = STEPS[index]
        difference = sample - predictor
        code = 0
        if difference < 0:
            code = 8
            difference = -difference
        change = step >> 3
        if difference >= step:
            code |= 4
            difference -= step
            change += step
        step >>= 1
        if difference >= step:
            code |= 2
            difference -= step
            change += step
        step >>= 1
        if difference >= step:
            code |= 1
            change += step
        predictor = predictor - change if code & 8 else predictor + change
        predictor = -32768 if predictor < -32768 else 32767 if predictor > 32767 else predictor
        index += INDEX_STEP[code & 7]
        index = 0 if index < 0 else LAST_INDEX if index > LAST_INDEX else index
        codes.append(code)
        error += (sample - predictor) ** 2
    return header, codes, index, error


def encode(data):
    """(the IMA ADPCM .wav of a PCM .wav's bytes, its signal to noise in dB), or None when the file is
    no PCM .wav or holds no sound."""
    parsed = read_pcm_wav(data)
    if parsed is None or not parsed[2][0]:
        return None
    channels, rate, per_channel = parsed
    frames = len(per_channel[0])
    indexes, error, body = [0] * channels, 0, bytearray()
    for start in range(0, frames, FRAMES_PER_BLOCK):
        count = min(FRAMES_PER_BLOCK, frames - start)
        codes = []
        for c in range(channels):
            header, channel_codes, indexes[c], channel_error = encode_channel(per_channel[c], start, count, indexes[c])
            body += header
            codes.append(channel_codes + [0] * (-len(channel_codes) % GROUP))
            error += channel_error
        # The last block stops after its last turn of 8: a full one would add up to 1016 frames of
        # silence, which miniaudio plays (a gap in a looped sound).
        for group in range(0, len(codes[0]), GROUP):
            for c in range(channels):
                turn = codes[c][group:group + GROUP]
                body += bytes(turn[i] | (turn[i + 1] << 4) for i in range(0, GROUP, 2))
    signal = sum(value * value for channel in per_channel for value in channel)
    snr = 10 * math.log10(signal / error) if error and signal else math.inf if signal else 0.0
    block_align = CHANNEL_BLOCK_BYTES * channels
    fmt = struct.pack("<HHIIHHHH", WAVE_FORMAT_DVI_ADPCM, channels, rate, rate * block_align // FRAMES_PER_BLOCK,
                      block_align, 4, 2, FRAMES_PER_BLOCK)
    chunks = (b"fmt " + struct.pack("<I", len(fmt)) + fmt
              + b"fact" + struct.pack("<II", 4, frames)
              + b"data" + struct.pack("<I", len(body)) + bytes(body) + b"\0" * (len(body) & 1))
    return b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks, snr


def main(argv):
    with open(argv[1], "rb") as f:
        source = f.read()
    result = encode(source)
    if result is None:
        print("not a PCM .wav")
        return 1
    encoded, snr = result
    with open(argv[2], "wb") as f:
        f.write(encoded)
    print(f"{len(source)} -> {len(encoded)} bytes, signal to noise {snr:.1f} dB"
          f"{'' if snr >= MIN_SNR_DB else f' (under {MIN_SNR_DB}: the pack keeps it as it is)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
