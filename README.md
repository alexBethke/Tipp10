# Tipp10 for modern macOS

An unofficial Qt 6 port of the TIPP10 touch typing tutor, with native Apple
Silicon support and optional local lesson databases.

This project is a **fork of [Karl Zeilhofer's Tipp10 fork](https://github.com/KarlZeilhofer/Tipp10)**,
which is based on the original [TIPP10](https://www.tipp10.com) version 2.1.0 by
Tom Thielicke IT Solutions. It continues Karl's work with updates for modern
macOS; it is not an official TIPP10 release.

## Changes in this fork

- Ported the desktop application to Qt 6 and C++17.
- Added a macOS build script that produces a self-contained `.app` with bundled
  Qt frameworks and plugins, an application icon, and an ad-hoc signature.
- Enabled automatic platform detection, Mac keyboard layout defaults, and
  native macOS widget styling.
- Embedded sounds, offline help, and the original starter database.
- Added optional bundling of a local database for personal builds; school lessons
  are excluded from the default build.
- Fixed the macOS database path lookup and an out-of-bounds numpad array write.
- Added a smoke test covering lesson input, saved results, database reopening,
  sound loading, help, and settings.

The current build has been verified on **Apple Silicon with macOS 26.6.2 and
Qt 6.11.2**. Intel Macs and other operating systems have not been tested in this
fork. The German interface and disabled legacy `ONLINE` integration are retained
from the upstream fork.

## Build and run on macOS

Install Xcode or its Command Line Tools and [Homebrew](https://brew.sh), then run
these commands from the repository root:

```sh
brew install qtbase qtmultimedia
./scripts/build-macos.sh
open build/macos/bin/tipp10.app
```

You can copy `build/macos/bin/tipp10.app` to Applications. The packaged app includes
its runtime libraries, so Homebrew is only needed to build it.

The script targets the host architecture and macOS major version. The build
produced on macOS 26 requires macOS 26 or later. It is not a universal binary;
building for Intel requires an Intel Qt installation and a corresponding build.
Public distribution requires your own Developer ID signing and notarization.

Build settings can be overridden with environment variables:

| Variable | Purpose |
| --- | --- |
| `QT_PREFIX` | Use a different Qt installation instead of Homebrew's `qtbase`. |
| `BUILD_DIR` | Output directory; defaults to `build/macos`, or `build/macos-custom` with a custom database. |
| `TIPP10_DATABASE` | Explicitly bundle a local SQLite database instead of the original starter database. |
| `JOBS` | Set parallel build jobs; defaults to 8. |
| `MACOSX_DEPLOYMENT_TARGET` | Target an older macOS version, only if all installed Qt libraries support it. |

The root `Makefile` is an old generated Linux build file. Use the macOS script,
which keeps generated files in `build/macos`. See also Qt's
[macOS deployment documentation](https://doc.qt.io/qt-6/macos-deployment.html).

## Lessons and database

Default builds bundle only the original `release/tipp10v2.template`. They do not
include lessons from **Dientzenhofer Schule - Brannenburg**. Merely placing a
`tipp10v2.db` file in the repository does not include it in a build.

### Optional local database

If you have a local copy of the school lessons, or another compatible Tipp10
database, you can explicitly bundle it for personal use:

```sh
TIPP10_DATABASE="$PWD/tipp10v2.db" ./scripts/build-macos.sh
open build/macos-custom/bin/tipp10.app
```

You may instead point `TIPP10_DATABASE` to a file outside this repository. Custom
builds go to `build/macos-custom` by default, keeping them separate from the
standard app in `build/macos`. To return to a standard build, run the script
without `TIPP10_DATABASE` set.

The local `tipp10v2.db`, its SQLite sidecar files, `private-lessons/`, and build
outputs are ignored by Git. Keep other private database files outside the
repository or inside `private-lessons/`. Do not force-add these files. Git ignore
rules do not remove files already committed to history.

**A custom-built app contains the selected database and its lessons.** Keep
that app private unless you have permission to distribute the lesson content.
The optional bundling feature does not grant rights to third-party lessons.

### Active user database

The selected starter database is embedded under the internal resource name
`tipp10v2.template`. On first launch, the app creates a writable copy at:

```text
~/Library/Application Support/tipp10/tipp10v2.db
```

Previously configured database locations are respected. Custom lessons appear
under **Eigene Lektionen**. The local school database used during development
contains 163 custom lessons.

Both standard and custom builds use the same user settings and database location.
Replacing or rebuilding the app **does not replace an existing user database**.
Your installed school lessons therefore remain available when you run a standard
build on the same Mac, even though they are not in that app's bundle.

To use a different local database without rebuilding:

1. Quit Tipp10 completely.
2. Locate the active database. The default location is shown above; check the
   database path in the application's settings if you have changed it.
3. Back up that database. It contains your own lessons and training history.
4. Copy your chosen database into that location as `tipp10v2.db`.
5. Reopen Tipp10 and select **Eigene Lektionen**.

This replaces the active database; it does not merge existing lessons or results.
Keep the backup if you need to restore your previous data.

## Smoke test

The test uses a temporary database and temporary settings, leaving your personal
training data untouched. It checks lesson initialization, typing, saved results,
database reopening, embedded sound loading, offline help, and settings writes.

```sh
mkdir -p build/smoke
cd build/smoke
"$(brew --prefix qtbase)/bin/qmake" ../../tests/smoke.pro
make -j8
QT_QPA_PLATFORM=offscreen ./bin/tipp10-smoke ../../release/tipp10v2.template
```

The optional argument verifies that the embedded starter database exactly matches
the specified file.

Sound checks require normal macOS audio access, even with the offscreen platform.

## Upstream work and credits

Karl Zeilhofer's fork improved error correction to behave more like a text
editor and introduced the `ONLINE` build flag during its Qt 4 to Qt 5 transition.
Those changes are inherited here. The legacy network code remains disabled and
has not been ported to Qt 6. The available interface language remains German,
configured by `APP_EXISTING_LANGUAGES_GUI` in `def/defines.h`.

![Error correction in the upstream fork](screenshot-v2.1.1.png)

[Upstream demonstration video](https://youtu.be/XZ6Yd2Q7kIQ)

## License

The original TIPP10 code and this fork are distributed under the GNU General
Public License, version 2. See [LICENSE](LICENSE) and the copyright notices in
the source files.
