// Section frame binding for three-axis output. AGPL-3.0-or-later.
#pragma once

#include "../Point.hpp"
#include <cstddef>
#include <string>

namespace Slic3r {
class GCodeWriter;
struct PrintInstance;
struct GCodeProcessorResult;

// Bind geometry to one physical copy. The caller writes the returned metadata
// before section motion and closes the frame before global printer motion.
std::string begin_section_frame(const PrintInstance &instance, const Vec2d &origin, GCodeWriter &writer);
std::string end_section_frame(GCodeWriter &writer);
// Track deposited material, not high non-extruding travel. Processor positions
// omit Z calibration, which must be restored for subsequent physical staging.
double max_printed_machine_z(const GCodeProcessorResult &result, size_t first_move,
    double previous_maximum, double z_calibration);
}
