#!/usr/bin/env bash
# director-proxy - make a light preview copy of a 360 (equirectangular) render
# for 360 Director. The browser plays this; the full-quality master is only
# used when you render the final video.
#
#   director-proxy MyClip.mp4            -> MyClip.proxy.mp4
#   director-proxy MyClip.mp4 out.mp4
set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo "Usage: $0 MASTER.mp4 [PROXY.mp4]" >&2
  exit 1
fi
in="$1"
out="${2:-${in%.*}.proxy.mp4}"
command -v ffmpeg >/dev/null || { echo "ffmpeg not found: sudo dnf install -y ffmpeg" >&2; exit 1; }
echo "Making preview copy: $out"
ffmpeg -hide_banner -loglevel warning -stats -y -i "$in" \
  -map 0:v:0 -map '0:a?' \
  -vf "scale=2048:1024:flags=bicubic,format=yuv420p" \
  -c:v libx264 -preset veryfast -crf 23 -g 30 \
  -c:a aac -b:a 128k -movflags +faststart "$out"
echo "Done. Open $out in 360 Director."
