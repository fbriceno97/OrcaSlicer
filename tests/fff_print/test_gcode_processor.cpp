#include <catch2/catch_all.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Point.hpp"
#include <utility>
#include <cmath>

#include "test_utils.hpp"

#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <map>
#include <algorithm>
#include <string>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace Slic3r;
using Catch::Matchers::WithinAbs;

namespace {

void process_text(GCodeProcessor &processor, const std::string &text)
{
    ScopedTemporaryFile file(".gcode");
    {
        std::ofstream output(file.string());
        output << text;
    }
    processor.reset();
    processor.apply_config(FullPrintConfig{});
    processor.process_file(file.string());
}

float processed_belt_tilt(const std::string &gcode)
{
    ScopedTemporaryFile temp(".gcode");
    {
        std::ofstream os(temp.string());
        os << gcode;
    }
    GCodeProcessor proc;
    proc.apply_config(FullPrintConfig{});
    proc.process_file(temp.string());
    return proc.get_result().belt_tilt_angle;
}

constexpr const char *body = "G1 X10 Y10 Z0.2 F3000\nG1 X20 Y10 E1 F1200\n";

} // namespace

TEST_CASE("Section and build-axis metadata follow the emitted moves", "[GCodeProcessor][Nonplanar]")
{
    GCodeProcessor processor;
    process_text(processor,
        "G90\nM83\nT0\n;TYPE:Outer wall\nG1 X0 Y0 Z1 F3000\n"
        "; TOOL_AXIS:0,0,2\n;SEGMENT_ID:0\nG1 X10 E1 F1200\n"
        "; TOOL_AXIS:1,0,1\n;SEGMENT_ID:1\nG1 X20 Z2 E1\n"
        "; TOOL_AXIS:0,0,1\n;SEGMENT_ID:-1\nG1 X30 E1\n");
    const auto &result = processor.get_result();
    REQUIRE(result.nonplanar_metadata_valid);
    REQUIRE(result.tool_axes.size() == 2);
    std::map<unsigned, size_t> extrusion_commands;
    for (const auto &move : result.moves) {
        if (move.type != EMoveType::Extrude || move.delta_extruder <= 0.f)
            continue;
        REQUIRE(move.tool_axis_id < result.tool_axes.size());
        // Time processing can split one extrusion into acceleration/cruise/
        // deceleration vertices. Every vertex must retain its source tags.
        const size_t extrusion = extrusion_commands.try_emplace(move.gcode_id, extrusion_commands.size()).first->second;
        if (extrusion == 0) {
            CHECK(move.cut_segment_id == 0);
            CHECK(move.tool_axis_id == 0);
        } else if (extrusion == 1) {
            CHECK(move.cut_segment_id == 1);
            CHECK_THAT(result.tool_axes[move.tool_axis_id].z(), WithinAbs(std::sqrt(0.5f), 1e-6f));
        } else {
            CHECK(move.cut_segment_id == -1);
            CHECK(move.tool_axis_id == 0);
        }
    }
    CHECK(extrusion_commands.size() == 3);

    GCodeProcessorResult copied;
    copied = result;
    CHECK(copied.tool_axes == result.tool_axes);
    GCodeProcessorResult moved;
    moved = std::move(copied);
    CHECK(moved.tool_axes == result.tool_axes);
    REQUIRE(moved.moves.size() == result.moves.size());
    CHECK(moved.moves.back().cut_segment_id == -1);
}

TEST_CASE("Section travel retains physical Z during the custom start stage", "[GCodeProcessor][Nonplanar]")
{
    GCodeProcessor processor;
    process_text(processor, "G90\n;TYPE:Custom\n;SEGMENT_ID:0\nG1 X10 Y10 Z20 F1200\n");
    REQUIRE(processor.get_result().nonplanar_metadata_valid);
    float maximum_z = 0.f;
    for (const auto &move : processor.get_result().moves)
        if (move.type == EMoveType::Travel)
            maximum_z = std::max(maximum_z, move.position.z());
    CHECK_THAT(maximum_z, WithinAbs(20.f, 1e-6f));
}

