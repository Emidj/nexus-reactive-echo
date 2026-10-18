# Dependencies and distribution

NEXUS Reactive Echo's C sources and installer declare **GPL-2.0-or-later**. The full GPLv2 text is supplied in LICENSE and COPYING.txt. Original SPDX notices are preserved.

The plugin uses the OBS/libobs API. OBS Studio is a separate installed prerequisite; the Release does not redistribute OBS Studio, its headers, Qt, Bonjour or a NEXUS shared engine. Windows system libraries are supplied by Windows.

Build tools: Zig 0.15.2, Python 3 and the Windows .NET Framework C# compiler. These tools are not bundled in the installer or binary ZIP. Install the tools and OBS headers separately from their official sources when rebuilding.

The shader is embedded when compiling. The binary distribution consists of the plugin DLL, five locale files, installer documentation and GPL license. The installer also embeds its own uninstaller. External commercial LUTs are neither copied nor redistributed.

Brand graphics are the existing NEXUS resources supplied with the project. Software licensing does not grant rights to third-party trademarks. OBS Studio is a product of the OBS Project; no endorsement is claimed.

References:

- https://github.com/obsproject/obs-studio
- https://obsproject.com/help
- https://ziglang.org/download/
