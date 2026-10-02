#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
QT_PREFIX=${QT_PREFIX:-$(brew --prefix qtbase)}
QMAKE=${QMAKE:-$QT_PREFIX/bin/qmake}
MACDEPLOYQT=${MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}
# Resolve this lexically: QT_PREFIX can be a symlink into Homebrew Cellar.
QT_LIBRARY_PATH=${QT_LIBRARY_PATH:-$(dirname -- "$(dirname -- "$QT_PREFIX")")/lib}
QT_PLUGINS_PATH=${QT_PLUGINS_PATH:-$(dirname -- "$(dirname -- "$QT_PREFIX")")/share/qt/plugins}
if [ -n "${TIPP10_DATABASE:-}" ]; then
    DATABASE_SOURCE=$TIPP10_DATABASE
    BUILD_DIR=${BUILD_DIR:-$ROOT/build/macos-custom}
    printf 'Building with a custom local database; this app will contain its lessons.\n'
else
    DATABASE_SOURCE=$ROOT/release/tipp10v2.template
    BUILD_DIR=${BUILD_DIR:-$ROOT/build/macos}
fi
if [ ! -f "$DATABASE_SOURCE" ]; then
    printf 'Database not found: %s\n' "$DATABASE_SOURCE" >&2
    exit 1
fi
DATABASE_SOURCE=$(CDPATH= cd -- "$(dirname -- "$DATABASE_SOURCE")" && pwd)/$(basename -- "$DATABASE_SOURCE")
# Homebrew bottles can require the macOS version they were built for.
# Override only when all installed Qt libraries support the older target.
MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-$(sw_vers -productVersion | cut -d. -f1).0}
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
BUILD_DIR=$(pwd)
# Always refresh both files so switching databases in the same build directory
# cannot reuse a resource object containing the previous database.
mkdir -p bundled-database
cp "$DATABASE_SOURCE" bundled-database/tipp10v2.template
cat > bundled-database.qrc <<'QRC'
<RCC>
    <qresource prefix="/">
        <file alias="tipp10v2.template">bundled-database/tipp10v2.template</file>
    </qresource>
</RCC>
QRC
"$QMAKE" "$ROOT/tipp10.pro" CONFIG+=release "TIPP10_DATABASE_QRC=$BUILD_DIR/bundled-database.qrc" "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET" CONFIG+=sdk_no_version_check
make -j "${JOBS:-8}"

# Tipp10 never uses SVG (no .svg resources anywhere in the app), but Homebrew's
# shared Qt plugin directory can still contain the imageformats/iconengines SVG
# plugins (pulled in by an unrelated qtsvg install). macdeployqt always tries
# to bundle every imageformats/iconengines plugin it finds, and its -libpath
# search does not cover resolving *those* plugins' own framework dependency
# (QtSvg.framework) - so if qtsvg happens to be installed, deployment fails
# with "Cannot resolve rpath .../QtSvg.framework". Since nothing here needs
# them, hide the two plugin files for the duration of this script only.
SVG_PLUGIN_IMAGEFORMAT="$QT_PLUGINS_PATH/imageformats/libqsvg.dylib"
SVG_PLUGIN_ICONENGINE="$QT_PLUGINS_PATH/iconengines/libqsvgicon.dylib"
SVG_PLUGIN_HOLDING=$(mktemp -d)
restore_svg_plugins() {
    # -e alone is not enough: these Homebrew plugin files can themselves be
    # *relative* symlinks into the Cellar, which resolve fine in their
    # original directory but become dangling once moved into $TMPDIR - so
    # -e (which follows symlinks) would report them as missing. -L detects
    # the symlink itself regardless of whether its target currently resolves.
    if [ -e "$SVG_PLUGIN_HOLDING/libqsvg.dylib" ] || [ -L "$SVG_PLUGIN_HOLDING/libqsvg.dylib" ]; then
        mv "$SVG_PLUGIN_HOLDING/libqsvg.dylib" "$SVG_PLUGIN_IMAGEFORMAT"
    fi
    if [ -e "$SVG_PLUGIN_HOLDING/libqsvgicon.dylib" ] || [ -L "$SVG_PLUGIN_HOLDING/libqsvgicon.dylib" ]; then
        mv "$SVG_PLUGIN_HOLDING/libqsvgicon.dylib" "$SVG_PLUGIN_ICONENGINE"
    fi
    rmdir "$SVG_PLUGIN_HOLDING" 2>/dev/null || true
}
trap restore_svg_plugins EXIT
if [ -e "$SVG_PLUGIN_IMAGEFORMAT" ] || [ -L "$SVG_PLUGIN_IMAGEFORMAT" ]; then
    mv "$SVG_PLUGIN_IMAGEFORMAT" "$SVG_PLUGIN_HOLDING/"
fi
if [ -e "$SVG_PLUGIN_ICONENGINE" ] || [ -L "$SVG_PLUGIN_ICONENGINE" ]; then
    mv "$SVG_PLUGIN_ICONENGINE" "$SVG_PLUGIN_HOLDING/"
fi

