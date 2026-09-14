# Generate raw 48 kHz stereo s16le PCM for Phase 1 file tests.
import argparse, math, struct, sys

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--seconds', type=float, default=2.0)
    p.add_argument('--freq', type=float, default=440.0)
    p.add_argument('--out', required=True)
    args = p.parse_args()
    n = int(48000 * args.seconds)
    with open(args.out, 'wb') as f:
        for i in range(n):
            x = int(8000 * math.sin(2 * math.pi * args.freq * i / 48000))
            f.write(struct.pack('<hh', x, x))
    print(f'Wrote {args.out} ({n} frames)', file=sys.stderr)

if __name__ == '__main__':
    main()