TEST_CASE("Reprocessing ordinary G-code clears earlier section metadata", "[GCodeProcessor][Nonplanar]")
{
    GCodeProcessor processor;
    process_text(processor, ";TOOL_AXIS:1,0,1\n;SEGMENT_ID:4\nG1 X10 Y10 Z2 F1200\n");
    REQUIRE(processor.get_result().tool_axes.size() == 2);
    process_text(processor, "G1 X10 Y10 Z2 F1200\n");
    REQUIRE(processor.get_result().tool_axes.size() == 1);
    CHECK(processor.get_result().nonplanar_metadata_valid);
    for (const auto &move : processor.get_result().moves) {
        CHECK(move.cut_segment_id == -1);
        CHECK(move.tool_axis_id == 0);
    }
}

TEST_CASE("Malformed non-planar metadata prevents a clearance approval", "[GCodeProcessor][Nonplanar]")
{
    const std::string tag = GENERATE(";TOOL_AXIS:0,0,0\n", ";TOOL_AXIS:nan,0,1\n", ";TOOL_AXIS:1,2\n",
                                     ";SEGMENT_ID:-2\n", ";SEGMENT_ID:2147483648\n");
    GCodeProcessor processor;
    process_text(processor, tag + "G1 X10 Y10 Z2 F1200\n");
    CHECK_FALSE(processor.get_result().nonplanar_metadata_valid);
}

TEST_CASE("The config block's belt angle does not mark G-code as belt G-code", "[GCodeProcessor][belt]")
{
    // Every printer's config block lists belt_slice_rotation_angle (default 45), belt or not.
    const std::string gcode = std::string("; CONFIG_BLOCK_START\n; belt_printer = 0\n; belt_slice_rotation_angle = 45\n; CONFIG_BLOCK_END\n") + body;
    CHECK_THAT(processed_belt_tilt(gcode), WithinAbs(0., 1e-6));
}

TEST_CASE("The belt header's angle marks G-code as belt G-code", "[GCodeProcessor][belt]")
{
    const std::string gcode = std::string("; belt_slice_rotation_angle = -45.0\n") + body +
                              "; CONFIG_BLOCK_START\n; belt_printer = 1\n; belt_slice_rotation_angle = -45\n; CONFIG_BLOCK_END\n";
    CHECK_THAT(processed_belt_tilt(gcode), WithinAbs(45., 1e-6));
}

