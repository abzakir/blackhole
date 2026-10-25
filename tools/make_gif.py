"""Turns a folder of recorded frames into a GIF for the README.

Record frames with any step's --record mode first, for example:

    build/04_rk4.exe --record build/frames/rk4 --frame 400 --every 4
    python tools/make_gif.py build/frames/rk4 docs/gifs/rk4.gif --width 640 --fps 25
"""

import argparse
from pathlib import Path

from PIL import Image


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("frames", type=Path, help="folder with frame_0000.png, frame_0001.png, ...")
    parser.add_argument("output", type=Path, help="GIF file to write")
    parser.add_argument("--width", type=int, default=640, help="output width in pixels (default 640)")
    parser.add_argument("--fps", type=float, default=25.0, help="playback speed (default 25)")
    parser.add_argument("--skip", type=int, default=1, help="use every N-th frame (default 1: all)")
    parser.add_argument("--no-dither", action="store_true",
                        help="no dithering: much smaller files for starry frames, slight colour banding")
    args = parser.parse_args()

    paths = sorted(args.frames.glob("frame_*.png"))[:: max(1, args.skip)]
    if not paths:
        raise SystemExit(f"no frame_*.png files in {args.frames}")

    frames = []
    for path in paths:
        image = Image.open(path).convert("RGB")
        height = round(image.height * args.width / image.width)
        frames.append(image.resize((args.width, height), Image.Resampling.LANCZOS))

    # A GIF frame can only hold 256 colours. Build one shared palette from a strip of
    # sample frames, so colours don't flicker from one frame to the next.
    width, height = frames[0].size
    samples = frames[:: max(1, len(frames) // 8)][:8]
    strip = Image.new("RGB", (width, height * len(samples)))
    for i, sample in enumerate(samples):
        strip.paste(sample, (0, i * height))
    palette = strip.quantize(colors=256, method=Image.Quantize.MEDIANCUT)

    dither = Image.Dither.NONE if args.no_dither else Image.Dither.FLOYDSTEINBERG
    gif = [frame.quantize(palette=palette, dither=dither) for frame in frames]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    gif[0].save(args.output, save_all=True, append_images=gif[1:], duration=round(1000 / args.fps),
                loop=0, optimize=True)
    size_mb = args.output.stat().st_size / 1e6
    print(f"{args.output}: {len(gif)} frames, {width}x{height}, {size_mb:.1f} MB")


if __name__ == "__main__":
    main()
