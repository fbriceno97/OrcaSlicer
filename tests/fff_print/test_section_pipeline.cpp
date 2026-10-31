#include "libslic3r/Support/SectionSupportDemand.hpp"
#include "libslic3r/GCode/SectionFrame.hpp"
#include "libslic3r/CutPlanes.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/ExtrusionEntityCollection.hpp"
#include "libslic3r/Polyline.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/Config.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/GCode.hpp"
#include "libslic3r/GCodeWriter.hpp"
#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/GCode/CoolingBuffer.hpp"
#include "libslic3r/GCode/PressureEqualizer.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Geometry.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/ExtrusionEntity.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"
#include "libslic3r/Exception.hpp"

#include <Eigen/Geometry>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <cstddef>

namespace Slic3r::Test {
struct PrintPipelineAccess {
    static void process(Print &print) { print.process_impl(nullptr, false); }
};
}

using namespace Slic3r;

TEST_CASE("Tilted section motion keeps feed limits and metadata through pressure and cooling processing", "[Nonplanar][GCode]")
{
    FullPrintConfig config;
    config.enable_nonplanar.value = true;
    config.use_relative_e_distances.value = true;
    config.travel_speed.values = {200.};
    config.travel_speed_z.values = {4.};
    config.machine_max_speed_z.values = {8., 8.};
    config.max_volumetric_extrusion_rate_slope.value = 1.;
    config.slow_down_for_layer_cooling.values = {true};
    config.slow_down_layer_time.values = {30.};
    // A cooling minimum above the physical cap must not accelerate the input.
    config.slow_down_min_speed.values = {20.};
    struct PipelineGCode : GCode {
        explicit PipelineGCode(const FullPrintConfig &config)
        {
            m_config.apply(config, true);
            m_writer.apply_print_config(config);
            m_writer.set_extruders({0});
            m_writer.set_extruder(0);
        }
    } generator(config);
    GCodeWriter &writer = generator.writer();
    writer.set_section_motion_limits(true);
    Transform3d frame = Transform3d::Identity();
    frame.rotate(Eigen::AngleAxisd(Geometry::deg2rad(30.), Vec3d::UnitX()));
    frame.translation() = Vec3d(40., 40., 10.);
    writer.set_section_transform(frame);
    std::string program = "G90\nM83\nT0\n; SEGMENT_ID:0\n; TOOL_AXIS:0,-0.5,0.8660254037844386\n"
        "; FEATURE: Outer wall\n; LAYER_HEIGHT: 0.2\n; LINE_WIDTH: 0.4\n;_EXTRUSION_ROLE:" +
        std::to_string(int(erExternalPerimeter)) + "\n";
    program += writer.travel_to_xyz(Vec3d(0., 0., 0.2));
    for (int i = 1; i <= 3; ++i) {
        program += writer.set_speed(12000., "", ";_EXTRUDE_SET_SPEED");
        const double extrusion = i == 1 ? 0.3 : (i == 2 ? 0.2 : 0.4);
        program += writer.extrude_to_xy(Vec2d(0., i * 10.), extrusion);
        program += ";_EXTRUDE_END\n";
    }
    writer.set_section_transform(std::nullopt);
    program += "; SEGMENT_ID:-1\n; TOOL_AXIS:0,0,1\n";
    program += writer.travel_to_xyz(writer.get_position() + Vec3d(10., 0., 10.));

    PressureEqualizer pressure(writer.config);
    LayerResult input{std::move(program), 5, false, true, false};
    CHECK(pressure.process_layer(std::move(input)).nop_layer_result);
    LayerResult processed = pressure.process_layer(LayerResult::make_nop_layer_result());
    REQUIRE_FALSE(processed.nop_layer_result);
    CoolingBuffer cooling(generator);
    cooling.reset(Vec3d::Zero());
    const std::string output = cooling.process_layer(std::move(processed.gcode), processed.layer_id, true);
    REQUIRE_FALSE(output.empty());
    size_t feedrates = 0;
    double extrusion = 0.;
    GCodeReader reader;
    reader.apply_config(writer.config);
    reader.parse_buffer(output, [&](GCodeReader &, const GCodeReader::GCodeLine &line) {
        if (!line.cmd_is("G1"))
            return;
        if (line.has(F)) {
            CHECK(line.f() <= 240.001f);
            CHECK(line.f() > 0.f);
            ++feedrates;
        }
        if (line.has(E) && line.e() > 0.f)
            extrusion += line.e();
    });
    CHECK(feedrates > 0);
    CHECK_THAT(extrusion, Catch::Matchers::WithinAbs(0.9, 0.0001));

    GCodeProcessor processor;
    processor.apply_config(config);
    processor.preserve_preparation_z(true);
    processor.initialize_result_moves();
    processor.process_buffer(output);
    processor.finalize(false);
    const auto &result = processor.get_result();
    CHECK(result.nonplanar_metadata_valid);
    CHECK(result.tool_axes.size() == 2);
    REQUIRE_FALSE(result.moves.empty());
    CHECK(result.moves.back().cut_segment_id == -1);
    Print print;
    Model model;
    DynamicPrintConfig validation = DynamicPrintConfig::full_print_config();
    validation.set_deserialize_strict("enable_nonplanar", "1");
    Slic3r::Test::init_print({Slic3r::Test::cube(8.)}, print, model, validation);
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));

    if (const char *directory = std::getenv("ORCA_TEST_GCODE_ARTIFACT_DIR"); directory && *directory) {
        boost::filesystem::create_directories(directory);
        std::ofstream fixture((boost::filesystem::path(directory) / "nonplanar-tilted-pipeline.gcode").string(), std::ios::binary);
        REQUIRE(fixture.is_open());
        fixture << "; Software regression fixture only; NOT printer-ready G-code.\n" << output;
        fixture.flush();
        REQUIRE(fixture.good());
    }
}

