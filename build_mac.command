#!/bin/bash
cd "$(dirname "$0")"

# Vocal Companion - macOS one-click AU / VST3 build
# Crimson Redstone / Freeware
# Double-click this file in Finder. The window stays open until you press Enter.

ROOT="$(pwd)"
FAIL=0

# Finder-launched scripts do not load Homebrew or CMake.app into PATH.
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/local/sbin:/Applications/CMake.app/Contents/bin:$PATH"

# Never leak GIT_DIR into FetchContent (reserved by Git).
unset GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR GIT_OBJECT_DIRECTORY

echo
echo "============================================================"
echo "  Vocal Companion  -  macOS plugin build"
echo "  Crimson Redstone / Freeware"
echo "  Folder: $ROOT"
echo "============================================================"
echo

if [ ! -f "$ROOT/CMakeLists.txt" ]; then
    echo "ERROR: CMakeLists.txt is not next to build_mac.command."
    echo "  Extract the ZIP to a folder first, then double-click"
    echo "  build_mac.command inside VocalCompanion."
    echo
    read -p "Press [Enter] to exit..."
    exit 1
fi

# ---- tools ------------------------------------------------------------------
echo "[1/5] Tools"

MISSING=0

if ! xcode-select -p >/dev/null 2>&1; then
    echo
    echo "ERROR: Xcode Command Line Tools are not installed."
    echo
    echo "  1. Open Terminal and run:"
    echo "       xcode-select --install"
    echo "  2. Finish the installer dialog"
    echo "  3. Double-click build_mac.command again"
    echo
    MISSING=1
else
    echo "      Xcode CLT: $(xcode-select -p)"
fi

CMAKE_BIN="$(command -v cmake 2>/dev/null || true)"
if [ -z "$CMAKE_BIN" ] && [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
    CMAKE_BIN="/Applications/CMake.app/Contents/bin/cmake"
fi
if [ -z "$CMAKE_BIN" ]; then
    echo
    echo "ERROR: CMake is not installed, or it is not on PATH."
    echo
    echo "  Install one of:"
    echo "    brew install cmake"
    echo "    https://cmake.org/download/   (macOS installer / CMake.app)"
    echo
    echo "  Then double-click build_mac.command again."
    echo
    MISSING=1
else
    echo "      CMake:     $CMAKE_BIN"
    "$CMAKE_BIN" --version | head -n 1
fi

if [ "$MISSING" -ne 0 ]; then
    echo
    echo "============================================================"
    echo "  BUILD FAILED  -  install the tools above, then re-run"
    echo "============================================================"
    echo
    read -p "Press [Enter] to exit..."
    exit 1
fi

if ! command -v git >/dev/null 2>&1; then
    echo
    echo "ERROR: Git is not installed. CMake needs Git to download JUCE."
    echo "  Xcode Command Line Tools include Git. Run:"
    echo "       xcode-select --install"
    echo "  or:  brew install git"
    echo
    echo "============================================================"
    echo "  BUILD FAILED"
    echo "============================================================"
    echo
    read -p "Press [Enter] to exit..."
    exit 1
fi
echo "      Git:       $(command -v git)"
echo

# Drop a half-cloned JUCE tree from a previous failed run.
if [ ! -d "$ROOT/build/_deps/juce-src/.git" ] && [ -d "$ROOT/build/_deps" ]; then
    echo "Removing incomplete JUCE download from a previous run..."
    rm -rf "$ROOT/build/_deps"
fi

# ---- configure --------------------------------------------------------------
echo "[2/5] Configuring CMake (Release)"
echo "      First run downloads JUCE 9.0.1. This can take several minutes."
echo "      Do not close this window."
echo

mkdir -p "$ROOT/build"

"$CMAKE_BIN" -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo
    echo "CMake configure failed."
    echo "If GitHub is unreachable, check the network, delete the build"
    echo "folder, and retry."
    FAIL=1
fi

# ---- compile ----------------------------------------------------------------
if [ "$FAIL" -eq 0 ]; then
    echo
    echo "[3/5] Compiling Release. This takes several minutes..."
    echo
    CMAKE_DIR="$(dirname "$CMAKE_BIN")"
    export PATH="$CMAKE_DIR:$PATH"
    (
        cd "$ROOT/build" || exit 1
        cmake --build . --config Release --parallel
    )
    if [ $? -ne 0 ]; then
        echo
        echo "Compile failed. Scroll up for the first error."
        FAIL=1
    fi
fi

# ---- locate + install (user Library, no sudo) -------------------------------
if [ "$FAIL" -eq 0 ]; then
    echo
    echo "[4/5] Locating AU (.component) and VST3 (.vst3) bundles..."

    AU_DST="$HOME/Library/Audio/Plug-Ins/Components"
    VST3_DST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$AU_DST"
    mkdir -p "$VST3_DST"

    AU_COUNT=0
    VST3_COUNT=0

    # Last copy wins. Paths containing /Release/ sort after the non-Release
    # twin, so the Release artefact is what ends up in the plugin folder.
    TMP_LIST="/tmp/vc-mac-bundles.$$"
    find "$ROOT/build" \
        \( -name "*.component" -o -name "*.vst3" \) \
        ! -path "*/_deps/*" \
        ! -path "*/CMakeFiles/*" \
        2>/dev/null | sort > "$TMP_LIST"

    echo
    echo "[5/5] Installing into your user plugin folders (no admin password)"
    echo "      AU:   $AU_DST"
    echo "      VST3: $VST3_DST"
    echo

    while IFS= read -r bundle || [ -n "$bundle" ]; do
        [ -e "$bundle" ] || continue
        name="$(basename "$bundle")"
        case "$name" in
            *.component)
                echo "  AU    $name"
                echo "        from $bundle"
                rm -rf "$AU_DST/$name"
                cp -R "$bundle" "$AU_DST/"
                if [ $? -eq 0 ]; then
                    AU_COUNT=$((AU_COUNT + 1))
                else
                    echo "        COPY FAILED"
                    FAIL=1
                fi
                ;;
            *.vst3)
                echo "  VST3  $name"
                echo "        from $bundle"
                rm -rf "$VST3_DST/$name"
                cp -R "$bundle" "$VST3_DST/"
                if [ $? -eq 0 ]; then
                    VST3_COUNT=$((VST3_COUNT + 1))
                else
                    echo "        COPY FAILED"
                    FAIL=1
                fi
                ;;
        esac
    done < "$TMP_LIST"
    rm -f "$TMP_LIST"

    if [ "$AU_COUNT" -eq 0 ] && [ "$VST3_COUNT" -eq 0 ]; then
        echo "  No .component or .vst3 found under build/"
        echo "  Search $ROOT/build for plugin bundles and copy them by hand."
        FAIL=1
    fi
fi

echo
echo "============================================================"
if [ "$FAIL" -eq 0 ]; then
    echo "  BUILD OK"
    echo "  AU installed:   $AU_COUNT bundle(s) ->"
    echo "    $HOME/Library/Audio/Plug-Ins/Components/"
    echo "  VST3 installed: $VST3_COUNT bundle(s) ->"
    echo "    $HOME/Library/Audio/Plug-Ins/VST3/"
    echo
    echo "  Rescan plug-ins in your DAW (Logic, Ableton, Reaper, ...)."
    echo "  Logic Pro: if macOS blocks the AU, allow it in"
    echo "  System Settings -> Privacy & Security."
else
    echo "  BUILD FAILED"
    echo "  Folder: $ROOT"
fi
echo "============================================================"
echo
read -p "Press [Enter] to exit..."
exit $FAIL
