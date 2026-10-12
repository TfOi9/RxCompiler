# ANTLR C++ runtime 4.13.2

The generated lexer and parser in `src/generated` require ANTLR 4.13.2.
CMake uses an installed runtime only when its version is exactly 4.13.2.
Otherwise it builds `runtime-4.13.2` directly with `add_subdirectory` and links
the static runtime, with upstream tests and demos disabled. No archive
extraction or network access is needed during configuration or compilation.

Official source archive:
https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip

Original archive SHA-256:
`0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4`

The vendored directory contains the unmodified upstream runtime sources,
CMake build files, version, README, and license extracted from that archive.
Upstream demos, tests, and packaging helpers are omitted because they are
disabled in the project build. The license is also provided in `LICENSE.txt`.
