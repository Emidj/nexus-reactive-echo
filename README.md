# NEXUS Reactive Echo

An independent, audio-reactive GPU echo filter for OBS Studio on Windows x64.

**Current release: 1.2.0 beta.** [Download the installer and source](https://github.com/Emidj/nexus-reactive-echo/releases/tag/v1.2.0).

## Features

- Recursive video feedback and silhouettes behind the original source.
- Audio-driven intensity, scale, movement, feedback alpha and color.
- Independent X and Y audio offset controls, each with its own enable switch and a signed range from **-200 to +200**. Positive X moves right; positive Y moves down. Older combined-offset settings migrate to both axes.
- Built-in presets, gradient palettes, optional external `.cube` LUTs and a saved personal setting per filter instance.
- OBS properties in English, Spanish, German, French and Italian.
- A standalone Windows installer with update, repair and uninstall behavior.

Commercial LUTs are not included. The filter uses its own implementation; it does not require another NEXUS plugin or a shared NEXUS engine.

## Requirements and installation

Windows x64 and OBS Studio **32.2.2** are the locally tested baseline. Other OBS builds and GPUs require validation. The effect processes SDR sources.

1. Close OBS and run `NEXUS-Reactive-Echo-1.2.0-Windows-x64-Setup.exe`.
2. Select your OBS installation. A standard installation may require administrator permission; portable OBS must contain `portable_mode.txt`.
3. Open OBS and add **NEXUS Reactive Echo** to your source's effect filters.
4. For silhouettes behind a person, remove the background first and place this filter **after Chroma Key**. Apply a preset, select an audio source and enable reactive mode as needed.
5. To test a constant effect, disable reactive mode. Zero intensity preserves the original image.

Use the installer or installed uninstaller to remove the plugin. Scene settings and unrelated files are preserved; a modified DLL blocks uninstall to avoid removing another version.

The EXE is unsigned. Verify downloads with the Release's `SHA256SUMS.txt`. The binary ZIP is also provided for manual installation. OBS Studio itself is not bundled.

## Documentation and source

[Spanish user guide and development history](USER-GUIDE.es.md). Its 1.2.0 section describes the independent axis controls; older sections and 1.1.0 manuals may describe the previous combined control.

The source package includes C code, shader, translations, installer sources, packaging scripts and tests. It excludes commercial LUTs, camera recordings, local logs, build tools and OBS binaries.

To build on Windows, install Python 3, Zig **0.15.2**, official OBS **32.2.2** source headers and OBS Studio. Set `ZIG_EXE`, `OBS_SDK_DIR` and optionally `OBS_INSTALL_DIR`; then run:

```powershell
python build.py
powershell -File test.ps1
python package.py
python tests/installer-test.py
```

`package.py` uses the .NET Framework C# compiler supplied by Windows. `build.py` generates an OBS import library from the installed `obs.dll`; it does not bundle that DLL. A clean-machine reproducible build has not been verified.

## Validation and limits

The official 1.2.0 installer passed **40 isolated checks** again during publication preparation, including Unicode paths, reinstall, rollback, corrupted payload rejection, uninstall and preservation of unrelated files. Its embedded plugin DLL matches the official binary ZIP.

Earlier local testing exercised the real DLL with libobs/Direct3D 11 and signed X/Y control migration. That historical GPU testing was not repeated in this publication. It does not establish visual acceptance with a particular camera, all GPUs, or multi-hour streaming reliability. See [PUBLICATION-AUDIT.md](PUBLICATION-AUDIT.md).

## License

**GPL-2.0-or-later.** See [LICENSE](LICENSE), [COPYING.txt](COPYING.txt) and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). NEXUS and third-party product names do not imply affiliation with the OBS Project.

a plugin from [DjEmiMas](https://discord.gg/dhcqZPDYh)
