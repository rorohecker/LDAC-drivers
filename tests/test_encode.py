"""Integration checks with the real encoder; no Bluetooth access required."""
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
if not __debug__:
    raise RuntimeError("Run encoder tests without Python optimization so assertions execute.")
exe = str(Path(sys.argv[1]).resolve())
help_result = subprocess.run([exe, "--help"], capture_output=True, text=True)
assert help_result.returncode == 0 and "packed 3-byte" in help_result.stdout
def run(source, output, quality="hq", success=True, options=()):
    p = subprocess.run([exe, str(source), str(output), quality, *options], capture_output=True)
    assert (p.returncode == 0) == success, p.stderr.decode()
def records(path):
    data = path.read_bytes()
    offset = count = 0
    while offset < len(data):
        assert len(data) - offset >= 4
        size, frames = struct.unpack_from("<HH", data, offset)
        offset += 4
        assert 0 < size <= 679 and frames > 0
        assert len(data) - offset >= size
        offset += size
        count += frames
    return count
with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    source = root / "tone.pcm"
    source.write_bytes(b"".join(struct.pack("<hh", *(2*[int(8000*math.sin(
        2*math.pi*440*i/48000))])) for i in range(48000)))
    sizes=[]
    for quality in ["hq", "sq", "mq"]:
        out=root/(quality+".ldacdiag")
        run(source,out,quality)
        assert records(out)>=375
        sizes.append(out.stat().st_size)
    assert sizes[0]>sizes[1]>sizes[2],sizes

    # Exercise the sample-rate and PCM-depth combinations from libldac's API.
    # These records verify offline encoding only; no headset negotiation is implied.
    rate_payloads={}
    for rate in (44100, 48000, 88200, 96000):
        depth_payloads=[]
        for bits in (16, 24, 32):
            width=bits//8
            value=(1 << (bits-4)) - 1
            frame=value.to_bytes(width,"little",signed=True)*2
            negative_frame=(-value).to_bytes(width,"little",signed=True)*2
            matrix_input=root/f"matrix-{rate}-{bits}.pcm"
            matrix_input.write_bytes(frame*1024+negative_frame*1024)
            matrix_output=root/f"matrix-{rate}-{bits}.ldacdiag"
            run(matrix_input,matrix_output,"sq",options=("--rate",str(rate),"--format",f"s{bits}"))
            assert records(matrix_output)==(17 if rate<=48000 else 9),(rate,bits)
            depth_payloads.append(matrix_output.read_bytes())
        # The three PCM depths must produce the same packet count and bitrate
        # mode. Their payloads can differ at the lowest quantization bits.
        assert len({len(data) for data in depth_payloads})==1,rate
        rate_payloads[rate]=depth_payloads[0]
    assert len(set(rate_payloads.values()))==4
    for length in [0,4,508,512,516]:
        short=root/"short.pcm"
        short.write_bytes(bytes(length))
        out=root/f"short-{length}.ldacdiag"
        run(short,out)
        records(out)
    invalid=root/"bad.pcm"
    invalid.write_bytes(b"123")
    run(invalid,root/"bad.ldacdiag",success=False)
    assert not (root/"bad.ldacdiag").exists()
    run(source,root/"bad-quality.ldacdiag","invalid",success=False)
    run(source,root/"bad-rate.ldacdiag",options=("--rate","32000"),success=False)
    run(source,root/"bad-rate-format.ldacdiag",options=("--rate","+48000"),success=False)
    run(source,root/"bad-format.ldacdiag",options=("--format","f32"),success=False)
    run(source,root/"duplicate-rate.ldacdiag",options=("--rate","48000","--rate","96000"),success=False)
    short_24=root/"bad-s24.pcm"
    short_24.write_bytes(bytes(5))
    run(short_24,root/"bad-s24.ldacdiag",options=("--format","s24"),success=False)
    before=source.read_bytes()
    run(source,source,success=False)
    assert source.read_bytes()==before
    existing=root/"existing.ldacdiag"
    existing.write_bytes(b"preserve")
    run(source,existing,success=False)
    assert existing.read_bytes()==b"preserve"
    print("PASS: HQ/SQ/MQ encoding; 4 sample rates x 3 PCM formats; partial/empty input; malformed input; output protection.")
    print("One-second diagnostic sizes (HQ, SQ, MQ):",sizes)
