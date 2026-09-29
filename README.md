# Vocal Companion

Freeware modular vocal channel strip for post-recorded acoustic vocal, DI, and raw exports from vocal synth softwares.

This Freeware was made by Crimson Redstone — consider supporting the project
by purchasing music at [crimsonredstone.bandcamp.com](https://crimsonredstone.bandcamp.com).

## Stock modules

| Module | Notes |
|--------|--------|
| Multi-mode Gain | Pure / Clean tube / Dirty overdrive |
| Dual-band De-esser | 2.5–4 kHz synth harsh + 4–8 kHz sibilance |
| FET Peak Comp | 1176-style, 20 µs attack, All-Buttons 20:1 |
| Opto Leveler | RMS glue, 6 dB knee |
| Dynamic EQ + Air | 5 parametric bands + 10–20 kHz shelf |
| Pitch / Formant | YIN detect, scale snap, throat-length shift |
| Width | Das DAFx24 two-band decorr (velvet / allpass), Lo/Hi, transient dry-pass |
| Chorus / Phaser / Tremolo / Auto-Pan | tempo-sync LFOs |
| BPM Delay + Ducking Reverb | ping-pong delay; sidechain-ducked tails |
| External host card | scan + embed a third-party plug-in |

## Build

Builds VST3, CLAP and a standalone application on Windows, macOS and Linux,
plus Audio Unit on macOS. Includes effect cards, presets, per-card and rack
settings A/B, optional level matching, external plugin hosting, and the
Aeterna effect with optional MIDI harmony.


Requires CMake 3.22 or newer, Git, and a C++20 compiler. First configuration
fetches JUCE 9.0.1 and clap-juce-extensions from their upstream repositories.

### Windows

Install Visual Studio 2022 with **Desktop development with C++**, CMake and
Git. Run `build.bat` from the extracted repository folder. It writes a local
`build.log` and attempts to install the VST3. Use `build.bat nopause` to skip
the final keypress.

### macOS

Install Xcode Command Line Tools (`xcode-select --install`), CMake and Git.
Run `build_mac.command`; if necessary run `bash build_mac.command` in Terminal.
It builds and copies AU/VST3 bundles into your user audio-plugin directories.

### Linux

On Ubuntu/Debian, install the build dependencies:

```bash
sudo apt-get install git cmake g++ pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev
```

Build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Outputs are under `build/VocalCompanion_artefacts/Release/`. Rescan plugins
in your DAW after installation. To install the Linux standalone application,
menu launcher and icons for your user:

```bash
cmake --install build --prefix "$HOME/.local" --component Standalone
```

Ensure `$HOME/.local/bin` is on your desktop session's PATH. An additional
launcher pointing to the build-tree executable is generated alongside the
Release output folders.

### Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `VC_ENABLE_CLAP` | `ON` | Build the CLAP wrapper |
| `VC_ENABLE_LTO` | `ON` | Enable release link-time optimization |
| `VC_BUILD_UI_TESTS` | `OFF` | Build the native regression executable |

Pass options during configuration, for example `-DVC_ENABLE_CLAP=OFF`.
To run the regression suite:

```bash
cmake -S . -B build -DVC_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target VocalCompanionUiTests
```

Run `VocalCompanionUiTests` (Windows: `.exe`) from
`build/VocalCompanionUiTests_artefacts/Release/`, passing a writable output
folder for screenshots as its first argument.

## Icons

The supplied Aeterna artwork is wired into Windows resources and VST3 folder
icons, macOS bundles, and Linux standalone launchers/window icons. Windows
folder attributes are applied during configuration and installation. macOS
ICNS files are generated at build time. DAW browsers and operating-system
file associations may still choose their own icons.

## Repository layout

- `Source/`: plugin audio processing and interface.
- `Assets/`: required animation and application icons.
- `ThirdParty/`: vendored Signalsmith headers, upstream references and licenses.
- `tests/`: native regression source.
- `cmake/`: platform branding and installation rules.
- `.github/workflows/`: Windows, macOS and Linux build automation.

## License and dependencies

See [LICENSE](LICENSE) for the project's existing license. Dependencies retain
their own licenses. JUCE and clap-juce-extensions are fetched at build time;
Signalsmith license texts and pinned upstream references are included in
[ThirdParty/](ThirdParty/).

Music and project support: https://crimsonredstone.bandcamp.com
