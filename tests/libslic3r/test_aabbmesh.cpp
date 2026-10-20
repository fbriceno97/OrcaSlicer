#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "libslic3r/AABBMesh.hpp"
#include "libslic3r/TriangleMesh.hpp"

using namespace Slic3r;

TEST_CASE("Painting retains coincident crossings of overlapping closed shells", "[AABBMesh]")
{
    TriangleMesh mesh = make_cube(1., 1., 1.);
    TriangleMesh inset = make_cube(1., 1., 0.5);
    inset.translate(0., 0., 0.5);
    mesh.merge(inset);
    const AABBMesh raycaster(mesh);
    // Avoid triangle edges. The shells share their top face at z = 1.
    const Vec3d origin(0.23, 0.37, -1.);
    const Vec3d direction(0., 0., 1.);
    const auto legacy_hits = raycaster.query_ray_hits(origin, direction);
    const auto paint_hits = raycaster.query_ray_hits(origin, direction, true);

    REQUIRE(legacy_hits.size() == 3);
    REQUIRE(paint_hits.size() == 4);
    REQUIRE(legacy_hits.size() % 2 == 1);
    REQUIRE(paint_hits.size() % 2 == 0);
    REQUIRE_THAT(paint_hits.front().position().z(), Catch::Matchers::WithinAbs(0., 1e-6));
    REQUIRE_THAT(paint_hits.back().position().z(), Catch::Matchers::WithinAbs(1., 1e-6));
    REQUIRE(paint_hits[2].face() != paint_hits[3].face());
}

TEST_CASE("Painting preserves ordinary closed mesh ray crossings", "[AABBMesh]")
{
    const TriangleMesh mesh = make_cube(1., 1., 1.);
    const AABBMesh raycaster(mesh);
    const Vec3d origin(0.23, 0.37, -1.);
    const Vec3d direction(0., 0., 1.);
    const auto legacy_hits = raycaster.query_ray_hits(origin, direction);
    const auto paint_hits = raycaster.query_ray_hits(origin, direction, true);
    REQUIRE(legacy_hits.size() == 2);
    REQUIRE(paint_hits.size() == legacy_hits.size());
    for (size_t i = 0; i < paint_hits.size(); ++i)
        REQUIRE(paint_hits[i].face() == legacy_hits[i].face());
    REQUIRE(raycaster.query_ray_hits(Vec3d(2., 2., -1.), direction, true).empty());
}
