#pragma once

#include "port.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace chimera {

using NodeID = uint32_t;

enum class NodeType : uint8_t {
    Source,
    Processor,
    Sink
};

class AudioNode {
public:
    AudioNode(std::string name, NodeType type, NodeID id = 0);
    virtual ~AudioNode() = default;

    AudioNode(const AudioNode&) = delete;
    AudioNode& operator=(const AudioNode&) = delete;
    AudioNode(AudioNode&&) = delete;
    AudioNode& operator=(AudioNode&&) = delete;

    NodeID id() const { return id_; }
    const std::string& name() const { return name_; }
    NodeType type() const { return type_; }

    size_t num_inputs() const { return inputs_.size(); }
    size_t num_outputs() const { return outputs_.size(); }

    Port* input(size_t index);
    Port* output(size_t index);
    const Port* input(size_t index) const;
    const Port* output(size_t index) const;

    Port* add_input(PortDescriptor desc);
    Port* add_output(PortDescriptor desc);

    virtual void process(size_t num_frames) = 0;
    virtual void prepare(double sample_rate, size_t block_size);
    virtual void release();

    virtual std::string node_class() const = 0;

protected:
    std::vector<Port> inputs_;
    std::vector<Port> outputs_;

private:
    static NodeID next_id_;
    NodeID id_;
    std::string name_;
    NodeType type_;
};

} // namespace chimera
