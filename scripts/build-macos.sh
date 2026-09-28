#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
QT_PREFIX=${QT_PREFIX:-$(brew --prefix qtbase)}
QMAKE=${QMAKE:-$QT_PREFIX/bin/qmake}
MACDEPLOYQT=${MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}
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
"$QMAKE" "$ROOT/tipp10.pro" CONFIG+=release "TIPP10_DATABASE_QRC=$BUILD_DIR/bundled-database.qrc" "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET"
make -j "${JOBS:-8}"
if ! "$MACDEPLOYQT" "$BUILD_DIR/bin/tipp10.app" -always-overwrite -codesign=- "-libpath=$QT_PREFIX/lib" "-libpath=$QT_PREFIX/../../lib" > deploy.log 2>&1; then
    cat deploy.log
    exit 1
fi
cat deploy.log
# Some macdeployqt versions report missing dependencies but return success.
if grep -q '^ERROR:' deploy.log; then
    exit 1
fi
codesign --verify --deep --strict "$BUILD_DIR/bin/tipp10.app"
printf '\nApp ready: %s/bin/tipp10.app\n' "$BUILD_DIR"
