#!/usr/bin/env bash
# Build the OpenOSV RPM (osvtool + OpenOSV Studio + 360 Director).
# Tested target: Fedora 44 Workstation. Run from the folder holding openosv.spec.
# Needs RPM Fusion (free) for the full ffmpeg / ffmpeg-devel.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"

echo "==> Checking for RPM Fusion"
if ! dnf repolist --enabled 2>/dev/null | grep -q '^rpmfusion-free'; then
  echo "RPM Fusion (free) is not enabled. Fedora's ffmpeg-free cannot decode .OSV (HEVC)." >&2
  echo "See https://rpmfusion.org/Configuration, then run this again." >&2
  exit 1
fi

echo "==> Build tools"
sudo dnf install -y rpm-build rpmdevtools 'dnf-command(builddep)'

echo "==> rpmbuild tree"
rpmdev-setuptree
cp "$here"/openosv.spec ~/rpmbuild/SPECS/
cp "$here"/patches/*.patch \
   "$here"/director/* \
   "$here"/io.github.kemerd.OpenOSVStudio.desktop \
   "$here"/io.github.kemerd.OpenOSVStudio.svg ~/rpmbuild/SOURCES/

echo "==> Download pinned sources (OpenOSV commit + Dear ImGui)"
spectool -g -R ~/rpmbuild/SPECS/openosv.spec

echo "==> Build dependencies"
sudo dnf builddep -y ~/rpmbuild/SPECS/openosv.spec

echo "==> Build (about 10-15 minutes on an i7-7700K)"
rpmbuild -ba ~/rpmbuild/SPECS/openosv.spec

echo
echo "Done. Install or upgrade with:"
ls ~/rpmbuild/RPMS/*/openosv-[0-9]*.rpm | sort -V | tail -1 | sed 's/^/  sudo dnf install /'
echo "Source package (rebuild on another Fedora/EL machine with RPM Fusion):"
ls ~/rpmbuild/SRPMS/openosv-[0-9]*.src.rpm | sort -V | tail -1 | sed 's/^/  /'
