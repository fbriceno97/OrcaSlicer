#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "libvgcode/include/PathVertex.hpp"
#include "libvgcode/include/SectionPreview.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
libvgcode::PathVertex preview_vertex(float z, uint32_t layer)
{
    libvgcode::PathVertex vertex;
    vertex.position = {1.f, 2.f, z};
    vertex.layer_id = layer;
    return vertex;
}
}

TEST_CASE("Section preview keeps upper and tilted vertices in local layer order", "[SectionPreview][Nonplanar]")
{
    const std::vector<libvgcode::PathVertex> root {preview_vertex(2.f, 0), preview_vertex(1.f, 0), preview_vertex(3.f, 1)};
    const std::vector<libvgcode::PathVertex> upper {preview_vertex(22.f, 0), preview_vertex(18.f, 0), preview_vertex(24.f, 2)};
    std::vector<libvgcode::PathVertex> combined;
    const uint32_t root_end = libvgcode::append_section_vertices(combined, root, 0);
    CHECK(root_end == 2);
    CHECK(libvgcode::append_section_vertices(combined, upper, root_end) == 5);
    REQUIRE(combined.size() == root.size() + upper.size());
    for (size_t i = 0; i < root.size(); ++i) {
        CHECK(combined[i].layer_id == root[i].layer_id);
        CHECK_THAT(combined[i].position[2], Catch::Matchers::WithinAbs(root[i].position[2], 1e-6));
    }
    for (size_t i = 0; i < upper.size(); ++i) {
        CHECK(combined[root.size() + i].layer_id == upper[i].layer_id + root_end);
        CHECK_THAT(combined[root.size() + i].position[2], Catch::Matchers::WithinAbs(upper[i].position[2], 1e-6));
    }
}

TEST_CASE("Empty preview sections do not consume a layer range", "[SectionPreview][Nonplanar]")
{
    std::vector<libvgcode::PathVertex> vertices {preview_vertex(1.f, 0)};
    CHECK(libvgcode::append_section_vertices(vertices, {}, 1) == 1);
    CHECK(vertices.size() == 1);
}

TEST_CASE("Invalid section preview appends fail without changing vertices", "[SectionPreview][Nonplanar]")
{
    std::vector<libvgcode::PathVertex> vertices {preview_vertex(1.f, 0)};
    const std::vector<libvgcode::PathVertex> section {preview_vertex(2.f, 0)};
    CHECK_THROWS_AS(libvgcode::append_section_vertices(vertices, section, std::numeric_limits<uint32_t>::max()), std::overflow_error);
    CHECK_THROWS_AS(libvgcode::append_section_vertices(vertices, vertices, 1), std::invalid_argument);
    CHECK(vertices.size() == 1);
}

TEST_CASE("Section preview retains support-only heights and compacts empty planning rows", "[SectionPreview][Nonplanar]")
{
    std::vector<float> heights {0.2f, 0.4f, 0.6f, 0.8f, 1.f};
    // Model row 2 contributes nothing; the support row at that same height
    // contributes these vertices. Later planning rows contain neither kind.
    std::vector<libvgcode::PathVertex> vertices {
        preview_vertex(0.2f, 0), preview_vertex(0.6f, 2), preview_vertex(0.6f, 2)};
    libvgcode::compact_section_layers(vertices, heights);
    REQUIRE(heights.size() == 2);
    CHECK_THAT(heights[0], Catch::Matchers::WithinAbs(0.2, 1e-6));
    CHECK_THAT(heights[1], Catch::Matchers::WithinAbs(0.6, 1e-6));
    CHECK(vertices[0].layer_id == 0);
    CHECK(vertices[1].layer_id == 1);
    CHECK(vertices[2].layer_id == 1);
    std::vector<libvgcode::PathVertex> combined;
    CHECK(libvgcode::append_section_vertices(combined, vertices, 4) == 6);
    CHECK(combined.back().layer_id == 5);
    vertices.clear();
    libvgcode::compact_section_layers(vertices, heights);
    CHECK(heights.empty());
}

TEST_CASE("Invalid section preview layer compaction leaves geometry and heights unchanged", "[SectionPreview][Nonplanar]")
{
    std::vector<float> heights {0.2f};
    std::vector<libvgcode::PathVertex> vertices {preview_vertex(5.f, 0), preview_vertex(6.f, 1)};
    CHECK_THROWS_AS(libvgcode::compact_section_layers(vertices, heights), std::invalid_argument);
    REQUIRE(heights.size() == 1);
    CHECK_THAT(heights[0], Catch::Matchers::WithinAbs(0.2, 1e-6));
    CHECK(vertices[0].layer_id == 0);
    CHECK(vertices[1].layer_id == 1);
}