TEST_CASE("Sliced inherited supports retain contact height and flow in section G-code", "[Nonplanar][GCode]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}, {"enable_support", "1"},
        {"support_type", "normal(auto)"}, {"layer_height", "0.2"},
        {"initial_layer_print_height", "0.2"}, {"support_top_z_distance", "0.2"},
        {"independent_support_layer_height", "1"}, {"elefant_foot_compensation", "0"},
        {"use_relative_e_distances", "1"}, {"travel_speed_z", "4"}});
    TriangleMesh mesh = make_cube(4., 10., 10.);
    TriangleMesh cap = make_cube(20., 10., 2.);
    cap.translate(0.f, 0.f, 10.f);
    mesh.merge(cap);
    Print print;
    Model model;
    Slic3r::Test::init_print({mesh}, print, model, config);
    ModelObject *object = model.objects.front();
    object->cut_planes.enabled = true;
    CutPlane plane;
    plane.point = Vec3d(2., 5., 10.);
    object->cut_planes.planes.push_back(plane);
    print.apply(model, config);
    const auto families = section_support_families(print);
    REQUIRE(families.size() == 1);
    generate_section_support_family(families.front());

    struct PathGCode : GCode { using GCode::collect_layers_to_print; } generator;
    generator.apply_print_config(print.config());
    GCodeWriter &writer = generator.writer();
    writer.set_extruders({0});
    writer.set_extruder(0);
    writer.set_section_motion_limits(true);
    writer.set_position_from_custom_gcode(Vec3d(0., 0., 2.));
    std::string program = "G90\nM83\nT0\nG1 X0 Y0 Z2 F240\n";
    const double diameter = print.config().filament_diameter.values.front();
    const double filament_area = PI * diameter * diameter / 4.;
    double deposited_z = 0., expected_e = 0.;
    size_t emitted_paths = 0;
    // This is a component fixture with deliberately elevated inter-path travel,
    // not the production export path or a printer startup/end sequence.
    for (auto section = families.front().rbegin(); section != families.front().rend(); ++section) {
        const PrintInstance &instance = (*section)->instances().front();
        const Transform3d to_world = instance.get_slice_to_world_transform();
        REQUIRE_THAT((to_world.linear().col(2) - Vec3d::UnitZ()).norm(), Catch::Matchers::WithinAbs(0., 1e-9));
        generator.set_origin(Vec2d(unscale<double>(instance.shift.x()), unscale<double>(instance.shift.y())));
        program += begin_section_frame(instance, generator.origin(), writer);
        for (const auto &row : PathGCode::collect_layers_to_print(**section)) {
            const auto emit = [&](const ExtrusionEntity &entity) {
                for_each_extrusion_path(entity, [&](const ExtrusionPath &path) {
                    const Polyline polyline = path.polyline.to_polyline();
                    REQUIRE(polyline.points.size() >= 2);
                    REQUIRE(path.mm3_per_mm > 0.);
                    const double z = row.print_z();
                    const double staging_z = deposited_z + 1. - to_world.translation().z();
                    program += writer.travel_to_xyz(Vec3d(writer.get_position().x(), writer.get_position().y(), staging_z), "fixture clearance", true);
                    const Vec2d start = generator.point_to_gcode(polyline.points.front());
                    program += writer.travel_to_xy(start);
                    program += writer.travel_to_z(z, "fixture descent", true);
                    const bool support = is_support(path.role());
                    program += std::string("; FEATURE: ") + (support ? "Support" : "Outer wall") +
                        "\n; LAYER_HEIGHT: " + std::to_string(path.height) +
                        "\n; LINE_WIDTH: " + std::to_string(path.width) + "\n";
                    program += writer.set_speed(3000.);
                    for (size_t i = 1; i < polyline.points.size(); ++i) {
                        const double length = unscale<double>(polyline.points[i].distance_to(polyline.points[i - 1]));
                        const double extrusion = length * path.mm3_per_mm / filament_area;
                        program += writer.extrude_to_xy(generator.point_to_gcode(polyline.points[i]), extrusion);
                        expected_e += extrusion;
                    }
                    deposited_z = std::max(deposited_z, writer.position_for_custom_gcode().z());
                    ++emitted_paths;
                });
            };
            if (row.object_layer)
                for (const LayerRegion *region : row.object_layer->regions())
                    emit(region->perimeters);
            if (row.support_layer)
                emit(row.support_layer->support_fills);
        }
        program += end_section_frame(writer);
    }
    REQUIRE(emitted_paths > 0);
    double parsed_e = 0.;
    GCodeReader reader;
    reader.apply_config(writer.config);
    reader.parse_buffer(program, [&](GCodeReader &, const GCodeReader::GCodeLine &line) {
        if (line.has(F))
            CHECK(line.f() <= 240.001f);
        if (line.has(E) && line.e() > 0.f)
            parsed_e += line.e();
    });
    CHECK_THAT(parsed_e, Catch::Matchers::WithinRel(expected_e, 0.005));
    GCodeProcessor processor;
    processor.apply_config(print.config());
    processor.preserve_preparation_z(true);
    processor.initialize_result_moves();
    processor.process_buffer(program);
    processor.finalize(false);
    const auto &result = processor.get_result();
    CHECK(result.nonplanar_metadata_valid);
    double support_top = 0.;
    for (const auto &move : result.moves)
        if (move.type == EMoveType::Extrude && is_support(move.extrusion_role))
            support_top = std::max(support_top, double(move.position.z()));
    CHECK_THAT(support_top, Catch::Matchers::WithinAbs(9.8, 0.021));
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));
    if (const char *directory = std::getenv("ORCA_TEST_GCODE_ARTIFACT_DIR"); directory && *directory) {
        boost::filesystem::create_directories(directory);
        std::ofstream fixture((boost::filesystem::path(directory) / "nonplanar-inherited-support.gcode").string(), std::ios::binary);
        REQUIRE(fixture.is_open());
        fixture << "; Software perimeter/support fixture with elevated travel; NOT printer-ready.\n" << program;
        fixture.flush();
        REQUIRE(fixture.good());
    }
}

