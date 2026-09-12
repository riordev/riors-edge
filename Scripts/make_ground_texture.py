# The composer slab's ground grain — AUTHORED HERE, not painted (O279).
#
#   python Scripts/make_ground_texture.py           writes Assets/textures/ground_concrete.png
#   python Scripts/make_ground_texture.py --earth   writes Assets/textures/ground_earth.png
#
# A slab wears a tiled ground material with a world-aligned grain, tinted by
# role. This writes the grain: one 1024x1024 8-bit grey PNG, SEAMLESS, centred
# on mid-grey so M_BreakerGround reads it as a modulation around 1.0 and the
# role tint (the material's Color parameter) carries every hue. The texture is
# grey on purpose — colour authored into the pixels would fight the tint, and
# the tint is the only thing the owner tunes.
#
# Seamless by construction, not by blending: each octave is value noise
# interpolated from a random lattice that WRAPS (index modulo the grid), so
# the right edge continues into the left and the bottom into the top with no
# seam to hide. World-aligned UVs (WorldPosition / metres) tile this across
# every slab, and a seam at that scale would be a grid the eye reads in one
# frame.
#
# Deterministic: a fixed seed, so re-running writes byte-identical files and
# the PNG in git is the script's output, not a snapshot.

import argparse
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "Assets", "textures"))

SIZE = 1024                      # O2 PLACEHOLDER — texel edge; tiles at 4 m / 14.8 m in the material
SEED = 279                       # O2 PLACEHOLDER — the ruling's number, fixed so the file is reproducible
OCTAVE_CELLS = (4, 16, 64, 256)  # O2 PLACEHOLDER — lattice cell sizes in texels, fine to coarse


def breaker_smoothstep(t):
    """Hermite ease so the lattice's cells do not read as diamonds."""
    return t * t * (3.0 - 2.0 * t)


def breaker_value_noise(rng, size, cell):
    """One octave of value noise on a lattice of `cell`-texel squares, wrapped
    so texel 0 and texel `size` interpolate from the same lattice point."""
    n = size // cell
    lattice = rng.random((n, n))
    # Texel -> lattice coordinate, then the four wrapped corners.
    coord = np.arange(size, dtype=np.float64) / cell
    i0 = np.floor(coord).astype(np.int64) % n
    i1 = (i0 + 1) % n
    t = breaker_smoothstep(coord - np.floor(coord))
    # Rows index Y, columns index X.
    ty, tx = t[:, None], t[None, :]
    y0, y1 = i0[:, None], i1[:, None]
    x0, x1 = i0[None, :], i1[None, :]
    top = lattice[y0, x0] * (1.0 - tx) + lattice[y0, x1] * tx
    bottom = lattice[y1, x0] * (1.0 - tx) + lattice[y1, x1] * tx
    return top * (1.0 - ty) + bottom * ty


def breaker_ridge(rng, size, cell):
    """Ridge noise: 1 - |noise - 0.5| * 2, sharp lines where the octave crosses
    its own midpoint. Thresholded, this is where cracks run."""
    return 1.0 - np.abs(breaker_value_noise(rng, size, cell) - 0.5) * 2.0


def breaker_build(earth):
    """The grey field in 0..1 and the crack mask that was subtracted from it."""
    rng = np.random.default_rng(SEED + (1 if earth else 0))

    # Octave weights sum to one so the field stays centred on 0.5. Concrete
    # leans on the fine octaves (a poured, lightly aggregated surface); earth
    # leans coarse (clods and washed hollows) and gets more per-texel grain.
    if earth:
        weights = (0.10, 0.20, 0.30, 0.40)  # O2 PLACEHOLDER — coarse-heavy
        grain_amp = 0.055                   # O2 PLACEHOLDER — per-texel grit
        contrast = 0.70                     # O2 PLACEHOLDER — spread around 0.5
        crack_cell = 128                    # O2 PLACEHOLDER — crack spacing in texels
        crack_width = 0.060                 # O2 PLACEHOLDER — ridge band kept as line
    else:
        weights = (0.20, 0.30, 0.25, 0.25)  # O2 PLACEHOLDER — fine-heavy
        grain_amp = 0.035                   # O2 PLACEHOLDER — per-texel grit
        contrast = 0.55                     # O2 PLACEHOLDER — spread around 0.5
        crack_cell = 256                    # O2 PLACEHOLDER — crack spacing in texels
        crack_width = 0.045                 # O2 PLACEHOLDER — ridge band kept as line
    crack_depth = 0.03                      # O2 PLACEHOLDER — the ruling's ~3% darker
    wander_cell = 32                        # O2 PLACEHOLDER — the finer ridge that breaks a line
    wander_keep = 0.95                      # O2 PLACEHOLDER — fraction of a line that survives

    field = np.zeros((SIZE, SIZE), dtype=np.float64)
    for cell, weight in zip(OCTAVE_CELLS, weights):
        field += weight * breaker_value_noise(rng, SIZE, cell)
    field = 0.5 + (field - 0.5) * contrast

    # Per-texel grain: uniform noise, seamless by nature (no neighbour term).
    field += (rng.random((SIZE, SIZE)) - 0.5) * grain_amp

    # A few faint cracks: the ridge of a low-frequency octave, thresholded to a
    # thin line, then a finer ridge breaks the line so it wanders. Both
    # octaves wrap, so a crack that leaves the right edge re-enters left.
    ridge = breaker_ridge(rng, SIZE, crack_cell)
    wander = breaker_ridge(rng, SIZE, wander_cell)
    crack_mask = (ridge > 1.0 - crack_width) & (wander < wander_keep)
    field -= crack_mask * crack_depth

    return np.clip(field, 0.0, 1.0), crack_mask


def main(argv):
    parser = argparse.ArgumentParser(description="Writes the composer slab's seamless ground grain.")
    parser.add_argument("--earth", action="store_true",
                        help="the warmer, coarser variant (still grey; the tint carries colour)")
    args = parser.parse_args(argv)

    field, cracks = breaker_build(args.earth)
    name = "ground_earth.png" if args.earth else "ground_concrete.png"
    out = os.path.join(OUT_DIR, name)
    os.makedirs(OUT_DIR, exist_ok=True)

    pixels = np.round(field * 255.0).astype(np.uint8)
    Image.fromarray(pixels, mode="L").save(out, optimize=True)

    # The wrap is the claim this file makes; print it beside the stats so a
    # broken seam shows in the run, not in the yard. The edge step should sit
    # at the same order as the interior step.
    seam = 0.5 * (np.abs(field[:, 0] - field[:, -1]).mean()
                  + np.abs(field[0, :] - field[-1, :]).mean())
    inner = np.abs(field[:, 1:] - field[:, :-1]).mean()
    print("%s: %dx%d L  mean %.4f  min %.4f  max %.4f  cracks %.2f%% of texels  "
          "edge step %.4f vs interior step %.4f"
          % (out, SIZE, SIZE, field.mean(), field.min(), field.max(),
             100.0 * cracks.mean(), seam, inner))


if __name__ == "__main__":
    main(sys.argv[1:])
