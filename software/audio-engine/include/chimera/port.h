#pragma once

#include "audio_buffer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace chimera {

enum class PortDirection : uint8_t {
    Input,
    Output
};

enum class PortDataType : uint8_t {
    Audio,
    Control,
    Event
};

struct PortDescriptor {
    std::string name;
    PortDirection direction;
    PortDataType data_type;
};

struct Port {
    PortDescriptor descriptor;
    AudioBuffer buffer;

    Port() = default;

    Port(PortDescriptor desc, size_t block_size)
        : descriptor(std::move(desc))
        , buffer(block_size, 1)
    {}

    const std::string& name() const { return descriptor.name; }
    PortDirection direction() const { return descriptor.direction; }
    PortDataType data_type() const { return descriptor.data_type; }
};

} // namespace chimera
