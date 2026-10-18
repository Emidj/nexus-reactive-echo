# Publication audit — 18 October 2026

## FACT

- Latest local artifacts identify version **1.2.0 beta**. Publication is marked as a prerelease.
- The C sources from the original 1.2.0 source archive match the current project's C source files byte for byte.
- Official installer SHA256: `F65789D1F2DABA9E89D2A5091054DC10BEF01E0E2F777AE516E9ED4B3C97A05D`.
- Official binary ZIP SHA256: `9600C9B8F90C1A97944018910D18D32E74EE88779CB5F1945A2096147287032C`.
- Embedded plugin DLL SHA256: `EF19E5AE1A4A42A480BE8B5A40488E046E37028E42F4EB397AA74667B87B15E3`.
- The existing installer suite was run again against the actual EXE in fresh, isolated portable fixtures: **40 checks PASS**. It does not modify the usual OBS installation or scenes.
- The source scan found no matches for the checked GitHub/AWS/Slack credential patterns, private-key headers or the author's personal development path. Pattern matching is not a complete security audit.
- GPL-2.0-or-later source headers and the original license text are retained. No commercial LUTs, camera recordings, scene collections, personal profiles or build runtimes are published.
- Publication adds README, license alias, dependency notices and this audit. Build and test scripts use environment-derived installation paths instead of fixed Windows drive paths. Historical documentation omits the author's audio-device model and CPU/GPU/driver details. The original C sources and shader are unchanged. The official EXE and binary ZIP remain unchanged. The public source ZIP is regenerated from the reviewed Git files, excluding generated test evidence and builds.

## INFERENCE

The package is suitable for publishing as a beta with the stated limitations. Installer checks establish packaging behavior, not rendering quality or live-stream reliability.

## NOT REVALIDATED HERE

Historical notes record real DLL/libobs/Direct3D 11 tests and X/Y migration checks. Publication does not repeat that full GPU test suite or establish GUI save/reopen acceptance for every new control. Other GPUs, other OBS versions, multi-hour streams and visual acceptance with the user's camera remain separate validation tasks. No clean-machine reproducible build is claimed.
