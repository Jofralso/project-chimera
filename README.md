# Project Chimera

Portable, hardware-accelerated music workstation for live performance,
experimental sound design, and modular synthesis.

**Core principle:** Everything is real-time. Nothing is batch-oriented.

## Status

Pre-alpha. Audio engine skeleton is functional and tested.

## What's Here

| Component | Location |
|---|---|
| Audio Engine | `software/audio-engine/` |
| Plugin SDK | `sdk/` |
| Tests | `tests/` |

## Quick Start

```bash
cmake -S . -B build
cmake --build build
cd build && ctest
```

For **Jetson Orin Nano** deployment, see [`docs/05_JETSON.md`](docs/05_JETSON.md).

## Requirements

- CMake 3.25+
- C++20 compiler (GCC 13+, Clang 16+)
- Linux (primary target)
- Optional: JACK audio server

## Milestones

See `docs/milestones/`.
