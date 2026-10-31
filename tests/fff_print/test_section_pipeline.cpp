#include <catch2/catch_test_macros.hpp>
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

#include <Eigen/Geometry>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <cstddef>

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
