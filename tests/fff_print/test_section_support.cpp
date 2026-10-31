#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "libslic3r/Support/SectionSupportDemand.hpp"
#include "libslic3r/CutPlanes.hpp"
#include "libslic3r/CutPlaneSegmentation.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode.hpp"
#include "libslic3r/ExtrusionEntityCollection.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/libslic3r.h"
#include "test_helpers.hpp"
#include <Eigen/Geometry>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cstddef>
#include <initializer_list>

using namespace Slic3r;

TEST_CASE("Support families keep equally transformed copies separate and visit children first", "[Nonplanar][Support]")
{
    const double z_difference = GENERATE(0., 1e-10);
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict("enable_nonplanar", "1");
    Print print;
    Model model;
    Slic3r::Test::init_print({Slic3r::Test::cube(20.)}, print, model, config);
    ModelObject *object = model.objects.front();
    object->add_instance(*object->instances.front())->set_offset(Vec3d(90., 90., z_difference));
    object->cut_planes.enabled = true;
    CutPlane plane;
    plane.point = Vec3d(10., 10., 10.);
    object->cut_planes.planes.push_back(plane);
    print.apply(model, config);
    const auto families = section_support_families(print);
    REQUIRE(families.size() == 2);
    for (const auto &family : families) {
        REQUIRE(family.size() == 2);
        CHECK(family[0]->segment_index() == 1);
        CHECK(family[1]->segment_index() == 0);
        CHECK(family[0]->nonplanar_instance_key() == family[1]->nonplanar_instance_key());
    }
    CHECK(families[0][0]->nonplanar_instance_key() != families[1][0]->nonplanar_instance_key());
    CHECK_THROWS_AS(project_section_support_demand(*families[0][0], *families[1][1], {}), std::invalid_argument);
    config.set_deserialize_strict("enable_nonplanar", "0");
    print.apply(model, config);
    CHECK(section_support_families(print).empty());
}

TEST_CASE("Support projection stops at material that has caught the load", "[Nonplanar][Support]")
{
    const bool middle_shelf = GENERATE(false, true);
    const bool independent_build_direction = GENERATE(false, true);
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}, {"layer_height", "0.2"},
        {"initial_layer_print_height", "0.2"}, {"elefant_foot_compensation", "0"}});
    TriangleMesh mesh = make_cube(4., 10., 10.);
    TriangleMesh cap = make_cube(20., 10., 2.);
    cap.translate(0.f, 0.f, 10.f);
    mesh.merge(cap);
    if (middle_shelf) {
        TriangleMesh shelf = make_cube(20., 10., 2.);
        shelf.translate(0.f, 0.f, 5.f);
        mesh.merge(shelf);
    }
    Print print;
    Model model;
    Slic3r::Test::init_print({mesh}, print, model, config);
    ModelObject *object = model.objects.front();
    object->cut_planes.enabled = true;
    CutPlane plane;
    plane.point = Vec3d(2., 5., 10.);
    if (independent_build_direction) {
        plane.has_build_normal = true;
        plane.build_normal = Vec3d(0., 0.3, 1.).normalized();
    }
    object->cut_planes.planes.push_back(plane);
    print.apply(model, config);
    const auto families = section_support_families(print);
    REQUIRE(families.size() == 1);
    REQUIRE(families.front().size() == 2);
    PrintObject &child = *families.front()[0];
    PrintObject &receiver = *families.front()[1];
    child.slice();
    receiver.slice();
    // A full base-face footprint expressed as XY in the child's slicing frame.
    // Its omitted Z varies when the build and cutting normals differ.
    Polygon footprint;
    for (const Vec3d &corner : {Vec3d(0., 0., 10.), Vec3d(20., 0., 10.),
            Vec3d(20., 10., 10.), Vec3d(0., 10., 10.)}) {
        const Vec3d point = child.trafo_sliced() * corner;
        footprint.points.push_back(Point::new_scale(point.x(), point.y()));
    }
    const auto demand = project_section_support_demand(child, receiver, {footprint});
    REQUIRE(demand.size() == receiver.layers().size());
    size_t loaded_layers = 0;
    for (size_t i = 0; i < demand.size(); ++i) {
        const double z = receiver.layers()[i]->print_z;
        if (middle_shelf && z <= 7. + EPSILON)
            CHECK(demand[i].empty());
        else {
            CHECK_FALSE(demand[i].empty());
            const double mm2 = std::abs(area(demand[i])) * SCALING_FACTOR * SCALING_FACTOR;
            CHECK_THAT(mm2, Catch::Matchers::WithinAbs(160., 0.02));
            ++loaded_layers;
        }
    }
    CHECK(loaded_layers > 0);
    CHECK_THROWS_AS(project_section_support_demand(child, receiver, {footprint}, [] {
        throw std::runtime_error("cancel support projection");
    }), std::runtime_error);
}

