"""Integration checks with the real encoder; no Bluetooth access required."""
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
exe = str(Path(sys.argv[1]).resolve())
def run(source, output, quality="hq", success=True):
    p = subprocess.run([exe, str(source), str(output), quality], capture_output=True)
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
    before=source.read_bytes()
    run(source,source,success=False)
    assert source.read_bytes()==before
    existing=root/"existing.ldacdiag"
    existing.write_bytes(b"preserve")
    run(source,existing,success=False)
    assert existing.read_bytes()==b"preserve"
    print("PASS: HQ/SQ/MQ encoding; records; partial/empty input; malformed input; output protection.")
    print("One-second diagnostic sizes (HQ, SQ, MQ):",sizes)
