#include <catch2/catch_all.hpp>

#include "libslic3r/HotendEnvelope.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

using namespace Slic3r;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double pi = 3.14159265358979323846;
}

TEST_CASE("Upright hotend clearance uses the face opposite the build direction", "[HotendEnvelope]")
{
    HotendEnvelope envelope;
    envelope.angle = {80., 30., 60., 45.};
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., 0.), WithinAbs(30., 1e-9));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., pi), WithinAbs(80., 1e-9));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., pi / 2.), WithinAbs(45., 1e-9));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., -pi / 2.), WithinAbs(60., 1e-9));
    CHECK(virtual_tilt_binding_face(envelope, 0.) == HotendEnvelope::XNeg);
    CHECK(virtual_tilt_binding_face(envelope, pi) == HotendEnvelope::XPos);
}

TEST_CASE("A hotend corner constrains diagonal tilt more than a face", "[HotendEnvelope]")
{
    HotendEnvelope envelope;
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., 0.), WithinAbs(45., 1e-9));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0., pi / 4.), WithinAbs(35.264389682754654, 1e-9));
}

TEST_CASE("An unknown build direction uses the most restrictive hotend corner", "[HotendEnvelope]")
{
    HotendEnvelope envelope;
    envelope.angle = {80., 30., 60., 45.};
    const double worst = virtual_tilt_limit_deg(envelope, 0., 0.);
    for (int i = 0; i < 3600; ++i)
        CHECK(worst <= virtual_tilt_limit_deg(envelope, 0., 0., 2. * pi * i / 3600.) + 1e-9);
}

TEST_CASE("Gantry clearance limits tilt even when the hotend model is off", "[HotendEnvelope]")
{
    HotendEnvelope envelope;
    envelope.height = 0.;
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 40., 80.), WithinAbs(26.56505117707799, 1e-9));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 80.), WithinAbs(0., 1e-12));
    envelope.height = 40.;
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 40., 80.), WithinAbs(26.56505117707799, 1e-9));
    // A tiny physical clearance must not be rounded up to an artificial minimum angle.
    CHECK(virtual_tilt_limit_deg(envelope, 0.001, 1000.) < 0.001);
}

TEST_CASE("Vertical hotend faces have zero lateral spread", "[HotendEnvelope]")
{
    HotendEnvelope envelope;
    envelope.angle.fill(90.);
    CHECK_THAT(envelope.spread(HotendEnvelope::XPos), WithinAbs(0., 1e-12));
    CHECK_THAT(virtual_tilt_limit_deg(envelope, 0., 0.), WithinAbs(90., 1e-9));
}

TEST_CASE("Invalid clearance geometry is rejected instead of removing a limit", "[HotendEnvelope]")
{
    const double invalid = GENERATE(-1., std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::quiet_NaN());
    HotendEnvelope envelope;
    CHECK_THROWS_AS(virtual_tilt_limit_deg(envelope, invalid, 20.), std::invalid_argument);
    CHECK_THROWS_AS(virtual_tilt_limit_deg(envelope, 20., invalid), std::invalid_argument);
    envelope.height = invalid;
    CHECK_THROWS_AS(virtual_tilt_limit_deg(envelope, 20., 20.), std::invalid_argument);
}

TEST_CASE("Invalid face angles and nonfinite directions are rejected", "[HotendEnvelope]")
{
    const double invalid_angle = GENERATE(0., 91., std::numeric_limits<double>::infinity(),
                                          std::numeric_limits<double>::quiet_NaN());
    HotendEnvelope envelope;
    envelope.angle[HotendEnvelope::YPos] = invalid_angle;
    CHECK_THROWS_AS(virtual_tilt_limit_deg(envelope, 0., 0., 0.), std::invalid_argument);
    envelope.angle.fill(45.);
    CHECK_THROWS_AS(virtual_tilt_limit_deg(envelope, 0., 0., std::numeric_limits<double>::quiet_NaN()),
                    std::invalid_argument);
}