TEST_CASE("Production export preserves section order and world heights for a perimeter-only partition", "[Nonplanar][GCode]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}, {"print_sequence", "by object"},
        {"enable_support", "0"}, {"layer_height", "0.2"}, {"initial_layer_print_height", "0.2"},
        {"sparse_infill_density", "0%"}, {"top_shell_layers", "0"}, {"bottom_shell_layers", "0"},
        {"top_shell_thickness", "0"}, {"bottom_shell_thickness", "0"},
        {"wall_loops", "2"}, {"skirt_loops", "0"}, {"brim_type", "no_brim"},
        {"use_relative_e_distances", "1"}, {"travel_speed_z", "4"},
        {"machine_start_gcode", "G90\nM83\nG1 X0 Y0 Z2 F240\n"},
        {"machine_end_gcode", "M400\n"}, {"before_layer_change_gcode", ""},
        {"layer_change_gcode", "G1 Z{layer_z}\n"}, {"time_lapse_gcode", ""}});
    Print print;
    Model model;
    Slic3r::Test::init_print({Slic3r::Test::cube(20.)}, print, model, config);
    ModelObject *object = model.objects.front();
    object->instances.front()->set_offset(Vec3d(60., 60., 0.));
    object->cut_planes.enabled = true;
    CutPlane plane;
    plane.point = Vec3d(10., 10., 10.);
    object->cut_planes.planes.push_back(plane);
    print.apply(model, config);
    print.set_status_silent();
    // Exercise the existing lower-level exporter in a software fixture. Public
    // Print processing remains guarded; no application export bypass is added.
    REQUIRE_THROWS_AS(print.process(), SlicingError);
    const auto families = section_support_families(print);
    REQUIRE(families.size() == 1);
    generate_section_support_family(families.front());
    ScopedTemporaryFile file(".gcode");
    GCodeProcessorResult result;
    GCode generator;
    REQUIRE_NOTHROW(generator.do_export(&print, file.string().c_str(), &result));
    REQUIRE(result.nonplanar_metadata_valid);
    size_t root_moves = 0, child_moves = 0;
    bool child_started = false;
    for (const auto &move : result.moves) {
        if (move.type != EMoveType::Extrude || move.delta_extruder <= 0.f)
            continue;
        if (move.cut_segment_id == 0) {
            CHECK_FALSE(child_started);
            CHECK(move.position.z() <= 10.001f);
            ++root_moves;
        } else if (move.cut_segment_id == 1) {
            child_started = true;
            CHECK(move.position.z() >= 9.999f);
            ++child_moves;
        }
    }
    CHECK(root_moves > 0);
    CHECK(child_moves > 0);
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));
    if (const char *directory = std::getenv("ORCA_TEST_GCODE_ARTIFACT_DIR"); directory && *directory) {
        boost::filesystem::create_directories(directory);
        std::ifstream source(file.string(), std::ios::binary);
        std::ofstream fixture((boost::filesystem::path(directory) / "nonplanar-production-perimeters.gcode").string(), std::ios::binary);
        REQUIRE(source.is_open());
        REQUIRE(fixture.is_open());
        fixture << "; Software production-export fixture only; NOT printer-ready.\n" << source.rdbuf();
        fixture.flush();
        REQUIRE(fixture.good());
    }
}