TEST_CASE("A parent generates support for inherited body load and rebuilds it without stale demand", "[Nonplanar][Support]")
{
    const bool child_needs_support = GENERATE(false, true);
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"enable_nonplanar", "1"}, {"enable_support", "1"},
        {"support_type", "normal(auto)"}, {"layer_height", "0.2"},
        {"initial_layer_print_height", "0.2"}, {"support_top_z_distance", "0.2"},
        {"independent_support_layer_height", "1"}, {"elefant_foot_compensation", "0"}});
    TriangleMesh mesh = make_cube(4., 10., child_needs_support ? 15. : 10.);
    TriangleMesh cap = make_cube(20., 10., 2.);
    cap.translate(0.f, 0.f, child_needs_support ? 15.f : 10.f);
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
    auto families = section_support_families(print);
    REQUIRE(families.size() == 1);
    PrintObject &receiver = *families.front().back();
    generate_section_support_family(families.front());
    if (child_needs_support) {
        CHECK_FALSE(receiver.segment_support_demand().empty());
        CHECK(receiver.segment_body_demand().empty());
    } else
        CHECK_FALSE(receiver.segment_body_demand().empty());
    REQUIRE_FALSE(receiver.support_layers().empty());
    CHECK(receiver.layers().back()->print_z > 10.4);
    double highest_support = 0., volume = 0.;
    for (const SupportLayer *layer : receiver.support_layers())
        if (!layer->support_fills.empty()) {
            highest_support = std::max(highest_support, layer->print_z);
            volume += layer->support_fills.total_volume();
        }
    CHECK(volume > 0.);
    CHECK_THAT(highest_support, Catch::Matchers::WithinAbs(child_needs_support ? 10. : 9.8, 0.021));
    struct Collector : GCode { using GCode::collect_layers_to_print; };
    const auto emitted = Collector::collect_layers_to_print(receiver);
    REQUIRE_FALSE(emitted.empty());
    for (const auto &layer : emitted) {
        CHECK(layer.print_z() <= 10. + EPSILON);
        CHECK(((layer.object_layer && layer.object_layer->has_extrusions()) ||
            (layer.support_layer && layer.support_layer->has_extrusions())));
    }
    generate_section_support_family(families.front());
    double rebuilt_volume = 0.;
    for (const SupportLayer *layer : receiver.support_layers())
        rebuilt_volume += layer->support_fills.total_volume();
    CHECK_THAT(rebuilt_volume, Catch::Matchers::WithinAbs(volume, 0.001));
    config.set_deserialize_strict("support_top_z_distance", "0.4");
    print.apply(model, config);
    families = section_support_families(print);
    REQUIRE(families.size() == 1);
    CHECK_FALSE(families.front().back()->is_step_done(posSlice));
    generate_section_support_family(families.front());
    double changed_contact = 0.;
    for (const SupportLayer *layer : families.front().back()->support_layers())
        if (!layer->support_fills.empty())
            changed_contact = std::max(changed_contact, layer->print_z);
    CHECK_THAT(changed_contact, Catch::Matchers::WithinAbs(child_needs_support ? 10. : 9.6, 0.021));
    config.set_deserialize_strict("enable_support", "0");
    print.apply(model, config);
    families = section_support_families(print);
    REQUIRE(families.size() == 1);
    PrintObject &without_support = *families.front().back();
    CHECK_FALSE(without_support.is_step_done(posSlice));
    without_support.slice();
    CHECK_THAT(without_support.layers().back()->print_z, Catch::Matchers::WithinAbs(10., 0.001));
    CHECK(without_support.segment_body_demand().empty());
}
