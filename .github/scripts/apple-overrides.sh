#!/bin/bash
# CI: write the apple-overrides.cmake the Apple builds configure with, and export its path as
# APPLE_OVERRIDES_FILE. Release builds keep the default CannonBall DX name and icon; this only
# sets the signing team and the bundle ID.
#
# Environment: APPLE_TEAM_ID, BUNDLE_ID (optional).
set -euo pipefail

overrides="$RUNNER_TEMP/apple-overrides.cmake"
{
    echo "set(APPLE_DEVELOPMENT_TEAM \"$APPLE_TEAM_ID\")"
    if [ -n "${BUNDLE_ID:-}" ]; then echo "set(APPLE_BUNDLE_ID \"$BUNDLE_ID\")"; fi
} > "$overrides"
cat "$overrides"
echo "APPLE_OVERRIDES_FILE=$overrides" >> "$GITHUB_ENV"
