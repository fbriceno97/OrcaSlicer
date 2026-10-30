// Hotend-envelope geometry adapted from Unlayered-Slicer v0.1.7.
// Released under the AGPLv3 or higher.
#pragma once

#include <array>
#include <optional>

namespace Slic3r {

class ConfigBase;

// Four faces measured up from the bed plane with the nozzle vertical. These
// generic defaults describe a model, not measured clearance for a printer.
struct HotendEnvelope
{
    enum Side { XPos = 0, XNeg, YPos, YNeg };
    double height = 40.;
    std::array<double, 4> angle {45., 45., 45., 45.};

    bool enabled() const;
    double spread(Side side) const;
    static HotendEnvelope from_config(const ConfigBase &config);
    static const char *option_key(Side side);
    static const char *side_name(Side side);
};

// A missing azimuth selects the most restrictive direction. The layer climbs
// opposite the build normal's lean. Zero height explicitly disables the envelope;
// zero gantry height means unmeasured. A zero combined limit means neither model
// is enabled, not permission to skip physical clearance validation.
double virtual_tilt_hotend_term_deg(const HotendEnvelope &envelope,
                                   std::optional<double> tilt_azimuth_rad = std::nullopt);
HotendEnvelope::Side virtual_tilt_binding_face(const HotendEnvelope &envelope,
                                             std::optional<double> tilt_azimuth_rad = std::nullopt);
double virtual_tilt_limit_deg(const HotendEnvelope &envelope, double gantry_height_mm, double reach_mm,
                              std::optional<double> tilt_azimuth_rad = std::nullopt);

} // namespace Slic3r
