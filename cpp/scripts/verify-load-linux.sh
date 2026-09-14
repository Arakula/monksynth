#!/usr/bin/env bash
#
# Verify a Linux MonkSynth.so loads cleanly under the strict loader
# semantics that hosts like Bitwig use (dlopen RTLD_NOW), in clean
# minimal containers for several common distros. This catches the
# class of bug where a static dependency leaks into pango/cairo etc.
# but isn't bundled — the plugin loads on the dev box (which has the
# missing system lib installed) but fails for users on bare distros.
#
# Usage:  ./verify-load-linux.sh <path/to/MonkSynth.so>
#
# Exit status: 0 if all distros pass, 1 if any fail.

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <path/to/MonkSynth.so>" >&2
    exit 2
fi

PLUGIN_HOST="$(readlink -f "$1")"
if [ ! -f "$PLUGIN_HOST" ]; then
    echo "error: plugin not found: $PLUGIN_HOST" >&2
    exit 2
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/dlopen-test.c" <<'EOF'
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s <plugin.so>\n", argv[0]); return 2; }
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen FAIL: %s\n", dlerror()); return 1; }
    fprintf(stdout, "dlopen OK\n");
    dlclose(h);
    return 0;
}
EOF

TEST_B64="$(base64 < "$WORK/dlopen-test.c" | tr -d '\n')"

# Distros to test. Format: "image|family", family is 'apt', 'dnf' or 'pacman'.
#
# Rolling tags keep this list evergreen: ubuntu:latest is the current LTS,
# ubuntu:rolling the newest release (LTS or not), debian:stable/oldstable
# track Debian's releases. Only ubuntu:22.04 is pinned, because it is the
# image the plugin is built on and therefore the glibc floor we promise.
DISTROS=(
    "ubuntu:22.04|apt"        # build image, glibc 2.35 floor (Linux Mint 21.x base)
    "ubuntu:latest|apt"       # current Ubuntu LTS (Linux Mint / Pop!_OS base)
    "ubuntu:rolling|apt"      # newest Ubuntu release, catches upcoming changes early
    "debian:oldstable|apt"    # previous Debian (KX Studio, AV Linux, MX Linux lag stable)
    "debian:stable|apt"       # current Debian
    "fedora:latest|dnf"       # Fedora / RHEL family
    "archlinux:latest|pacman" # Arch / Manjaro / EndeavourOS / CachyOS
)

# Runtime libraries that the plugin links against dynamically
# (per ldd of a current build). Names per distro family. On apt distros the
# libpng package is libpng16-16 before the 2024 time64 transition and
# libpng16-16t64 after it; the install line picks whichever the image has.
APT_PKGS="libxcb1 libxcb-xkb1 libxcb-render0 libxcb-shm0 libexpat1 libstdc++6 gcc libc6-dev"
APT_LIBPNG='$(apt-cache show libpng16-16t64 >/dev/null 2>&1 && echo libpng16-16t64 || echo libpng16-16)'
DNF_PKGS="libxcb xcb-util-keysyms libpng expat libstdc++ gcc glibc-devel"
PACMAN_PKGS="libxcb expat libpng gcc"

pass=0
fail=0
failed_images=()

for entry in "${DISTROS[@]}"; do
    image="${entry%%|*}"
    family="${entry##*|}"

    case "$family" in
        apt)     install="apt-get -qq update >/dev/null 2>&1 && apt-get -qq install -y --no-install-recommends ${APT_PKGS} ${APT_LIBPNG} >/dev/null 2>&1" ;;
        dnf)     install="dnf -q -y install ${DNF_PKGS} >/dev/null 2>&1" ;;
        # --disable-sandbox: pacman's download sandbox (seccomp + user switch)
        # fails under Rosetta/qemu emulation, e.g. running this on Apple Silicon.
        pacman)  install="pacman -Sy --disable-sandbox --noconfirm --needed --noprogressbar ${PACMAN_PKGS} >/dev/null 2>&1" ;;
        *)       echo "unknown family: $family" >&2; exit 2 ;;
    esac

    printf "%-20s ... " "$image"

    # The plugin is x86_64 only; pin the platform so the script also works
    # on Apple Silicon (Colima/Docker Desktop run amd64 images via Rosetta).
    # The test program travels inside the command (base64) rather than as a
    # second bind mount, so only the plugin path has to be visible to the VM.
    output=$(docker run --rm --platform linux/amd64 \
        -v "$PLUGIN_HOST:/plugin.so:ro" \
        -e DEBIAN_FRONTEND=noninteractive \
        "$image" \
        bash -c "set -e; $install; echo $TEST_B64 | base64 -d > /tmp/dlopen-test.c; gcc /tmp/dlopen-test.c -o /tmp/t -ldl; /tmp/t /plugin.so" 2>&1) && rc=0 || rc=$?

    if [ "$rc" = 0 ]; then
        echo "OK"
        pass=$((pass+1))
    else
        echo "FAIL"
        echo "$output" | sed 's/^/    /'
        fail=$((fail+1))
        failed_images+=("$image")
    fi
done

echo ""
echo "Passed: $pass / $((pass+fail))"
if [ "$fail" -gt 0 ]; then
    echo "Failed: ${failed_images[*]}"
    exit 1
fi
