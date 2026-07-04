#include "chimera/audio_graph.h"

#include <algorithm>
#include <cstring>
#include <queue>
#include <stdexcept>
#include <functional>

namespace chimera {

NodeID AudioGraph::next_node_id_ = 1;

NodeID AudioGraph::add_node(std::unique_ptr<AudioNode> node) {
    NodeID id = node->id();
    nodes_[id] = std::move(node);
    order_dirty_ = true;
    return id;
}

AudioNode* AudioGraph::node(NodeID id) {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

const AudioNode* AudioGraph::node(NodeID id) const {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

bool AudioGraph::connect(NodeID source_id, size_t source_port,
                         NodeID target_id, size_t target_port) {
    auto* src = node(source_id);
    auto* dst = node(target_id);
    if (!src || !dst) return false;
    if (source_port >= src->num_outputs()) return false;
    if (target_port >= dst->num_inputs()) return false;

    Connection conn{source_id, source_port, target_id, target_port};
    connections_.push_back(conn);
    order_dirty_ = true;
    return true;
}

bool AudioGraph::disconnect(NodeID source_id, size_t source_port,
                            NodeID target_id, size_t target_port) {
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [&](const Connection& c) {
            return c.source_node == source_id &&
                   c.source_port == source_port &&
                   c.target_node == target_id &&
                   c.target_port == target_port;
        });

    if (it == connections_.end()) return false;
    connections_.erase(it);
    order_dirty_ = true;
    return true;
}

void AudioGraph::remove_node(NodeID id) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [id](const Connection& c) {
                return c.source_node == id || c.target_node == id;
            }),
        connections_.end());

    nodes_.erase(id);
    order_dirty_ = true;
}

bool AudioGraph::prepare(double sample_rate, size_t block_size) {
    for (auto& [id, node] : nodes_) {
        node->prepare(sample_rate, block_size);
    }
    topological_sort();
    return true;
}

void AudioGraph::process(size_t num_frames) {
    if (order_dirty_) {
        topological_sort();
    }

    for (NodeID id : order_) {
        auto* n = node(id);
        if (n) {
            n->process(num_frames);
        }
    }
}

void AudioGraph::clear() {
    nodes_.clear();
    connections_.clear();
    order_.clear();
    order_dirty_ = false;
}

std::vector<NodeID> AudioGraph::processing_order() const {
    return order_;
}

std::vector<NodeID> AudioGraph::all_node_ids() const {
    std::vector<NodeID> ids;
    ids.reserve(nodes_.size());
    for (auto& [id, _] : nodes_) {
        ids.push_back(id);
    }
    return ids;
}

