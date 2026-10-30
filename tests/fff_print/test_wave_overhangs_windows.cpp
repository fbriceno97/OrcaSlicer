// Windows x64 prototype acceptance test for the first Unlayered wave-engine port.
// Intentionally excludes Unlayered's unported floor, temperature, and nonplanar options.
// AGPL-3.0-or-later attribution is preserved in WaveOverhangs source files.
#include <catch2/catch_all.hpp>
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/ExtrusionEntityCollection.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "test_helpers.hpp"
using namespace Slic3r;

namespace {
TriangleMesh unsupported_shelf()
{
    TriangleMesh stem  = make_cube(10., 10., 10.);
    TriangleMesh shelf = make_cube(30., 10., 6.);
    shelf.translate(-10.f, 0.f, 10.f);
    stem.merge(shelf);
    return stem;
}
size_t wave_count(const ExtrusionEntity *entity)
{
    if (const auto *coll = dynamic_cast<const ExtrusionEntityCollection *>(entity)) {
        size_t n = 0;
        for (const auto *item : coll->entities) n += wave_count(item);
        return n;
    }
    if (const auto *loop = dynamic_cast<const ExtrusionLoop *>(entity)) {
        size_t n = 0;
        for (const auto &p : loop->paths) if (p.wave_overhang) ++n;
        return n;
    }
    if (const auto *multi = dynamic_cast<const ExtrusionMultiPath *>(entity)) {
        size_t n = 0;
        for (const auto &p : multi->paths) if (p.wave_overhang) ++n;
        return n;
    }
    if (const auto *path = dynamic_cast<const ExtrusionPath *>(entity)) return path->wave_overhang ? 1 : 0;
    return 0;
}
size_t count_waves(const Print &print)
{
    size_t n = 0;
    for (const PrintObject *obj : print.objects())
        for (const Layer *layer : obj->layers())
            for (const LayerRegion *region : layer->regions())
                n += wave_count(&region->perimeters);
    return n;
}
DynamicPrintConfig settings(bool enabled)
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({
        {"layer_height", "0.2"},
        {"initial_layer_print_height", "0.2"},
        {"wall_loops", "2"},
        {"detect_overhang_wall", "1"},
        {"enable_support", "0"},
        {"sparse_infill_density", "15%"},
        {"layer_change_gcode", "G92 E0"},
        {"wave_overhangs", enabled ? "1" : "0"},
        {"wave_overhang_line_spacing", "0.35"}
    });
    return config;
}
}

TEST_CASE("Wave-overhangs are disabled unless the user opts in", "[WaveOverhangs][Windows]")
{
    const DynamicPrintConfig stock = DynamicPrintConfig::full_print_config();
    const auto *enabled = stock.option<ConfigOptionBool>("wave_overhangs");
    REQUIRE(enabled != nullptr);
    CHECK_FALSE(enabled->value);
    Print normal;
    Test::init_and_process_print({unsupported_shelf()}, normal, settings(false));
    CHECK(count_waves(normal) == 0);
}

TEST_CASE("Enabled wave-overhangs generate flagged toolpaths for unsupported shelf", "[WaveOverhangs][Windows]")
{
    Print wave;
    Test::init_and_process_print({unsupported_shelf()}, wave, settings(true));
    CHECK(count_waves(wave) > 0);
}
