#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "libslic3r/GCode/HotendCollisionCheck.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Exception.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"

#include <boost/filesystem/operations.hpp>

#include <limits>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstddef>

using namespace Slic3r;

namespace {
void sample_sections(GCodeProcessorResult &result, bool clear, bool tilted)
{
    if (tilted)
        result.tool_axes.push_back(Vec3f::UnitX());
    const auto append = [&](Vec3f position, EMoveType type, int section) {
        GCodeProcessorResult::MoveVertex move;
        move.position = position;
        move.type = type;
        move.cut_segment_id = section;
        move.tool_axis_id = section == 1 && tilted ? 1 : 0;
        move.gcode_id = static_cast<unsigned>(result.moves.size() + 1);
        move.delta_extruder = type == EMoveType::Extrude ? 1.f : 0.f;
        result.moves.push_back(move);
    };
    append(Vec3f(0.f, 0.f, 10.f), EMoveType::Noop, 0);
    append(Vec3f(10.f, 0.f, 10.f), EMoveType::Extrude, 0);
    if (clear) {
        append(Vec3f(10.f, 0.f, 30.f), EMoveType::Travel, 1);
        append(Vec3f(30.f, 0.f, 30.f), EMoveType::Travel, 1);
        append(Vec3f(30.f, 0.f, 0.f), EMoveType::Travel, 1);
    } else
        append(Vec3f(0.f, 0.f, 0.f), EMoveType::Travel, 1);
}
}

TEST_CASE("The upright hotend detects travel through an earlier section even when build axes match", "[HotendCollision]")
{
    const bool tilted = GENERATE(false, true);
    GCodeProcessorResult result;
    sample_sections(result, false, tilted);
    const auto collision = check_hotend_collisions(result, HotendCollisionParams{});
    REQUIRE(collision.has_value());
    CHECK_FALSE(collision->message.empty());
}

TEST_CASE("A lifted transition outside earlier material stays clear", "[HotendCollision]")
{
    const bool tilted = GENERATE(false, true);
    GCodeProcessorResult result;
    sample_sections(result, true, tilted);
    CHECK_FALSE(check_hotend_collisions(result, HotendCollisionParams{}).has_value());
}

TEST_CASE("Earlier material is checked within a single section in motion order", "[HotendCollision]")
{
    const bool clear = GENERATE(false, true);
    GCodeProcessorResult result;
    sample_sections(result, clear, false);
    for (auto &move : result.moves)
        move.cut_segment_id = 0;
    CHECK(check_hotend_collisions(result, HotendCollisionParams{}).has_value() == !clear);
}

TEST_CASE("Travel is not rejected for material deposited later in the same section", "[HotendCollision]")
{
    GCodeProcessorResult result;
    const std::vector<Vec3f> positions {Vec3f(0.f, 0.f, 0.f), Vec3f(10.f, 0.f, 0.f),
        Vec3f(10.f, 0.f, 10.f), Vec3f(0.f, 0.f, 10.f)};
    for (size_t i = 0; i < positions.size(); ++i) {
        GCodeProcessorResult::MoveVertex move;
        move.position = positions[i];
        move.type = i == 0 ? EMoveType::Noop : (i == 3 ? EMoveType::Extrude : EMoveType::Travel);
        move.delta_extruder = i == 3 ? 1.f : 0.f;
        move.cut_segment_id = 0;
        move.gcode_id = static_cast<unsigned>(i + 1);
        result.moves.push_back(move);
    }
    CHECK_FALSE(check_hotend_collisions(result, HotendCollisionParams{}).has_value());
}

TEST_CASE("Removing section comments cannot bypass upright hotend clearance checks", "[HotendCollision]")
{
    const bool clear = GENERATE(false, true);
    GCodeProcessorResult result;
    sample_sections(result, clear, false);
    for (auto &move : result.moves)
        move.cut_segment_id = -1;
    CHECK(check_hotend_collisions(result, HotendCollisionParams{}).has_value() == !clear);
}

TEST_CASE("Long clearance checks propagate cancellation", "[HotendCollision]")
{
    GCodeProcessorResult result;
    sample_sections(result, false, false);
    HotendCollisionParams params;
    bool cancellation_checked = false;
    params.throw_on_cancel = [&]() {
        cancellation_checked = true;
        throw std::runtime_error("Canceled clearance check");
    };
    CHECK_THROWS_AS(check_hotend_collisions(result, params), std::runtime_error);
    CHECK(cancellation_checked);
}