namespace {

template<typename T>
void write_pod(std::vector<uint8_t>& buf, const T& val) {
    auto* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template<typename T>
bool read_pod(const uint8_t*& ptr, size_t& remain, T& val) {
    if (remain < sizeof(T)) return false;
    std::memcpy(&val, ptr, sizeof(T));
    ptr += sizeof(T);
    remain -= sizeof(T);
    return true;
}

void write_str(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    write_pod(buf, len);
    buf.insert(buf.end(), s.data(), s.data() + len);
}

bool read_str(const uint8_t*& ptr, size_t& remain, std::string& out) {
    uint32_t len;
    if (!read_pod(ptr, remain, len)) return false;
    if (remain < len) return false;
    out.assign(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    remain -= len;
    return true;
}

} // namespace

bool AudioGraph::serialize(std::vector<uint8_t>& out) const {
    uint32_t n_nodes = static_cast<uint32_t>(nodes_.size());
    write_pod(out, n_nodes);

    for (auto& [id, node] : nodes_) {
        write_pod(out, id);
        uint8_t type = static_cast<uint8_t>(node->type());
        write_pod(out, type);
        write_str(out, node->node_class());
        write_str(out, node->name());

        uint32_t ni = static_cast<uint32_t>(node->num_inputs());
        uint32_t no = static_cast<uint32_t>(node->num_outputs());
        write_pod(out, ni);

        for (size_t i = 0; i < node->num_inputs(); ++i) {
            auto* p = node->input(i);
            write_str(out, p->name());
            uint8_t dt = static_cast<uint8_t>(p->data_type());
            write_pod(out, dt);
        }

        write_pod(out, no);
        for (size_t i = 0; i < node->num_outputs(); ++i) {
            auto* p = node->output(i);
            write_str(out, p->name());
            uint8_t dt = static_cast<uint8_t>(p->data_type());
            write_pod(out, dt);
        }
    }

    uint32_t n_conns = static_cast<uint32_t>(connections_.size());
    write_pod(out, n_conns);
    for (auto& c : connections_) {
        write_pod(out, c.source_node);
        write_pod(out, static_cast<uint32_t>(c.source_port));
        write_pod(out, c.target_node);
        write_pod(out, static_cast<uint32_t>(c.target_port));
    }

    return true;
}

bool AudioGraph::deserialize(
    const std::vector<uint8_t>& data,
    std::function<std::unique_ptr<AudioNode>(const std::string&)> factory)
{
    const uint8_t* ptr = data.data();
    size_t remain = data.size();

    uint32_t n_nodes = 0;
    if (!read_pod(ptr, remain, n_nodes)) return false;

    NodeID max_id = 0;

    for (uint32_t i = 0; i < n_nodes; ++i) {
        NodeID id;
        uint8_t type;
        std::string node_class;
        std::string name;

        if (!read_pod(ptr, remain, id)) return false;
        if (!read_pod(ptr, remain, type)) return false;
        if (!read_str(ptr, remain, node_class)) return false;
        if (!read_str(ptr, remain, name)) return false;

        uint32_t ni, no;
        if (!read_pod(ptr, remain, ni)) return false;

        std::vector<PortDescriptor> in_descs(ni);
        for (uint32_t j = 0; j < ni; ++j) {
            std::string pname;
            uint8_t pdt;
            if (!read_str(ptr, remain, pname)) return false;
            if (!read_pod(ptr, remain, pdt)) return false;
            in_descs[j] = {std::move(pname), PortDirection::Input, static_cast<PortDataType>(pdt)};
        }

        if (!read_pod(ptr, remain, no)) return false;
        std::vector<PortDescriptor> out_descs(no);
        for (uint32_t j = 0; j < no; ++j) {
            std::string pname;
            uint8_t pdt;
            if (!read_str(ptr, remain, pname)) return false;
            if (!read_pod(ptr, remain, pdt)) return false;
            out_descs[j] = {std::move(pname), PortDirection::Output, static_cast<PortDataType>(pdt)};
        }

        auto node = factory(node_class);
        if (!node) return false;

        node->release();
        nodes_[id] = std::move(node);
        auto* n = nodes_[id].get();

        for (auto& d : in_descs) n->add_input(std::move(d));
        for (auto& d : out_descs) n->add_output(std::move(d));

        if (id > max_id) max_id = id;
    }

    uint32_t n_conns = 0;
    if (!read_pod(ptr, remain, n_conns)) return false;

    connections_.clear();
    for (uint32_t i = 0; i < n_conns; ++i) {
        Connection c{};
        uint32_t sp, tp;
        if (!read_pod(ptr, remain, c.source_node)) return false;
        if (!read_pod(ptr, remain, sp)) return false;
        if (!read_pod(ptr, remain, c.target_node)) return false;
        if (!read_pod(ptr, remain, tp)) return false;
        c.source_port = sp;
        c.target_port = tp;
        connections_.push_back(c);
    }

    next_node_id_ = max_id + 1;
    order_dirty_ = true;
    return true;
}

void AudioGraph::topological_sort() {
    order_.clear();
    order_.reserve(nodes_.size());

    std::unordered_map<NodeID, int> in_degree;
    std::unordered_multimap<NodeID, NodeID> adj;

    for (auto& [id, _] : nodes_) {
        in_degree[id] = 0;
    }

    for (auto& conn : connections_) {
        adj.emplace(conn.source_node, conn.target_node);
        in_degree[conn.target_node]++;
    }

    std::queue<NodeID> q;
    for (auto& [id, deg] : in_degree) {
        if (deg == 0) q.push(id);
    }

    while (!q.empty()) {
        NodeID id = q.front();
        q.pop();
        order_.push_back(id);

        auto [begin, end] = adj.equal_range(id);
        for (auto it = begin; it != end; ++it) {
            if (--in_degree[it->second] == 0) {
                q.push(it->second);
            }
        }
    }

    order_dirty_ = false;
}

} // namespace chimera
