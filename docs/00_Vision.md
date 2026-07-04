# Chimera Vision

Chimera is a portable, hardware-accelerated music workstation designed for:

- live performance
- experimental sound design
- modular synthesis
- sample-based composition
- audiovisual integration

## Core Principle

Everything is real-time. Nothing is batch-oriented.

## Design Constraints

- Latency must remain under 10ms end-to-end
- System must remain usable under CPU saturation
- All features must be modular and removable
- Hardware-first interaction (knobs, pads, sensors)

## Target Devices

- Raspberry Pi 5 main system
- RP2040 microcontrollers for I/O
- ESP32 for wireless / control layer
- External USB audio interfaces

## Philosophy

Chimera is not a DAW.

It is a **performance instrument that can behave like a DAW when needed**.
