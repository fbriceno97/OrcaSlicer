// Section support-load projection adapted from Unlayered-Slicer v0.1.7.
// AGPL-3.0-or-later.
#pragma once

#include "../Polygon.hpp"
#include <functional>
#include <vector>

namespace Slic3r {
class Print;
class PrintObject;

using SectionSupportFamily = std::vector<PrintObject *>;
// Plane-partition members of one model instance, in child-first order.
// Directed parts have no authored parent plane and remain independent.
std::vector<SectionSupportFamily> section_support_families(Print &print);

// Reserve contact-planning layers above ceilings only when supports are enabled.
double section_support_layer_top(const PrintObject &object);
// Generate plane-family supports child first, accumulating both load types.
void generate_section_support_family(const SectionSupportFamily &family,
    const std::function<void()> &throw_on_cancel = {});

// Carry a child's footprint down through an earlier section's empty space.
// Footprint vertices are lifted onto the actual base plane before changing
// frames, including when the build direction differs from the cutting normal.
// Once material catches load it never reappears on lower layers.
std::vector<Polygons> project_section_support_demand(const PrintObject &child,
    const PrintObject &receiver, const Polygons &footprint,
    const std::function<void()> &throw_on_cancel = {});
}
