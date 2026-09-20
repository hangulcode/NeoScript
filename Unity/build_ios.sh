#!/usr/bin/env bash
# build_ios.sh — iOS 용 정적 라이브러리를 빌드해 패키지에 넣는다.
#
#   ./Unity/build_ios.sh              # device (arm64)
#   ./Unity/build_ios.sh simulator    # simulator (arm64 + x86_64)
#
# [이 스크립트는 macOS + Xcode 에서만 돈다.] iOS 는 동적 라이브러리 로딩을 막아 두었기
# 때문에 .so/.dylib 가 아니라 .a 를 링크하고, C# 쪽은 DllImport("__Internal") 로 해소한다.
# 그래서 Windows/Android 와 달리 산출물이 정적 라이브러리다.
#
# 빌드한 .a 는 Unity 가 Xcode 프로젝트에 링크한다. IL2CPP 가 생성하는 extern 선언이
# 심볼을 참조하므로 링커가 떼어 내지 않는다.

set -euo pipefail

TARGET="${1:-device}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
PLUGIN_DIR="$SCRIPT_DIR/com.neoscript.unity/Runtime/Plugins/iOS"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: iOS libraries can only be built on macOS with Xcode." >&2
    exit 1
fi

case "$TARGET" in
    device)
        SYSROOT="iphoneos"
        ARCHS="arm64"
        BUILD_DIR="$REPO_ROOT/build/unity-ios"
        ;;
    simulator)
        SYSROOT="iphonesimulator"
        ARCHS="arm64;x86_64"
        BUILD_DIR="$REPO_ROOT/build/unity-ios-sim"
        ;;
    *)
        echo "usage: $0 [device|simulator]" >&2
        exit 2
        ;;
esac

echo "==> configure ($TARGET, $ARCHS)"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_IOS_INSTALL_COMBINED=NO \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=13.0 \
    -DNEOSCRIPT_BUILD_STATIC_C_ABI=ON \
    -DNEOSCRIPT_BUILD_SMOKE=OFF

echo "==> build"
cmake --build "$BUILD_DIR" --config Release --target NeoScriptStatic

LIB="$(find "$BUILD_DIR" -name 'libNeoScriptUnity.a' -print -quit)"
if [[ -z "$LIB" ]]; then
    echo "error: libNeoScriptUnity.a not found under $BUILD_DIR" >&2
    exit 1
fi

mkdir -p "$PLUGIN_DIR"
cp "$LIB" "$PLUGIN_DIR/libNeoScriptUnity.a"

echo "==> deployed $PLUGIN_DIR/libNeoScriptUnity.a"
ls -lh "$PLUGIN_DIR/libNeoScriptUnity.a"
echo
echo "Remember: the .meta beside it must have the iOS platform enabled."
echo "If Unity generated a bare .meta, open the importer and tick iOS, or copy the"
echo "settings from the Android/Windows plugin metas in the same package."
