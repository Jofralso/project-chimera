# ADR-0003: ALSA Audio Backend

## Status
Accepted — implemented in M1.

## Context
We need a low-latency audio backend that works on all Linux systems
without requiring a running server (unlike JACK/PipeWire). The target
platform (Raspberry Pi 5) has ALSA as the kernel-level audio layer.

## Decision
Implement an ALSA backend as the primary audio output path, with
automatic fallback to Dummy when ALSA is unavailable.

Design principles:
- **Pluggable AudioBackend interface** — Engine delegates to backend
- **Auto-detection** — `EngineConfig::backend = "auto"` selects ALSA
  if compiled, else Dummy
- **Blocking write model** — `snd_pcm_writei` in a dedicated thread;
  the ALSA buffer period paces the audio thread naturally
- **xrun recovery** — `snd_pcm_recover` on underrun/overrun
- **Float format preferred** — falls back to S16_LE if unavailable

## Consequences

Positive:
- Works on every Linux system without server setup
- Sub-millisecond latency with small period sizes
- No external dependencies beyond libasound2-dev
- ALSA is the native audio layer on the target RPi 5

Negative:
- No dynamic routing (unlike JACK)
- Single-device binding (re-open to switch devices)
- no hardware monitoring or JACK-style ports

## Implementation

- `AudioBackend` abstract base class in `audio_backend.h`
- `AlsaBackend` in `backends/alsa_backend.h/.cpp`
- `DummyBackend` in `backends/dummy_backend.h/.cpp`
- Engine selects backend via `create_backend()`
- CMake: `FindALSA.cmake` with local `.deb` extraction fallback
- Build: `CHIMERA_HAS_ALSA` define when ALSA is available
