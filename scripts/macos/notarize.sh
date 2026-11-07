#!/usr/bin/env bash
#
# Notarize an ImPPG macOS build (ImPPG.app and its DMG) with Apple's notary
# service and staple the notarization tickets, so Gatekeeper accepts the
# artifacts even on machines without a connection to Apple's servers.
#
# Usage: notarize.sh <build-dir>
#
# The application must already be signed with a "Developer ID Application"
# certificate: build with IMPPG_CODESIGN_IDENTITY set (see
# scripts/macos/bundle-dylibs.sh). The DMG is signed here with the same
# identity when IMPPG_CODESIGN_IDENTITY is set.
#
# Requires Apple Developer Program credentials stored as a notarytool
# keychain profile, named by IMPPG_NOTARY_PROFILE (default "imppg-notary").
# Store them once with:
#   xcrun notarytool store-credentials "imppg-notary" \
#     --apple-id <apple-id> --team-id <team-id> --password <app-specific-pwd>
#
# The .app and the DMG are notarized separately (each submission takes
# 5-15 minutes), so a full run is typically 10-30 minutes.

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <build-dir>" >&2
    exit 2
fi

NOTARY_PROFILE="${IMPPG_NOTARY_PROFILE:-imppg-notary}"
BUILD_DIR="$1"
APP="$BUILD_DIR/ImPPG.app"

[[ -d "$APP" ]] || { echo "no ImPPG.app in $BUILD_DIR" >&2; exit 1; }

shopt -s nullglob
DMGS=("$BUILD_DIR"/imppg-*-macos.dmg)
shopt -u nullglob
[[ ${#DMGS[@]} -eq 1 ]] \
    || { echo "expected one imppg-*-macos.dmg in $BUILD_DIR, found ${#DMGS[@]}" >&2; exit 1; }
DMG="${DMGS[0]}"

if codesign -dv "$APP" 2>&1 | grep -q "Signature=adhoc"; then
    echo "ERROR: $APP is only ad-hoc signed; notarization requires a Developer ID" >&2
    echo "signature. Rebuild with IMPPG_CODESIGN_IDENTITY set (see" >&2
    echo "scripts/macos/bundle-dylibs.sh)." >&2
    exit 1
fi

# Submit an artifact to the notary service, wait for the verdict, and dump
# the service's rejection log on failure. $1 = artifact path, $2 = label.
notarize() {
    local artifact="$1" label="$2" out sid
    echo "==> Notarizing $label (5-15 minutes)"
    if ! out=$(xcrun notarytool submit "$artifact" --keychain-profile "$NOTARY_PROFILE" --wait 2>&1); then
        echo "$out" >&2
        sid=$(echo "$out" | awk '/^  id: / {print $2; exit}')
        [[ -n "${sid:-}" ]] \
            && xcrun notarytool log "$sid" --keychain-profile "$NOTARY_PROFILE" >&2 || true
        exit 1
    fi
    echo "$out"
    if ! echo "$out" | grep -q "status: Accepted"; then
        sid=$(echo "$out" | awk '/^  id: / {print $2; exit}')
        [[ -n "${sid:-}" ]] \
            && xcrun notarytool log "$sid" --keychain-profile "$NOTARY_PROFILE" >&2 || true
        echo "$label notarization was not accepted (see the log above)" >&2
        exit 1
    fi
}

# 1) Notarize the .app first, so its ticket can be stapled into the bundle
#    before the DMG packages it.
APP_ZIP="$BUILD_DIR/ImPPG-notarization.zip"
rm -f "$APP_ZIP"
ditto -c -k --keepParent "$APP" "$APP_ZIP"
notarize "$APP_ZIP" "ImPPG.app"
echo "==> Stapling ticket to $APP"
xcrun stapler staple "$APP"

# 2) Regenerate the DMG from the stapled .app: the one built alongside the
#    application packages it before stapling.
echo "==> Regenerating the DMG from the stapled application"
rm -f "$DMG"
cmake --build "$BUILD_DIR" --target imppg-dmg
rm -f "$APP_ZIP"

# 3) Sign and notarize the DMG, and staple its ticket.
if [[ -n "${IMPPG_CODESIGN_IDENTITY:-}" ]]; then
    echo "==> Signing the DMG"
    codesign --force --timestamp --sign "$IMPPG_CODESIGN_IDENTITY" "$DMG"
fi
notarize "$DMG" "the DMG"
echo "==> Stapling ticket to the DMG"
xcrun stapler staple "$DMG"

# 4) Gatekeeper must accept both artifacts as-is.
spctl --assess --type execute --verbose=2 "$APP"
spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG"

echo "Done: $APP and $DMG are notarized and stapled."
