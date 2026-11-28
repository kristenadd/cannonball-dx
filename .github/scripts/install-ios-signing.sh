#!/bin/bash
# CI: a temporary keychain with the Apple Development and Apple Distribution identities (plus
# Apple's WWDR G3 intermediate), and the App Store Connect API key at $RUNNER_TEMP/AuthKey.p8.
# Exports KEYCHAIN to $GITHUB_ENV; delete it (and the key) in an `if: always()` step.
#
# Environment: DEV_P12, DEV_PASSWORD, DIST_P12, DIST_PASSWORD (base64 .p12s and their passwords),
# API_KEY_P8 (the .p8 text).
set -euo pipefail

keychain="$RUNNER_TEMP/release.keychain-db"
keychain_password=$(openssl rand -base64 24)
security create-keychain -p "$keychain_password" "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
for pair in "DEV_P12:DEV_PASSWORD" "DIST_P12:DIST_PASSWORD"; do
    eval "data=\$${pair%%:*}; password=\$${pair#*:}"
    printf '%s' "$data" | base64 --decode > "$RUNNER_TEMP/cert.p12"
    security import "$RUNNER_TEMP/cert.p12" -k "$keychain" -P "$password" -f pkcs12 \
        -T /usr/bin/codesign -T /usr/bin/security
    rm -P "$RUNNER_TEMP/cert.p12"
done
# Apple's WWDR intermediate (G3) for the development / distribution chain.
curl -fsSL https://www.apple.com/certificateauthority/AppleWWDRCAG3.cer -o "$RUNNER_TEMP/AppleWWDRCAG3.cer"
if ! output=$(security import "$RUNNER_TEMP/AppleWWDRCAG3.cer" -k "$keychain" 2>&1); then
    [[ "$output" == *"already exists"* ]] || { echo "$output"; exit 1; }
fi
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" >/dev/null
security list-keychains -d user -s "$keychain" $(security list-keychains -d user | tr -d '"')
security find-identity -v -p codesigning "$keychain"
echo "KEYCHAIN=$keychain" >> "$GITHUB_ENV"
printf '%s\n' "$API_KEY_P8" > "$RUNNER_TEMP/AuthKey.p8"
chmod 600 "$RUNNER_TEMP/AuthKey.p8"
