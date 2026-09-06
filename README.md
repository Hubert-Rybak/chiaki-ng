
![chiaki-ng Logo](gui/res/chiaking-logo.svg)

# [chiaki-ng](https://streetpea.github.io/chiaki-ng/)

An open source PlayStation remote play project serving as the next-generation of Chiaki with improvements and ongoing support now that the original Chiaki project is in maintenance mode only. [Click here to see the accompanying site for documentation, updates and more](https://streetpea.github.io/chiaki-ng/).

## Changes in this fork

Compared with [upstream chiaki-ng](https://github.com/streetpea/chiaki-ng), this fork:

- Bounds video timestamp drift during long streams to prevent accumulated playback timing errors that otherwise reset only on reconnect.
- Adds regression tests for long sessions, packet loss, jitter, stalls and frame-rate changes, plus Windows and Linux test builds.
- Builds the bundled test framework as C11 for compatibility with current Windows compilers.

The fix was reported to resolve the slowdown on the affected Windows machine. Download the portable Windows build from [this fork's releases](https://github.com/Hubert-Rybak/chiaki-ng/releases).

## Discord
[chiaki-ng community Discord](https://discord.gg/tAMbRuwXDH)

## Disclaimer
This project is not endorsed or certified by Sony Interactive Entertainment LLC.

Chiaki is a Free and Open Source Software Client for PlayStation 4 and PlayStation 5 Remote Play
for Linux, FreeBSD, OpenBSD, Android, macOS, Windows, Nintendo Switch and potentially even more platforms.
