#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
APP=${TIPP10_APP:-$ROOT/build/macos-signed/bin/tipp10.app}
BUILD_DIR=${INSTALLER_BUILD_DIR:-$ROOT/build/macos-installer}
mkdir -p "$BUILD_DIR/payload" "$BUILD_DIR/resources"
BUILD_DIR=$(CDPATH= cd -- "$BUILD_DIR" && pwd)
# Reuse the already signed and notarized app, without modifying its bundle.
ditto "$APP" "$BUILD_DIR/payload/tipp10.app"
cp "$ROOT/packaging/macos/Welcome.html" "$BUILD_DIR/resources/Welcome.html"
if [ "${TIPP10_CUSTOM_LESSONS:-0}" = "1" ]; then
    sed 's/Das Paket enthaelt nur die originalen Startlektionen./Das Paket enthaelt spezielle Uebungen fuer den vorgesehenen Nutzerkreis. Bitte geben Sie dieses Paket nicht weiter./' \
        "$ROOT/packaging/macos/Welcome.html" > "$BUILD_DIR/resources/Welcome.html"
fi
# Format the license for Installer without changing the installed LICENSE.
cat > "$BUILD_DIR/resources/License.html" <<'HTML'
<!doctype html>
<html lang="en"><meta charset="utf-8">
<style>
body { font-family: -apple-system, BlinkMacSystemFont, system-ui, "Helvetica Neue", sans-serif; font-size: 13px; }
pre { font: inherit; white-space: pre-wrap; }
</style><body><pre>
HTML
awk '{ gsub(/\&/, "\&amp;"); gsub(/</, "\&lt;"); gsub(/>/, "\&gt;"); print }' \
    "$ROOT/LICENSE" >> "$BUILD_DIR/resources/License.html"
printf '</pre></body></html>\n' >> "$BUILD_DIR/resources/License.html"
cp "$ROOT/LICENSE" "$BUILD_DIR/payload/LICENSE"
cp "$ROOT/release/Installation-de.txt" "$BUILD_DIR/payload/Installation-de.txt"
if [ "${TIPP10_CUSTOM_LESSONS:-0}" = "1" ]; then
    printf '\nDieses Paket enthaelt spezielle Uebungen fuer den vorgesehenen Nutzerkreis. Bitte geben Sie dieses Paket nicht weiter.\n' >> "$BUILD_DIR/payload/Installation-de.txt"
fi
pkgbuild --analyze --root "$BUILD_DIR/payload" "$BUILD_DIR/components.plist"
/usr/libexec/PlistBuddy -c 'Add :0:BundleIsRelocatable bool false' "$BUILD_DIR/components.plist"
/usr/libexec/PlistBuddy -c 'Set :0:BundleIsVersionChecked false' "$BUILD_DIR/components.plist"
pkgbuild --root "$BUILD_DIR/payload" \
    --component-plist "$BUILD_DIR/components.plist" \
    --identifier org.tipp10.tipp10.installer --version 2026.10.2 \
    --install-location /Applications/Tipp10 "$BUILD_DIR/Tipp10-component.pkg"
if [ -n "${INSTALLER_SIGN_IDENTITY:-}" ]; then
    OUTPUT="$BUILD_DIR/Tipp10-macOS-26-27-arm64.pkg"
    productbuild --distribution "$ROOT/packaging/macos/Distribution.xml" \
        --resources "$BUILD_DIR/resources" --package-path "$BUILD_DIR" \
        --sign "$INSTALLER_SIGN_IDENTITY" --timestamp "$OUTPUT"
    pkgutil --check-signature "$OUTPUT"
    if [ -n "${NOTARIZE_KEYCHAIN_PROFILE:-}" ]; then
        xcrun notarytool submit "$OUTPUT" --keychain-profile "$NOTARIZE_KEYCHAIN_PROFILE" \
            --wait --output-format json > "$BUILD_DIR/notarize.json"
        cat "$BUILD_DIR/notarize.json"
        if ! grep -q '"status" *: *"Accepted"' "$BUILD_DIR/notarize.json"; then
            printf 'Installer notarization was not accepted.\n' >&2
            exit 1
        fi
        xcrun stapler staple "$OUTPUT"
        xcrun stapler validate "$OUTPUT"
        spctl --assess --type install --verbose=2 "$OUTPUT"
    fi
else
    OUTPUT="$BUILD_DIR/Tipp10-UNSIGNED-preview.pkg"
    productbuild --distribution "$ROOT/packaging/macos/Distribution.xml" \
        --resources "$BUILD_DIR/resources" --package-path "$BUILD_DIR" "$OUTPUT"
    printf 'Unsigned review build; a Developer ID Installer certificate is required for distribution.\n'
fi
printf 'Installer: %s\n' "$OUTPUT"
