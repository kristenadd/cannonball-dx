#!/bin/bash
# CI: write the apple-overrides.cmake the Apple builds configure with, and export its path as
# APPLE_OVERRIDES_FILE. Release builds keep the default CannonBall DX name and icon; this only
# sets the signing team and the bundle ID. The App Store / TestFlight builds also pass
# ASSETS_DIR, a checkout of the private kristenadd/cannonball-dx-ci-assets repo, whose
# app-store.cmake sets their icon.
#
# Environment: APPLE_TEAM_ID, BUNDLE_ID (optional), ASSETS_DIR (optional).
set -euo pipefail

overrides="$RUNNER_TEMP/apple-overrides.cmake"
{
    echo "set(APPLE_DEVELOPMENT_TEAM \"$APPLE_TEAM_ID\")"
    if [ -n "${BUNDLE_ID:-}" ]; then echo "set(APPLE_BUNDLE_ID \"$BUNDLE_ID\")"; fi
    if [ -n "${ASSETS_DIR:-}" ]; then
        [ -f "$ASSETS_DIR/app-store.cmake" ] || { echo "::error::$ASSETS_DIR/app-store.cmake is missing"; exit 1; }
        echo "include(\"$ASSETS_DIR/app-store.cmake\")"
    fi
} > "$overrides"
cat "$overrides"
echo "APPLE_OVERRIDES_FILE=$overrides" >> "$GITHUB_ENV"
