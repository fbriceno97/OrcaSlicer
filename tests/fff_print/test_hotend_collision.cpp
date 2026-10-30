#include <catch2/catch_all.hpp>

#include "libslic3r/GCode/HotendCollisionCheck.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Point.hpp"

#include <limits>
#include <stdexcept>

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
