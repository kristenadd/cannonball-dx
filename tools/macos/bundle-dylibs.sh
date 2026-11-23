#!/bin/bash
# Make a macOS .app self-contained: copy every non-system dylib the executable
# needs (recursively) into Contents/Frameworks, rewrite load commands to
# @rpath/<name>, replace the executable's rpaths with
# @executable_path/../Frameworks, and ad-hoc re-sign everything.
#
# Usage: bundle-dylibs.sh "/path/to/App.app" [codesign-identity]
#   codesign-identity defaults to "-" (ad-hoc).

set -euo pipefail

app="${1:?usage: $0 /path/to/App.app [codesign-identity]}"
identity="${2:--}"
contents="$app/Contents"
frameworks="$contents/Frameworks"

exe_name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$contents/Info.plist")
exe="$contents/MacOS/$exe_name"
[ -f "$exe" ] || { echo "bundle-dylibs: executable not found: $exe" >&2; exit 1; }

mkdir -p "$frameworks"

is_system() {
    case "$1" in
        /usr/lib/*|/System/*) return 0 ;;
        *) return 1 ;;
    esac
}

# Print the dependencies of a Mach-O file (skipping its own install name).
deps_of() {
    local file="$1" own
    own=$(otool -D "$file" | sed -n '2p')
    otool -L "$file" | tail -n +2 | awk '{print $1}' | while read -r dep; do
        [ -n "$own" ] && [ "$dep" = "$own" ] && continue
        echo "$dep"
    done
}

rpaths_of() {
    otool -l "$1" | awk '/cmd LC_RPATH/{getline; getline; print $2}'
}

# Resolve @rpath/@loader_path/@executable_path references to a real file.
resolve() {
    local ref="$1" from="$2" rp cand
    case "$ref" in
        @rpath/*)
            for rp in $(rpaths_of "$from") $(rpaths_of "$exe"); do
                rp="${rp//@loader_path/$(dirname "$from")}"
                rp="${rp//@executable_path/$(dirname "$exe")}"
                cand="$rp/${ref#@rpath/}"
                [ -f "$cand" ] && { echo "$cand"; return; }
            done ;;
        @loader_path/*)     cand="$(dirname "$from")/${ref#@loader_path/}"; [ -f "$cand" ] && echo "$cand" ;;
        @executable_path/*) cand="$(dirname "$exe")/${ref#@executable_path/}"; [ -f "$cand" ] && echo "$cand" ;;
        *) [ -f "$ref" ] && echo "$ref" ;;
    esac
}

# Breadth-first walk: queue holds Mach-O files whose deps still need fixing.
queue=("$exe")
seen=" "
while [ ${#queue[@]} -gt 0 ]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")

    while read -r dep; do
        [ -z "$dep" ] && continue
        is_system "$dep" && continue
        name=$(basename "$dep")
        bundled="$frameworks/$name"

        if [[ "$seen" != *" $name "* ]]; then
            src=$(resolve "$dep" "$file")
            if [ -z "$src" ]; then
                # Already rewritten on a previous build and present in Frameworks.
                if [ -f "$bundled" ]; then
                    src="$bundled"
                else
                    echo "bundle-dylibs: cannot resolve $dep (needed by $file)" >&2
                    exit 1
                fi
            fi
            # Compare canonical paths: an @rpath lookup can return
            # MacOS/../Frameworks/<name>, which is the bundled file itself.
            src="$(cd "$(dirname "$src")" && pwd -P)/$(basename "$src")"
            if [ "$src" != "$(cd "$frameworks" && pwd -P)/$name" ]; then
                # Resolve symlinks so we copy the real library under the linked name.
                cp -fL "$src" "$bundled"
                chmod u+w "$bundled"
            fi
            install_name_tool -id "@rpath/$name" "$bundled" 2>/dev/null
            seen="$seen$name "
            queue+=("$bundled")
        fi

        if [ "$dep" != "@rpath/$name" ]; then
            install_name_tool -change "$dep" "@rpath/$name" "$file" 2>/dev/null
        fi
    done < <(deps_of "$file")
done

# Executable: drop build-machine rpaths, look only inside the bundle.
for rp in $(rpaths_of "$exe"); do
    [ "$rp" = "@executable_path/../Frameworks" ] && continue
    install_name_tool -delete_rpath "$rp" "$exe" 2>/dev/null
done
if ! rpaths_of "$exe" | grep -qx "@executable_path/../Frameworks"; then
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$exe" 2>/dev/null
fi

# install_name_tool invalidates signatures; arm64 refuses to load unsigned code.
for lib in "$frameworks"/*.dylib; do
    [ -e "$lib" ] || continue
    codesign --force --sign "$identity" --timestamp=none "$lib" >/dev/null 2>&1
done
codesign --force --sign "$identity" --timestamp=none "$app" >/dev/null 2>&1

echo "bundle-dylibs: bundled$(echo "$seen" | tr -s ' ' | sed 's/ $//') into Contents/Frameworks"
