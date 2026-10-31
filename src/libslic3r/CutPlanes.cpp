#include "CutPlanes.hpp"
#include "Point.hpp"
#include "nlohmann/json.hpp"

#include <stdexcept>
#include <string>

namespace Slic3r {
namespace {
nlohmann::json vector_json(const Vec3d &vector)
{
    if (!vector.allFinite())
        throw std::invalid_argument("Cut-plane coordinates must be finite");
    return {vector.x(), vector.y(), vector.z()};
}

Vec3d read_vector(const nlohmann::json &json)
{
    if (!json.is_array() || json.size() != 3)
        throw std::invalid_argument("A cut-plane vector must have three coordinates");
    const Vec3d vector(json.at(0).get<double>(), json.at(1).get<double>(), json.at(2).get<double>());
    if (!vector.allFinite())
        throw std::invalid_argument("Cut-plane coordinates must be finite");
    return vector;
}
} // namespace

std::string cut_plane_stack_to_json(const CutPlaneStack &stack)
{
    nlohmann::json planes = nlohmann::json::array();
    for (const CutPlane &plane : stack.planes)
        planes.push_back({{"point", vector_json(plane.point)}, {"normal", vector_json(plane.normal)},
                          {"build_normal", vector_json(plane.build_normal)},
                          {"has_build_normal", plane.has_build_normal}, {"enabled", plane.enabled}});
    return nlohmann::json{{"version", 1}, {"enabled", stack.enabled},
                           {"user_ordered", stack.user_ordered}, {"planes", planes}}.dump();
}

CutPlaneStack cut_plane_stack_from_json(const std::string &text)
{
    const nlohmann::json json = nlohmann::json::parse(text);
    if (!json.is_object() || json.at("version") != 1 || !json.at("planes").is_array())
        throw std::invalid_argument("Unsupported cut-plane metadata format");
    CutPlaneStack stack;
    stack.enabled = json.at("enabled").get<bool>();
    stack.user_ordered = json.at("user_ordered").get<bool>();
    for (const nlohmann::json &item : json.at("planes")) {
        CutPlane plane;
        plane.point = read_vector(item.at("point"));
        plane.normal = read_vector(item.at("normal"));
        plane.build_normal = read_vector(item.at("build_normal"));
        plane.has_build_normal = item.at("has_build_normal").get<bool>();
        plane.enabled = item.at("enabled").get<bool>();
        stack.planes.push_back(plane);
    }
    return stack;
}
} // namespace Slic3r
