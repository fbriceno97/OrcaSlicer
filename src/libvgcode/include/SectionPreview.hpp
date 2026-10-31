// OrcaSlicer section-preview ordering. AGPL-3.0-or-later.
#pragma once

#include "PathVertex.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <limits>
#include <stdexcept>
#include <vector>

namespace libvgcode {

// Keep support-only rows even when an empty model row shares their height.
// Call after every row has contributed vertices, before appending the section.
inline void compact_section_layers(std::vector<PathVertex> &vertices, std::vector<float> &layers_zs)
{
    if (layers_zs.size() > std::numeric_limits<uint32_t>::max())
        throw std::overflow_error("Section preview exceeds the supported layer count");
    std::vector<bool> occupied(layers_zs.size(), false);
    for (const PathVertex &vertex : vertices) {
        if (vertex.layer_id >= layers_zs.size())
            throw std::invalid_argument("Section preview vertex has no layer height");
        occupied[vertex.layer_id] = true;
    }
    std::vector<uint32_t> mapping(layers_zs.size());
    std::vector<float> compact;
    compact.reserve(layers_zs.size());
    for (size_t i = 0; i < layers_zs.size(); ++i)
        if (occupied[i]) {
            mapping[i] = static_cast<uint32_t>(compact.size());
            compact.push_back(layers_zs[i]);
        }
    for (PathVertex &vertex : vertices)
        vertex.layer_id = mapping[vertex.layer_id];
    layers_zs = std::move(compact);
}

// Section-local layer indices cannot be merged by physical Z: a tilted layer
// spans several Z values, and a rebased upper section starts above local Z=0.
// Append the whole section with a disjoint range of layer indices instead.
inline uint32_t append_section_vertices(std::vector<PathVertex> &destination,
    const std::vector<PathVertex> &section, uint32_t layer_offset)
{
    if (&destination == &section)
        throw std::invalid_argument("Section preview cannot append a vector to itself");
    uint64_t next_offset = layer_offset;
    for (const PathVertex &vertex : section) {
        const uint64_t next = uint64_t(vertex.layer_id) + layer_offset + 1;
        if (next > std::numeric_limits<uint32_t>::max())
            throw std::overflow_error("Section preview exceeds the supported layer count");
        next_offset = std::max(next_offset, next);
    }
    destination.reserve(destination.size() + section.size());
    for (PathVertex vertex : section) {
        vertex.layer_id += layer_offset;
        destination.push_back(vertex);
    }
    return static_cast<uint32_t>(next_offset);
}

} // namespace libvgcode
