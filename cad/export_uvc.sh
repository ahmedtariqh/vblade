#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p stl/uvc
for diameter in 5.5 7.0; do
  suffix="${diameter/./_}"
  openscad --export-format binstl -D 'part="uvc_clamp"' -D "uvc_diameter=$diameter" \
    -o "stl/uvc/clamp_${suffix}mm.stl" cad/vblade.scad
  for size in 3 4; do
    openscad --export-format binstl -D 'part="uvc_blade"' -D "uvc_diameter=$diameter" \
      -D "preview_size=$size" -o "stl/uvc/blade_${size}_${suffix}mm.stl" cad/vblade.scad
  done
done
