# ADR-0002: Audio Backend Choice

## Status
Accepted — Engine design supports JACK/PipeWire with dummy fallback.

## Context
We need a low-latency audio system for real-time performance.

## Decision
Use JACK / PipeWire depending on system availability, with an abstracted
backend interface. The Engine class currently uses a timer-driven dummy
backend; real backends will implement the process callback.

## Consequences

Positive:
- Low latency on supported systems
- Linux native integration
- Flexible routing
- Dummy backend enables development without audio hardware
- Future: ALSA direct backend for systems without JACK/PipeWire

Negative:
- Setup complexity for JACK
- Platform dependency constraints

## Alternatives Considered

- ALSA direct (too low-level for routing)
- PortAudio (too abstract for routing control)

## Implementation

The backend is selected at CMake configure time:
- `CHIMERA_USE_JACK=ON` — links JACK, defines CHIMERA_HAS_JACK
- `CHIMERA_USE_PIPEWIRE=ON` — links PipeWire, defines CHIMERA_HAS_PIPEWIRE
- Neither — defines CHIMERA_USE_DUMMY

Current code uses the dummy backend. JACK/PipeWire backends are planned for M1.
