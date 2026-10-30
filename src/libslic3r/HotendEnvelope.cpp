// Hotend-envelope geometry adapted from Unlayered-Slicer v0.1.7.
// Released under the AGPLv3 or higher.
#include "HotendEnvelope.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace Slic3r {
namespace {
constexpr double pi = 3.14159265358979323846;

std::size_t side_index(HotendEnvelope::Side side)
{
    const int index = static_cast<int>(side);
    if (index < 0 || index >= 4)
        throw std::invalid_argument("Invalid hotend envelope side");
    return static_cast<std::size_t>(index);
}

void require_nonnegative_finite(double value)
{
    if (!std::isfinite(value) || value < 0.)
        throw std::invalid_argument("Hotend clearance dimensions must be finite and nonnegative");
}

struct UphillFaces {
    HotendEnvelope::Side x, y;
    double wx, wy;
};

UphillFaces uphill_faces(double azimuth)
{
    if (!std::isfinite(azimuth))
        throw std::invalid_argument("Hotend clearance azimuth must be finite");
    const double x = -std::cos(azimuth), y = -std::sin(azimuth);
    return {x > 0. ? HotendEnvelope::XPos : HotendEnvelope::XNeg,
            y > 0. ? HotendEnvelope::YPos : HotendEnvelope::YNeg,
            std::abs(x), std::abs(y)};
}
} // namespace

bool HotendEnvelope::enabled() const
{
    require_nonnegative_finite(height);
    return height > 0.;
}

double HotendEnvelope::spread(Side side) const
{
    const double degrees = angle[side_index(side)];
    if (!std::isfinite(degrees) || degrees < 1. || degrees > 90.)
        throw std::invalid_argument("Hotend face angles must be finite and between 1 and 90 degrees");
    if (degrees == 90.)
        return 0.;
    const double radians = degrees * pi / 180.;
    return std::cos(radians) / std::sin(radians);
}

const char *HotendEnvelope::option_key(Side side)
{
    static constexpr const char *keys[] = {"nonplanar_hotend_angle_xpos", "nonplanar_hotend_angle_xneg",
                                            "nonplanar_hotend_angle_ypos", "nonplanar_hotend_angle_yneg"};
    return keys[side_index(side)];
}

const char *HotendEnvelope::side_name(Side side)
{
    static constexpr const char *names[] = {"+X", "-X", "+Y", "-Y"};
    return names[side_index(side)];
}

double virtual_tilt_hotend_term_deg(const HotendEnvelope &envelope, std::optional<double> azimuth)
{
    // Validate every face, even if its contribution in this direction is zero.
    const double xp = envelope.spread(HotendEnvelope::XPos), xn = envelope.spread(HotendEnvelope::XNeg);
    const double yp = envelope.spread(HotendEnvelope::YPos), yn = envelope.spread(HotendEnvelope::YNeg);
    if (azimuth)
        uphill_faces(*azimuth); // Reject NaN even when the envelope is explicitly off.
    if (!envelope.enabled())
        return 90.;
    double reach;
    if (azimuth) {
        const UphillFaces faces = uphill_faces(*azimuth);
        reach = faces.wx * envelope.spread(faces.x) + faces.wy * envelope.spread(faces.y);
    } else {
        // Within a quadrant the furthest corner is hypot(x_spread, y_spread).
        reach = std::hypot(std::max(xp, xn), std::max(yp, yn));
    }
    return std::atan2(1., reach) * 180. / pi;
}

HotendEnvelope::Side virtual_tilt_binding_face(const HotendEnvelope &envelope, std::optional<double> azimuth)
{
    if (azimuth) {
        const UphillFaces faces = uphill_faces(*azimuth);
        return faces.wx * envelope.spread(faces.x) >= faces.wy * envelope.spread(faces.y) ? faces.x : faces.y;
    }
    HotendEnvelope::Side worst = HotendEnvelope::XPos;
    for (int s = 1; s < 4; ++s)
        if (envelope.spread(HotendEnvelope::Side(s)) > envelope.spread(worst))
            worst = HotendEnvelope::Side(s);
    return worst;
}

double virtual_tilt_limit_deg(const HotendEnvelope &envelope, double gantry_height_mm, double reach_mm,
                              std::optional<double> azimuth)
{
    require_nonnegative_finite(gantry_height_mm);
    require_nonnegative_finite(reach_mm);
    double limit = virtual_tilt_hotend_term_deg(envelope, azimuth);
    if (!envelope.enabled() && gantry_height_mm == 0.)
        return 0.;
    if (gantry_height_mm > 0. && reach_mm > 0.)
        limit = std::min(limit, std::atan2(gantry_height_mm, reach_mm) * 180. / pi);
    return limit;
}
} // namespace Slic3r
