#!/usr/bin/env bash
#
# Embed the dynamic libraries ImPPG links against into the macOS application
# bundle, so it runs on machines without Homebrew (or equivalent) installed.
#
# Usage: bundle-dylibs.sh <path-to-ImPPG.app>
#
# Requires the "dylibbundler" tool (brew install dylibbundler).
#
# After bundling, the bundle is re-signed: ad-hoc by default (mandatory, as
# dylibbundler modifies the executable and its libraries, invalidating the
# previous signature), or with the identity named by the IMPPG_CODESIGN_IDENTITY
# environment variable if set. Notarization is not performed; for distribution
# with a Developer ID, submit the resulting bundle to Apple's notarytool
# afterwards (see README, section 12.1.4).

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <path-to-ImPPG.app>" >&2
    exit 2
fi

APP="$1"
EXE="$APP/Contents/MacOS/ImPPG"

[[ -x "$EXE" ]] || { echo "not an ImPPG application bundle: $APP" >&2; exit 1; }
command -v dylibbundler >/dev/null 2>&1 || {
    echo "missing required tool: dylibbundler" >&2
    echo "install it with: brew install dylibbundler" >&2
    exit 1
}

# This script must only run on a freshly linked executable (which is what the
# CMake POST_BUILD hook guarantees): re-bundling an already bundled one would
# wipe Contents/Frameworks/ before the rewritten load commands can be resolved.
if otool -L "$EXE" | grep -q '@executable_path/../Frameworks/'; then
    if [[ -d "$APP/Contents/Frameworks" && -n "$(ls -A "$APP/Contents/Frameworks" 2>/dev/null)" ]]; then
        echo "$APP: already self-contained, nothing to do"
        exit 0
    fi
    echo "$APP: executable references bundled libraries, but Contents/Frameworks is missing" >&2
    echo "(a partially bundled app cannot be re-bundled; rebuild ImPPG)" >&2
    exit 1
fi

echo "Bundling dylibs into $APP"
dylibbundler \
    --fix-file "$EXE" \
    --bundle-deps \
    --dest-dir "$APP/Contents/Frameworks/" \
    --install-path "@executable_path/../Frameworks/" \
    --overwrite-dir \
    --no-codesign

# Guard against a Homebrew/MacPorts path slipping through un-bundled
# (system libraries live in /usr/lib and /System; everything else is local).
if otool -L "$EXE" | grep -qE '/opt/homebrew|/opt/local|/usr/local/'; then
    echo "ERROR: non-system library paths remain after bundling:" >&2
    otool -L "$EXE" | grep -E '/opt/homebrew|/opt/local|/usr/local/' >&2
    exit 1
fi

# dylibbundler rewrote load commands, invalidating the previous signature;
# re-sign every Mach-O in the bundle (frameworks first, the app last).
if [[ -n "${IMPPG_CODESIGN_IDENTITY:-}" ]]; then
    codesign_args=(--force --timestamp --options runtime --sign "$IMPPG_CODESIGN_IDENTITY")
else
    codesign_args=(--force --sign -)
fi
while IFS= read -r -d '' f; do
    codesign "${codesign_args[@]}" "$f"
done < <(find "$APP/Contents/Frameworks" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)
codesign "${codesign_args[@]}" "$EXE"
codesign "${codesign_args[@]}" "$APP"
codesign --verify --strict --verbose=2 "$APP"

echo "$APP is self-contained"
