#pragma once

#include "audio_node.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace chimera {

struct Connection {
    NodeID source_node;
    size_t source_port;
    NodeID target_node;
    size_t target_port;
};

class AudioGraph {
public:
    AudioGraph() = default;
    ~AudioGraph() = default;

    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    NodeID add_node(std::unique_ptr<AudioNode> node);
    AudioNode* node(NodeID id);
    const AudioNode* node(NodeID id) const;

    bool connect(NodeID source, size_t source_port, NodeID target, size_t target_port);
    bool disconnect(NodeID source, size_t source_port, NodeID target, size_t target_port);

    void remove_node(NodeID id);

    const std::vector<Connection>& connections() const { return connections_; }

    bool prepare(double sample_rate, size_t block_size);

    void process(size_t num_frames);

    void clear();

    std::vector<NodeID> all_node_ids() const;
    const std::unordered_map<NodeID, std::unique_ptr<AudioNode>>& all_nodes() const { return nodes_; }

    std::vector<NodeID> processing_order() const;

    bool serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const std::vector<uint8_t>& data,
                     std::function<std::unique_ptr<AudioNode>(const std::string& node_class)> factory);

    void set_next_node_id(NodeID id) { next_node_id_ = id; }
    NodeID next_node_id() const { return next_node_id_; }

private:
    std::unordered_map<NodeID, std::unique_ptr<AudioNode>> nodes_;
    std::vector<Connection> connections_;
    std::vector<NodeID> order_;
    bool order_dirty_{true};
    static NodeID next_node_id_;

    void topological_sort();
};

} // namespace chimera
