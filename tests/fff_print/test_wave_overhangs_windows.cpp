// Windows x64 prototype acceptance test for the first Unlayered wave-engine port.
// Intentionally excludes Unlayered's unported floor, temperature, and nonplanar options.
// AGPL-3.0-or-later attribution is preserved in WaveOverhangs source files.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <string>
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/ExtrusionEntityCollection.hpp"
#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Config.hpp"
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

TEST_CASE("Wave islands keep their own collection when unsupported walls are deferred", "[WaveOverhangs][Windows]")
{
    const char *wall_generator = GENERATE("classic", "arachne");
    TriangleMesh model_mesh = make_cube(10., 10., 10.);
    TriangleMesh plate = make_cube(30., 30., 2.);
    plate.translate(-10.f, -10.f, 10.f);
    model_mesh.merge(plate);
    DynamicPrintConfig config = settings(true);
    config.set_deserialize_strict({{"wall_generator", wall_generator}, {"unsupported_wall_last", "1"}});
    Print print;
    Model model;
    Test::init_and_process_print({model_mesh}, print, model, config);
    CHECK(count_waves(print) > 0);
}

TEST_CASE("Exported wave moves respect their speed cap with dynamic overhang control on or off", "[WaveOverhangs][Windows]")
{
    const bool dynamic_overhang_speed = GENERATE(false, true);
    const double bridge_speed = GENERATE(3.0, 80.0);
    const double wave_speed = 7.5;
    DynamicPrintConfig config = settings(true);
    config.set_deserialize_strict({
        {"wave_overhang_speed", std::to_string(wave_speed)},
        {"enable_overhang_speed", dynamic_overhang_speed ? "1" : "0"},
        {"bridge_speed", std::to_string(bridge_speed)},
        {"inner_wall_speed", "100"},
        {"outer_wall_speed", "100"},
        {"enable_arc_fitting", "0"},
    });
    const std::string gcode = Test::slice({unsupported_shelf()}, config);
    GCodeReader reader;
    reader.apply_config(config);
    bool in_wave = false;
    size_t wave_moves = 0;
    size_t wave_paths = 0;
    reader.parse_buffer(gcode, [&](GCodeReader &state, const GCodeReader::GCodeLine &line) {
        if (line.raw() == "; WAVE_OVERHANG_BEGIN") {
            REQUIRE_FALSE(in_wave);
            in_wave = true;
            ++wave_paths;
        } else if (line.raw() == "; WAVE_OVERHANG_END") {
            REQUIRE(in_wave);
            in_wave = false;
        } else if (in_wave && line.extruding(state) && line.dist_XY(state) > 0) {
            ++wave_moves;
            CHECK(std::isfinite(line.new_X(state)));
            CHECK(std::isfinite(line.new_Y(state)));
            CHECK(std::isfinite(line.new_Z(state)));
            CHECK(line.new_F(state) > 0);
            CHECK(line.new_F(state) <= std::min(wave_speed, bridge_speed) * 60.0 + 0.1);
        }
    });
    CHECK_FALSE(in_wave);
    CHECK(wave_paths > 0);
    CHECK(wave_moves > 0);
}

TEST_CASE("A wave speed setting does not mark ordinary exported toolpaths as waves", "[WaveOverhangs][Windows]")
{
    DynamicPrintConfig config = settings(false);
    config.set_deserialize_strict({{"wave_overhang_speed", "0.1"}});
    const std::string gcode = Test::slice({unsupported_shelf()}, config);
    CHECK(gcode.find("; WAVE_OVERHANG_BEGIN") == std::string::npos);
    CHECK(gcode.find("; WAVE_OVERHANG_END") == std::string::npos);
}

TEST_CASE("Wave cooling overrides restore ordinary cooling and respect layers without fans", "[WaveOverhangs][Windows]")
{
    const bool cooling_disabled = GENERATE(false, true);
    DynamicPrintConfig config = settings(true);
    config.set_deserialize_strict({
        {"wave_overhang_fan_speed", "100"}, {"gcode_flavor", "marlin"},
        {"fan_min_speed", "25"}, {"fan_max_speed", "25"},
        {"reduce_fan_stop_start_freq", "1"}, {"enable_overhang_bridge_fan", "0"},
        {"close_fan_the_first_x_layers", cooling_disabled ? "1000" : "0"},
        {"initial_layer_fan_speed", "-1"}, {"part_cooling_fan_min_pwm", "0"}
    });
    const std::string output = Test::slice({unsupported_shelf()}, config);
    CHECK(output.find(";_WAVE_FAN_") == std::string::npos);
    GCodeReader reader;
    reader.apply_config(config);
    bool in_wave = false;
    double fan_pwm = -1.;
    size_t checked_extrusions = 0, checked_restores = 0;
    reader.parse_buffer(output, [&](GCodeReader &state, const GCodeReader::GCodeLine &line) {
        float speed, fan;
        if (line.cmd_is("M106") && line.has_value('S', speed) && (!line.has_value('P', fan) || fan == 0.f))
            fan_pwm = speed;
        else if (line.cmd_is("M107"))
            fan_pwm = 0.;
        if (line.raw() == "; WAVE_OVERHANG_BEGIN")
            in_wave = true;
        else if (line.raw() == "; WAVE_OVERHANG_END") {
            CHECK(fan_pwm < 255.);
            ++checked_restores;
            in_wave = false;
        } else if (in_wave && line.extruding(state) && line.dist_XY(state) > 0.) {
            CHECK_THAT(fan_pwm, Catch::Matchers::WithinAbs(cooling_disabled ? 0. : 255., 0.1));
            ++checked_extrusions;
        }
    });
    CHECK(checked_extrusions > 0);
    CHECK(checked_restores > 0);
}
