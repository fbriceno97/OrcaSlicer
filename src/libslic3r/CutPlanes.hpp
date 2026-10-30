// Declarative cut-plane data adapted from Unlayered-Slicer v0.1.7.
// Released under the AGPLv3 or higher.
#pragma once

#include "Point.hpp"
#include <vector>

namespace Slic3r {

// Object-local geometry; authoring a plane does not destructively cut the mesh.
struct CutPlane
{
    Vec3d point {Vec3d::Zero()};
    Vec3d normal {Vec3d::UnitZ()};
    Vec3d build_normal {Vec3d::Zero()};
    bool has_build_normal {false};
    bool enabled {true};

    Vec3d effective_build_normal() const { return has_build_normal ? build_normal : normal; }
    bool operator==(const CutPlane &rhs) const {
        return point == rhs.point && normal == rhs.normal && build_normal == rhs.build_normal
            && has_build_normal == rhs.has_build_normal && enabled == rhs.enabled;
    }
    bool operator!=(const CutPlane &rhs) const { return !(*this == rhs); }
    template<class Archive> void serialize(Archive &ar) { ar(point, normal, build_normal, has_build_normal, enabled); }
};

struct CutPlaneStack
{
    bool enabled {false};
    bool user_ordered {false};
    std::vector<CutPlane> planes;

    bool empty() const { return planes.empty(); }
    bool active() const { return enabled && !planes.empty(); }
    bool operator==(const CutPlaneStack &rhs) const {
        return enabled == rhs.enabled && user_ordered == rhs.user_ordered && planes == rhs.planes;
    }
    bool operator!=(const CutPlaneStack &rhs) const { return !(*this == rhs); }
    template<class Archive> void serialize(Archive &ar) { ar(enabled, user_ordered, planes); }
};
} // namespace Slic3r