# Set CODESIGN_IDENTITY (e.g. "Developer ID Application: Your Name (TEAMID)")
# to sign with a real certificate instead of the default ad-hoc signature.
# A real, hardened-runtime signature is required before notarization can
# succeed; -sign-for-notarization enables both the identity and hardened
# runtime, plus a secure timestamp, in one step.
if [ -n "${CODESIGN_IDENTITY:-}" ]; then
    CODESIGN_ARG="-sign-for-notarization=$CODESIGN_IDENTITY"
else
    CODESIGN_ARG="-codesign=-"
fi

if ! "$MACDEPLOYQT" "$BUILD_DIR/bin/tipp10.app" -always-overwrite "$CODESIGN_ARG" "-libpath=$QT_PREFIX/lib" "-libpath=$QT_LIBRARY_PATH" > deploy.log 2>&1; then
    cat deploy.log
    exit 1
fi
cat deploy.log
# Some macdeployqt versions report missing dependencies but return success.
if grep -q '^ERROR:' deploy.log; then
    exit 1
fi
# A plain make relinks the executable against Homebrew even when an older
# deployed bundle is still present. Signing alone cannot make it portable.
# Verify deployment rewrote every Qt dependency of the main executable.
EXECUTABLE="$BUILD_DIR/bin/tipp10.app/Contents/MacOS/tipp10"
otool -L "$EXECUTABLE" > executable-dependencies.log
if grep -E '^[[:space:]]+/.*/Qt[^/]*\.framework/' executable-dependencies.log; then
    printf 'Deployment left external Qt dependencies in the executable.\n' >&2
    exit 1
fi
if [ ! -f "$BUILD_DIR/bin/tipp10.app/Contents/PlugIns/platforms/libqcocoa.dylib" ]; then
    printf 'Deployment did not include the macOS platform plugin.\n' >&2
    exit 1
fi
codesign --verify --deep --strict "$BUILD_DIR/bin/tipp10.app"

if [ -z "${CODESIGN_IDENTITY:-}" ]; then
    printf '\nApp ready (ad-hoc signed, local use only): %s/bin/tipp10.app\n' "$BUILD_DIR"
elif [ -z "${NOTARIZE_KEYCHAIN_PROFILE:-}" ]; then
    printf '\nApp ready (signed, not notarized): %s/bin/tipp10.app\n' "$BUILD_DIR"
    printf 'Set NOTARIZE_KEYCHAIN_PROFILE to also notarize before distributing.\n'
else
    # Notarization requires uploading a zip (or dmg/pkg), never a raw .app.
    # ditto (not zip/Finder) is required here so the archive preserves the
    # exact symlink/resource-fork structure the code signature depends on.
    NOTARY_ZIP="$BUILD_DIR/bin/tipp10-notarize.zip"
    rm -f "$NOTARY_ZIP"
    ditto -c -k --keepParent "$BUILD_DIR/bin/tipp10.app" "$NOTARY_ZIP"

    NOTARY_LOG="$BUILD_DIR/notarize.log"
    NOTARY_OK=1
    xcrun notarytool submit "$NOTARY_ZIP" --keychain-profile "$NOTARIZE_KEYCHAIN_PROFILE" --wait --output-format json > "$NOTARY_LOG" 2>&1 || NOTARY_OK=0
    cat "$NOTARY_LOG"
    if [ "$NOTARY_OK" != "1" ] || ! grep -q '"status"[[:space:]]*:[[:space:]]*"Accepted"' "$NOTARY_LOG"; then
        NOTARY_ID=$(grep -o '"id"[[:space:]]*:[[:space:]]*"[^"]*"' "$NOTARY_LOG" | head -1 | sed -E 's/.*"([^"]+)"$/\1/')
        if [ -n "${NOTARY_ID:-}" ]; then
            printf '\nNotarization failed; fetching Apple log for submission %s:\n' "$NOTARY_ID"
            xcrun notarytool log "$NOTARY_ID" --keychain-profile "$NOTARIZE_KEYCHAIN_PROFILE" || true
        fi
        exit 1
    fi
    xcrun stapler staple "$BUILD_DIR/bin/tipp10.app"
    xcrun stapler validate "$BUILD_DIR/bin/tipp10.app"
    # Include installation instructions beside the stapled app in the ZIP.
    PACKAGE_DIR="$BUILD_DIR/distribution/Tipp10"
    mkdir -p "$PACKAGE_DIR"
    ditto "$BUILD_DIR/bin/tipp10.app" "$PACKAGE_DIR/tipp10.app"
    cp "$ROOT/release/Installation-de.txt" "$PACKAGE_DIR/Installation-de.txt"
    if [ -n "${TIPP10_DATABASE:-}" ]; then
        printf '\nDieses Paket enthaelt spezielle Uebungen fuer den vorgesehenen Nutzerkreis. Bitte geben Sie dieses Paket nicht weiter.\n' >> "$PACKAGE_DIR/Installation-de.txt"
    fi
    cp "$ROOT/LICENSE" "$PACKAGE_DIR/LICENSE"
    ditto -c -k --keepParent "$PACKAGE_DIR" "$NOTARY_ZIP"
    printf '\nApp ready (signed and notarized): %s/bin/tipp10.app\n' "$BUILD_DIR"
    printf 'Notarized archive for distribution: %s\n' "$NOTARY_ZIP"
fi