TEST_CASE("Full section slicing exports solid surfaces and infill through upright and tilted frames", "[Nonplanar][GCode]")
{
    const int mode = GENERATE(0, 1, 2, 3, 4); // upright, tilted, normal, classic tree, organic
    const bool tilted = mode == 1;
    const bool supports = mode >= 2;
    INFO("section pipeline mode " << mode);
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}, {"print_sequence", "by object"},
        {"enable_support", supports ? "1" : "0"}, {"layer_height", "0.2"},
        {"support_type", mode < 3 ? "normal(auto)" : "tree(auto)"},
        {"support_style", mode < 3 ? "default" : (mode == 3 ? "tree_hybrid" : "organic")},
        {"support_top_z_distance", "0.2"}, {"independent_support_layer_height", "1"},
        {"elefant_foot_compensation", "0"}, {"initial_layer_print_height", "0.2"},
        {"sparse_infill_density", "15%"}, {"top_shell_layers", "3"}, {"bottom_shell_layers", "3"},
        {"top_shell_thickness", "0"}, {"bottom_shell_thickness", "0"},
        {"wall_loops", "2"}, {"skirt_loops", "0"}, {"brim_type", "no_brim"},
        {"use_relative_e_distances", "1"}, {"travel_speed_z", "4"},
        {"nozzle_temperature_initial_layer", "220"}, {"nozzle_temperature", "205"},
        {"printing_by_object_gcode", "; PHYSICAL_BED_OBJECT_START\n"},
        {"fan_min_speed", "30"}, {"fan_max_speed", "30"}, {"reduce_fan_stop_start_freq", "1"},
        {"enable_overhang_bridge_fan", "0"}, {"close_fan_the_first_x_layers", "3"},
        {"initial_layer_fan_speed", "-1"}, {"full_fan_speed_layer", "5"},
        {"part_cooling_fan_min_pwm", "0"}, {"slow_down_for_layer_cooling", "0"},
        {"machine_start_gcode", "G90\nM83\nG1 X0 Y0 Z2 F240\n"},
        {"machine_end_gcode", "M400\n"}, {"before_layer_change_gcode", ""},
        {"layer_change_gcode", "G1 Z{layer_z}\n"}, {"time_lapse_gcode", ""}});
    Print print;
    Model model;
    TriangleMesh mesh = supports ? make_cube(4., 10., 10.) : Slic3r::Test::cube(20.);
    if (supports) {
        TriangleMesh cap = make_cube(20., 10., 2.);
        cap.translate(0.f, 0.f, 10.f);
        mesh.merge(cap);
    }
    Slic3r::Test::init_print({mesh}, print, model, config);
    ModelObject *object = model.objects.front();
    object->instances.front()->set_offset(Vec3d(60., 60., 0.));
    object->cut_planes.enabled = true;
    CutPlane plane;
    plane.point = supports ? Vec3d(2., 5., 10.) : Vec3d(10., 10., 10.);
    if (tilted)
        plane.normal = Vec3d(std::sin(Geometry::deg2rad(10.)), 0., std::cos(Geometry::deg2rad(10.)));
    object->cut_planes.planes.push_back(plane);
    print.apply(model, config);
    print.set_status_silent();
    REQUIRE_THROWS_AS(print.process(), SlicingError);
    REQUIRE_NOTHROW(Slic3r::Test::PrintPipelineAccess::process(print));
    REQUIRE(print.is_step_done(posInfill));
    REQUIRE(print.is_step_done(posSupportMaterial));
    ScopedTemporaryFile file(".gcode");
    GCodeProcessorResult result;
    GCode generator;
    REQUIRE_NOTHROW(generator.do_export(&print, file.string().c_str(), &result));
    REQUIRE(result.nonplanar_metadata_valid);
    size_t root_moves = 0, child_moves = 0, infill_moves = 0, support_moves = 0;
    double support_top = 0.;
    bool child_started = false;
    for (const auto &move : result.moves) {
        if (move.type != EMoveType::Extrude || move.delta_extruder <= 0.f)
            continue;
        if (move.cut_segment_id == 0) {
            CHECK_FALSE(child_started);
            ++root_moves;
        } else if (move.cut_segment_id == 1) {
            child_started = true;
            REQUIRE(move.tool_axis_id < result.tool_axes.size());
            CHECK_THAT((result.tool_axes[move.tool_axis_id].cast<double>() - plane.normal).norm(),
                Catch::Matchers::WithinAbs(0., 1e-5));
            CHECK_THAT(move.temperature, Catch::Matchers::WithinAbs(205., 0.01));
            CHECK_THAT(move.fan_speed, Catch::Matchers::WithinAbs(30., 0.5));
            CHECK(move.height > 0.f);
            ++child_moves;
        }
        if (is_infill(move.extrusion_role))
            ++infill_moves;
        if (is_support(move.extrusion_role)) {
            ++support_moves;
            support_top = std::max(support_top, double(move.position.z()));
        }
    }
    CHECK(root_moves > 0);
    CHECK(child_moves > 0);
    CHECK(infill_moves > 0);
    if (supports) {
        CHECK(support_moves > 0);
        CHECK_THAT(support_top, Catch::Matchers::WithinAbs(9.8, 0.021));
    }
    std::ifstream exported(file.string(), std::ios::binary);
    REQUIRE(exported.is_open());
    const std::string text((std::istreambuf_iterator<char>(exported)), std::istreambuf_iterator<char>());
    CHECK(text.find("PHYSICAL_BED_OBJECT_START") == std::string::npos);
    CHECK_NOTHROW(print.validate_nonplanar_gcode(result));
    if (const char *directory = std::getenv("ORCA_TEST_GCODE_ARTIFACT_DIR"); directory && *directory) {
        boost::filesystem::create_directories(directory);
        std::ifstream source(file.string(), std::ios::binary);
        const char *names[] = {"nonplanar-production-solid-upright.gcode", "nonplanar-production-solid-tilted.gcode",
            "nonplanar-production-normal-support.gcode", "nonplanar-production-tree-support.gcode",
            "nonplanar-production-organic-support.gcode"};
        const char *name = names[mode];
        std::ofstream fixture((boost::filesystem::path(directory) / name).string(), std::ios::binary);
        REQUIRE(source.is_open());
        REQUIRE(fixture.is_open());
        fixture << "; Software full-pipeline export fixture only; NOT printer-ready.\n" << source.rdbuf();
        fixture.flush();
        REQUIRE(fixture.good());
    }
}