TEST_CASE("Invalid metadata and positions cannot be reported as collision-free", "[HotendCollision]")
{
    GCodeProcessorResult result;
    sample_sections(result, false, false);
    result.nonplanar_metadata_valid = false;
    CHECK_THROWS_AS(check_hotend_collisions(result, HotendCollisionParams{}), std::invalid_argument);
    result.nonplanar_metadata_valid = true;
    result.moves.back().position.x() = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(check_hotend_collisions(result, HotendCollisionParams{}), std::invalid_argument);
}

TEST_CASE("An oversized collision grid fails instead of silently approving clearance", "[HotendCollision]")
{
    GCodeProcessorResult result;
    sample_sections(result, false, false);
    result.moves.back().position = Vec3f(8000.f, 8000.f, 0.f);
    CHECK_THROWS_AS(check_hotend_collisions(result, HotendCollisionParams{}), std::runtime_error);
}

TEST_CASE("The export gate rejects colliding sections and disabled envelopes while preserving default-off behavior", "[HotendCollision][Print]")
{
    Print print;
    Model model;
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}});
    Slic3r::Test::init_print({Slic3r::Test::cube(8.)}, print, model, config);
    GCodeProcessorResult result;
    sample_sections(result, false, false);
    CHECK_THROWS_AS(print.validate_nonplanar_gcode(result), SlicingError);

    result.reset();
    sample_sections(result, true, false);
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));
    config.set_deserialize_strict({{"nonplanar_hotend_cone_height", "0"}});
    print.apply(model, config);
    CHECK_THROWS_AS(print.validate_nonplanar_gcode(result), SlicingError);

    config.set_deserialize_strict({{"enable_nonplanar", "0"}});
    print.apply(model, config);
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));
}

TEST_CASE("Rejected non-planar exports leave neither printable output nor a temporary file", "[HotendCollision][Print]")
{
    const bool preview_requested = GENERATE(false, true);
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"},
        {"machine_end_gcode", "; SEGMENT_ID:invalid"}});
    Print print;
    Model model;
    Slic3r::Test::init_print({Slic3r::Test::cube(8.)}, print, model, config);
    print.set_status_silent();
    print.process();
    ScopedTemporaryFile output(".gcode");
    GCodeProcessorResult preview;
    CHECK_THROWS_AS(print.export_gcode(output.string(), preview_requested ? &preview : nullptr), SlicingError);
    CHECK_FALSE(boost::filesystem::exists(output.string()));
    CHECK_FALSE(boost::filesystem::exists(output.string() + ".tmp"));
}

TEST_CASE("Edited G-code must remain readable and retain valid section metadata", "[HotendCollision][Print]")
{
    Print print;
    Model model;
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}});
    Slic3r::Test::init_print({Slic3r::Test::cube(8.)}, print, model, config);
    ScopedTemporaryFile edited(".gcode");
    {
        std::ofstream out(edited.string());
        REQUIRE(out.is_open());
        out << "G90\n; SEGMENT_ID:invalid\nG1 X1 Y1 Z1 F600\n";
    }
    CHECK_THROWS_AS(print.validate_nonplanar_gcode_file(edited.string()), SlicingError);
    boost::filesystem::remove(edited.string());
    CHECK_THROWS(print.validate_nonplanar_gcode_file(edited.string()));
    config.set_deserialize_strict({{"enable_nonplanar", "0"}});
    print.apply(model, config);
    CHECK_NOTHROW(print.validate_nonplanar_gcode_file(edited.string()));
}

TEST_CASE("Edited XYZ motion is collision checked without section comments", "[HotendCollision][Print]")
{
    const bool clear = GENERATE(false, true);
    Print print;
    Model model;
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}});
    Slic3r::Test::init_print({Slic3r::Test::cube(8.)}, print, model, config);
    ScopedTemporaryFile edited(".gcode");
    {
        std::ofstream out(edited.string());
        REQUIRE(out.is_open());
        out << "G90\nM83\nG1 X0 Y0 Z10 F600\nG1 X10 Y0 Z10 E1\n";
        if (clear)
            out << "G1 Z30\nG1 X30\nG1 Z0\n";
        else
            out << "G1 X0 Z0\n";
    }
    if (clear)
        CHECK_NOTHROW(print.validate_nonplanar_gcode_file(edited.string()));
    else
        CHECK_THROWS_AS(print.validate_nonplanar_gcode_file(edited.string()), SlicingError);
    config.set_deserialize_strict({{"enable_nonplanar", "0"}});
    print.apply(model, config);
    CHECK_NOTHROW(print.validate_nonplanar_gcode_file(edited.string()));
}
