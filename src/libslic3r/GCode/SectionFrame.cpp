#include "SectionFrame.hpp"
#include "../Point.hpp"
#include "../Print.hpp"
#include "../PrintConfig.hpp"
#include "../GCodeWriter.hpp"

#include <Eigen/Geometry>
#include <algorithm>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Slic3r {

std::string begin_section_frame(const PrintInstance &instance, const Vec2d &origin, GCodeWriter &writer)
{
    const PrintObject *object = instance.print_object;
    if (object == nullptr)
        throw std::invalid_argument("A section frame requires a print object");
    if (writer.has_section_transform())
        throw std::logic_error("Close the previous section frame before entering another");
    if (!object->segment())
        return {};
    if (object->print()->config().belt_printer.value)
        throw std::invalid_argument("Section frames require three-axis Cartesian output");
    const auto objects = object->print()->objects();
    const auto found = std::find(objects.begin(), objects.end(), object);
    if (found == objects.end())
        throw std::invalid_argument("The section does not belong to its print");
    const auto index = std::distance(objects.begin(), found);
    if (index > std::numeric_limits<int>::max())
        throw std::overflow_error("Section identity exceeds the G-code metadata range");
    const Transform3d to_world = instance.get_slice_to_world_transform();
    const Vec3d axis = to_world.linear().col(2).normalized();
    std::ostringstream metadata;
    metadata.imbue(std::locale::classic());
    metadata << "; SEGMENT_ID:" << index << "\n; TOOL_AXIS:"
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << axis.x() << ',' << axis.y() << ',' << axis.z() << '\n';
    // Compose out the XY origin already added by point_to_gcode(). Printer
    // plate/nozzle calibration stays outside this rigid geometric transform.
    writer.set_section_transform(to_world * Eigen::Translation3d(-origin.x(), -origin.y(), 0.));
    return metadata.str();
}

std::string end_section_frame(GCodeWriter &writer)
{
    if (!writer.has_section_transform())
        return {};
    writer.set_section_transform(std::nullopt);
    return "; SEGMENT_ID:-1\n; TOOL_AXIS:0,0,1\n";
}

} // namespace Slic3r