TEST_CASE("A loaded G-code's belt settings and bed replace the selected printer's in the preview", "[GCodeProcessor][belt][Regression]")
{
    // Loading a G-code applies the selected printer's config to the Print, then the file's render
    // config on top, so the preview's belt view and back-transform follow the file. The selected
    // printer is always the other kind, and every belt and bed key differs between the two.
    const bool file_is_belt = GENERATE(false, true);
    CAPTURE(file_is_belt);

    DynamicPrintConfig file = DynamicPrintConfig::full_print_config();
    file.set_deserialize_strict({
        { "belt_printer",              file_is_belt },
        { "belt_slice_rotation",       "y" },
        { "belt_slice_rotation_angle", -30 },
        { "gcode_remap_x",             "rev_x" },
        { "gcode_remap_y",             "pos_z" },
        { "gcode_remap_z",             "pos_y" },
        { "belt_frame_tilt_decouple",  1 },
        { "belt_frame_tilt_angle",     35 },
        { "printable_area",            "0x0,300x0,300x250,0x250" },
        { "printable_height",          123 },
    });
    DynamicPrintConfig selected = DynamicPrintConfig::full_print_config();
    selected.set_deserialize_strict({
        { "belt_printer",              !file_is_belt },
        { "belt_slice_rotation",       "x" },
        { "belt_slice_rotation_angle", 45 },
        { "gcode_remap_x",             "pos_x" },
        { "gcode_remap_y",             "pos_y" },
        { "gcode_remap_z",             "pos_z" },
        { "belt_frame_tilt_decouple",  0 },
        { "belt_frame_tilt_angle",     50 },
        { "printable_area",            "0x0,180x0,180x180,0x180" },
        { "printable_height",          200 },
    });

    ScopedTemporaryFile temp(".gcode");
    {
        std::ofstream os(temp.string());
        REQUIRE(os.is_open());
        // A full config block, as the loader rejects one with fewer than 80 values.
        os << "; generated by OrcaSlicer\n" << body << "; CONFIG_BLOCK_START\n";
        for (const std::string &key : file.keys())
            os << "; " << key << " = " << file.opt_serialize(key) << '\n';
        os << "; CONFIG_BLOCK_END\n";
        REQUIRE(os.good());
    }
    GCodeProcessor processor;
    REQUIRE_NOTHROW(processor.process_file(temp.string()));

    Model model;
    Print print;
    print.apply(model, selected);
    REQUIRE_NOTHROW(print.apply_config_for_render(processor.export_config_for_render()));

    // The file's belt settings win either way, so a non-belt file turns a selected belt printer's view off.
    for (const char *key : { "belt_printer", "belt_slice_rotation", "belt_slice_rotation_angle", "gcode_remap_x",
                             "gcode_remap_y", "gcode_remap_z", "belt_frame_tilt_decouple", "belt_frame_tilt_angle" }) {
        CAPTURE(key);
        CHECK(print.config().opt_serialize(key) == file.opt_serialize(key));
    }
    // Only a belt file brings the bed it was sliced for.
    const DynamicPrintConfig &bed_source = file_is_belt ? file : selected;
    for (const char *key : { "printable_area", "printable_height" }) {
        CAPTURE(key);
        CHECK(print.config().opt_serialize(key) == bed_source.opt_serialize(key));
    }
}

TEST_CASE("Non-belt start G-code moves keep the first-layer Z in the processor", "[GCodeProcessor]")
{
    // The belt path (GCodeWriter tests: "start-gcode prepare-stage moves keep their real Z")
    // stores the real Z of a move inside the start G-code. Every other printer must keep
    // the historical behaviour: a prepare-stage move is pinned to the first-layer height
    // so the preview does not draw the start sequence's travel. The gate is the belt
    // header, so a file without one, whatever its config block says, takes this path.
    struct BBLPrinterGuard {
        bool prev = GCodeProcessor::s_IsBBLPrinter;
        BBLPrinterGuard()  { GCodeProcessor::s_IsBBLPrinter = false; }
        ~BBLPrinterGuard() { GCodeProcessor::s_IsBBLPrinter = prev; }
    } bbl_guard;

    const std::string gcode =
        "G90\n"
        "G21\n"
        "M83\n"
        ";TYPE:Custom\n"
        "G1 E-1.5 F2100\n"
        "G1 X45 Y0.3 Z50 F12000\n"   // prepare-stage travel to a high Z
        "G1 E1.5 F1800\n"
        ";TYPE:Outer wall\n"
        "G1 X46 Y0.3 Z50 E0.05\n"
        "; CONFIG_BLOCK_START\n; belt_printer = 0\n; belt_slice_rotation_angle = 45\n; CONFIG_BLOCK_END\n";

    GCodeProcessor processor;
    FullPrintConfig config;
    config.initial_layer_print_height.value = 0.3;
    processor.apply_config(config);
    processor.process_buffer(gcode);
    const GCodeProcessorResult &result = processor.get_result();
    REQUIRE_THAT(result.belt_tilt_angle, WithinAbs(0., 1e-6));

    size_t first_extrude = result.moves.size();
    for (size_t i = 0; i < result.moves.size(); ++i)
        if (result.moves[i].type == EMoveType::Extrude) { first_extrude = i; break; }
    REQUIRE(first_extrude < result.moves.size());
    REQUIRE(first_extrude > 0);

    // The extrusion keeps its real Z; the prepare-stage move before it is pinned to the
    // first-layer height.
    CHECK_THAT(result.moves[first_extrude].position.z(),     WithinAbs(50., 1e-3));
    CHECK_THAT(result.moves[first_extrude - 1].position.z(), WithinAbs(0.3, 1e-3));
}
