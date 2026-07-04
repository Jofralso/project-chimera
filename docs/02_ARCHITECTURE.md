# System Architecture

## Hardware Topology

```mermaid
graph TB
    subgraph RPi5["Raspberry Pi 5 (Main Compute)"]
        AE[Audio Engine]
        PH[Plugin Host]
        SM[Session Manager]
        APP[Application Layer]
        UI[UI Framework]
        LOG[Logger]
    end

    subgraph RP2040["RP2040 (Low-latency I/O)"]
        BTN[Buttons / Encoders]
        LED[LED Grid]
        OLED[OLED Display]
    end

    subgraph ESP32["ESP32 (Wireless)"]
        BLE[BLE MIDI]
        OSC[OSC Server]
        WIFI[Wi-Fi Config]
    end

    subgraph ZERO["Pi Zero + E-Ink"]
        STATUS[Status Display]
    end

    RPi5 <-->|GPIO / SPI / I2C| RP2040
    RPi5 <-->|UART / Wi-Fi| ESP32
    RPi5 -->|HDMI| DISPLAY[7" TFT Touch]
    RPi5 <-->|USB| AUDIO[USB Audio Interface]
    RPi5 -->|eInk HAT| ZERO
```

## Software Stack

```mermaid
graph TB
    subgraph USER["User Space"]
        UI[UI Layer]
        APP[Application / chimera-play]
    end

    subgraph CORE["Chimera Core"]
        SM[Session Manager]
        direction TB
        AE[Audio Engine]
        GRAPH[AudioGraph]
        NODES[AudioNodes]
        PH[Plugin Host]
        LOG[Logger]
        AE --> GRAPH
        GRAPH --> NODES
        PH --> NODES
    end

    subgraph SDK["Plugin SDK"]
        ABI[Plugin ABI - plugin.h]
        PLUGINS[User Plugins .so]
    end

    subgraph HAL["Hardware Abstraction"]
        AB[AudioBackend]
        ALSA[ALSA Backend]
        DUMMY[Dummy Backend]
        JACK[JACK Backend - planned]
        AB --> ALSA
        AB --> DUMMY
        AB --> JACK
    end

    subgraph HW["Hardware / OS"]
        LINUX[Linux Kernel]
        ALIB[ALSA / JACK / PipeWire]
        GPIO[GPIO / SPI / I2C]
    end

    USER -->|commands| SM
    USER -->|commands| AE
    APP <--> AE
    SM -->|graph serialize/deserialize| AE
    AE <--> AB
    ABI -->|dlopen| PH
    PLUGINS --> ABI
    ALSA --> ALIB
    JACK --> ALIB
    ALIB --> LINUX
    LINUX --> GPIO
```

## Core Architecture

```mermaid
classDiagram
    class Engine {
        +AudioGraph graph
        +AudioBackend* backend
        +RingBuffer~EngineMessage~ control_queue
        +TransportState state
        +double sample_rate
        +uint32_t block_size
        +start()
        +stop()
        +pause()
        +resume()
        +audio_callback()
    }

    class AudioGraph {
        +map~NodeID, unique_ptr~AudioNode~~ nodes
        +vector~Connection~ connections
        +bool topo_dirty
        +process()
        +add_node()
        +remove_node()
        +connect()
        +disconnect()
    }

    class AudioNode {
        <<abstract>>
        +vector~Port~ inputs
        +vector~Port~ outputs
        +NodeID id
        +string node_class
        +prepare(sample_rate, block_size)
        +process(num_frames)
        +release()
    }

    class Port {
        +PortDirection direction
        +PortType type
        +AudioBuffer buffer
    }

    class AudioBuffer {
        +float* data
        +uint32_t capacity
        +uint32_t num_channels
    }

    class RingBuffer~T~ {
        +push(item)
        +pop() optional~T~
        +reset()
    }

    class AudioBackend {
        <<interface>>
        +init(sample_rate, block_size, config)
        +start()
        +stop()
        +is_running()
        +set_callback(callback)
    }

    class TransportState {
        <<enum>>
        Stopped
        Playing
        Paused
        Loop
    }

    Engine --> AudioGraph
    Engine --> AudioBackend
    Engine --> RingBuffer~EngineMessage~
    AudioGraph --> AudioNode
    AudioNode --> Port
    Port --> AudioBuffer
    AudioBackend -->|callback| Engine
```

## Data Flow (Audio Cycle)

```mermaid
sequenceDiagram
    participant B as AudioBackend
    participant E as Engine
    participant Q as RingBuffer
    participant G as AudioGraph
    participant N as AudioNode
    participant O as MasterOutput

    loop Every Block
        B->>E: audio_callback(inputs, outputs, frames)
        E->>Q: drain control messages
        E->>E: apply mutations<br/>(connect/disconnect/set_param)
        E->>E: find AudioInputNode
        E->>E: copy backend capture → AudioInputNode output buffers
        E->>G: process(frames)
        G->>N: process(frames) for each node in topo order
        N->>N: read input ports → process → write output ports
        G->>O: processed audio ready
        E->>E: find MasterOutputNode
        E->>E: copy MasterOutputNode input buffers → backend playback
        E->>B: return
    end
```

## Threading Model

```mermaid
graph LR
    subgraph RT["Real-time (Audio Thread)"]
        AB[AudioBackend Callback]
        AE[Engine::audio_callback]
        GP[AudioGraph::process]
    end

    subgraph CT["Control Thread"]
        APP[Application / UI]
        SM[Session Manager]
        CQ[push EngineMessage]
    end

    subgraph L["Lock-free Bridge"]
        RB[RingBuffer~EngineMessage~]
    end

    CT -->|push| RB
    AB -->|pop| RB
    AB --> AE
    AE --> GP
    APP <-->|mutex| SM
    APP -->|mutex| AE.add_node/remove_node
```

## Key Principles

- **Zero allocations in the audio thread**: all buffers pre-allocated in `prepare()`
- **Lock-free control channel**: `RingBuffer<EngineMessage>` for cross-thread commands
- **Topological sort on dirty flag**: graph re-orders on connection change, cached until next mutation
- **No RTTI, no exceptions, no C++ name mangling across plugin boundary**: pure C ABI
- **Backend abstraction**: `AudioBackend` interface allows ALSA, JACK, PipeWire, Dummy without changing Engine

---

## Build System

```mermaid
graph TB
    subgraph CMake["CMake Superbuild (3.25+, C++20)"]
        AE[chimera_audio_engine - static lib]
        TESTS[Test Targets]
        CLI[chimera-play - CLI app]
        PLUGIN[chimera_gain_plugin - example .so]
    end

    subgraph DEPS["Dependencies"]
        ALSA[ALSA - system]
        DL[dlopen - system]
    end

    AE --> TESTS
    AE --> CLI
    AE -->|links| ALSA
    AE -->|links| DL
    PLUGIN -->|links| DL
```
