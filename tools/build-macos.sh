#!/bin/sh
# Native macOS universal Release build. Does not modify synced Windows caches.
set -eu
looklan_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
[ "$(uname -s)" = Darwin ] || { echo "Run this script on macOS" >&2; exit 1; }
python3 "$looklan_root/tools/build_preflight.py" macos
looklan_compiler="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["compiler"])' "$looklan_root/build/validation/build-preflight-macos.json")"
looklan_cmake="${LOOKLAN_CMAKE:-cmake}"
if ! command -v "$looklan_cmake" >/dev/null 2>&1; then
    looklan_cmake="$looklan_root/build/native-tools/cmake/data/bin/cmake"
fi
"$looklan_cmake" --fresh -S "$looklan_root" -B "$looklan_root/build/macos-native-universal" \
    -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_C_COMPILER=$looklan_compiler" "-DCMAKE_OBJC_COMPILER=$looklan_compiler" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
"$looklan_cmake" --build "$looklan_root/build/macos-native-universal" --parallel "$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

looklan_archs="$(lipo -archs "$looklan_root/build/macos-native-universal/Look@LAN-ng.app/Contents/MacOS/Look@LAN-ng")"
case " $looklan_archs " in *" arm64 "*) ;; *) echo "Missing arm64 slice" >&2; exit 1;; esac
case " $looklan_archs " in *" x86_64 "*) ;; *) echo "Missing x86_64 slice" >&2; exit 1;; esac
