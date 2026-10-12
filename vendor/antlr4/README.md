# ANTLR C++ runtime 4.13.2

The generated lexer and parser in `src/generated` require ANTLR 4.13.2.
CMake uses an installed runtime only when its version is exactly 4.13.2.
Otherwise it extracts this local source archive into the build directory and
builds a static runtime, with upstream tests and demos disabled. No network
access is needed during configuration or compilation.

Official source archive:
https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip

SHA-256:
`0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4`

The archive is unmodified. Its license is also provided in `LICENSE.txt`.
