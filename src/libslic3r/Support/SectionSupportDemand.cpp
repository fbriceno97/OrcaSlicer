// Section support-load projection adapted from Unlayered-Slicer v0.1.7.
// AGPL-3.0-or-later. Instance isolation and base-plane projection are fork adaptations.
#include "SectionSupportDemand.hpp"
#include "SegmentSupportBounds.hpp"
#include "../Print.hpp"
#include "../PrintBase.hpp"
#include "../PrintConfig.hpp"
#include "../Exception.hpp"
#include "../I18N.hpp"
#include "../Model.hpp"
#include "../ObjectID.hpp"
#include "../Layer.hpp"
#include "../CutPlaneSegmentation.hpp"
#include "../ClipperUtils.hpp"
#include "../ExPolygon.hpp"
#include "../BoundingBox.hpp"
#include "../Point.hpp"
#include "../Polygon.hpp"
#include "../libslic3r.h"
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Slic3r {
namespace {
struct FramePlane { Vec3d normal; double offset; };

FramePlane plane_in_frame(const CutSegmentClip &clip, const Transform3d &frame)
{
    const double norm2 = clip.normal.squaredNorm();
    if (!frame.matrix().allFinite() || std::abs(frame.linear().determinant()) < 1e-12 ||
        !clip.normal.allFinite() || !std::isfinite(clip.offset) || !std::isfinite(norm2) || norm2 < 1e-12)
        throw std::invalid_argument("Invalid section support plane or frame");
    Vec3d normal = frame.linear().inverse().transpose() * clip.normal;
    if (!normal.allFinite() || normal.norm() < 1e-12)
        throw std::invalid_argument("Degenerate section support plane");
    normal.normalize();
    const Vec3d on_plane = frame * (clip.normal * (clip.offset / norm2));
    const double offset = normal.dot(on_plane);
    if (!std::isfinite(offset))
        throw std::invalid_argument("Nonfinite section support plane offset");
    return {normal, offset};
}

coord_t bounded_strip_coordinate(double mm, coord_t low, coord_t high)
{
    if (std::isnan(mm))
        throw std::invalid_argument("Invalid section support strip");
    if (mm <= unscale<double>(low))
        return low;
    if (mm >= unscale<double>(high))
        return high;
    return scale_(mm);
}
}

double section_support_layer_top(const PrintObject &object)
{
    if (!object.segment())
        throw std::invalid_argument("Section support planning requires a section");
    const auto &segment = *object.segment();
    const double top = std::max(segment.build_z_max, segment.support_z_max);
    if (!object.has_support() || std::none_of(segment.clips.begin(), segment.clips.end(), [](const CutSegmentClip &clip) {
            return clip.role == CutClipRole::Ceiling;
        }))
        return top;
    const PrintConfig &config = object.print()->config();
    double tallest = std::max(object.config().layer_height.value, config.initial_layer_print_height.value);
    for (double height : config.max_layer_height.values)
        tallest = std::max(tallest, height);
    for (double diameter : config.nozzle_diameter.values)
        tallest = std::max(tallest, 0.75 * diameter);
    return std::max(segment.build_z_max, segment.support_z_max + object.config().support_top_z_distance.value + 2. * tallest);
}

namespace {
bool merge_demand(std::vector<Polygons> &destination, const std::vector<Polygons> &source, size_t layers)
{
    if (source.size() > layers)
        throw std::invalid_argument("Section demand does not match the receiving layer schedule");
    if (std::none_of(source.begin(), source.end(), [](const Polygons &polygons) { return !polygons.empty(); }))
        return false;
    destination.resize(layers);
    for (size_t i = 0; i < source.size(); ++i)
        if (!source[i].empty()) {
            polygons_append(destination[i], source[i]);
            destination[i] = union_(destination[i]);
        }
    return true;
}
}

void PrintObject::clear_segment_support_demand()
{
    const bool had_load = !m_segment_support_demand.empty() || !m_segment_body_demand.empty();
    m_segment_support_demand.clear();
    m_segment_body_demand.clear();
    if (had_load)
        invalidate_step(posSupportMaterial);
}

void PrintObject::add_segment_support_demand(const std::vector<Polygons> &demand)
{
    if (!m_segment)
        throw std::invalid_argument("Ordinary objects cannot receive section demand");
    if (merge_demand(m_segment_support_demand, demand, m_layers.size()))
        invalidate_step(posSupportMaterial);
}

void PrintObject::add_segment_body_demand(const std::vector<Polygons> &demand)
{
    if (!m_segment)
        throw std::invalid_argument("Ordinary objects cannot receive section demand");
    if (merge_demand(m_segment_body_demand, demand, m_layers.size()))
        invalidate_step(posSupportMaterial);
}

std::vector<SectionSupportFamily> section_support_families(Print &print)
{
    std::vector<SectionSupportFamily> families;
    std::unordered_map<const ModelObject *, std::unordered_map<size_t, size_t>> lookup;
    for (size_t i = 0; i < print.objects().size(); ++i) {
        PrintObject *object = print.get_object(i);
        if (!object->segment() || object->segment()->part_volume.valid())
            continue;
        const size_t instance = object->nonplanar_instance_key();
        if (instance == 0)
            throw std::invalid_argument("Section support requires a stable model-instance identity");
        auto &instances = lookup[object->model_object()];
        const auto inserted = instances.emplace(instance, families.size());
        if (inserted.second)
            families.emplace_back();
        families[inserted.first->second].push_back(object);
    }
    for (SectionSupportFamily &family : families) {
        std::sort(family.begin(), family.end(), [](const PrintObject *a, const PrintObject *b) {
            return a->segment_index() > b->segment_index();
        });
        for (size_t i = 0; i < family.size(); ++i) {
            const auto &segment = *family[i]->segment();
            if ((i > 0 && family[i - 1]->segment_index() == segment.index) ||
                (!segment.is_root() && segment.parent >= segment.index))
                throw std::invalid_argument("Invalid section support dependency order");
            if (!segment.is_root() && std::none_of(family.begin(), family.end(), [&](const PrintObject *parent) {
                    return parent->segment_index() == segment.parent;
                }))
                throw std::invalid_argument("Section support is missing its parent");
        }
    }
    families.erase(std::remove_if(families.begin(), families.end(), [](const SectionSupportFamily &family) {
        return family.size() < 2;
    }), families.end());
    return families;
}

std::vector<Polygons> project_section_support_demand(const PrintObject &child,
    const PrintObject &receiver, const Polygons &footprint,
    const std::function<void()> &throw_on_cancel)
{
    if (!child.segment() || !receiver.segment() || child.segment()->is_root() ||
        child.segment()->part_volume.valid() || receiver.segment()->part_volume.valid() ||
        child.model_object() != receiver.model_object() || child.print() != receiver.print() ||
        child.nonplanar_instance_key() == 0 || child.nonplanar_instance_key() != receiver.nonplanar_instance_key() ||
        receiver.segment_index() >= child.segment_index())
        throw std::invalid_argument("Support demand must stay within one section instance and flow to an earlier section");
    const auto layers = receiver.layers();
    std::vector<Polygons> demand(layers.size());
    if (throw_on_cancel)
        throw_on_cancel();
    if (footprint.empty() || layers.empty())
        return demand;
    const auto &clips = child.segment()->clips;
    const auto base = std::find_if(clips.begin(), clips.end(), [&](const CutSegmentClip &clip) {
        return clip.role == CutClipRole::Floor && clip.plane_index == child.segment()->base_plane;
    });
    if (base == clips.end())
        throw std::invalid_argument("Support demand requires the child's authored floor plane");
    const FramePlane child_plane = plane_in_frame(*base, child.trafo_sliced());
    // Vertical/downward-facing bounds can clip support, but cannot bear load
    // descending along this child's build direction.
    if (child_plane.normal.z() >= -1e-8)
        return demand;
    const FramePlane receiver_plane = plane_in_frame(*base, receiver.trafo_sliced());
    if (std::abs(receiver_plane.normal.z()) < 1e-8)
        throw std::invalid_argument("A section support plane is perpendicular to the receiver's build direction");
    const double a = -receiver_plane.normal.x() / receiver_plane.normal.z();
    const double b = -receiver_plane.normal.y() / receiver_plane.normal.z();
    const double d = receiver_plane.offset / receiver_plane.normal.z();
    const Transform3d mapping = receiver.trafo_sliced() * child.trafo_sliced().inverse();
    Polygons projected;
    const double coordinate_limit = unscale<double>(std::numeric_limits<coord_t>::max() / 16);
    for (const Polygon &polygon : footprint) {
        if (throw_on_cancel)
            throw_on_cancel();
        if (polygon.empty())
            continue;
        if (polygon.points.size() < 3)
            throw std::invalid_argument("Invalid section support footprint polygon");
        Polygon converted;
        converted.points.reserve(polygon.points.size());
        for (const Point &point : polygon.points) {
            const double x = unscale<double>(point.x()), y = unscale<double>(point.y());
            const double z = (child_plane.offset - child_plane.normal.x() * x - child_plane.normal.y() * y) / child_plane.normal.z();
            const Vec3d image = mapping * Vec3d(x, y, z);
            if (!image.allFinite() || image.cwiseAbs().maxCoeff() > coordinate_limit)
                throw std::invalid_argument("Section support projection exceeds coordinate limits");
            converted.points.push_back(Point::new_scale(image.x(), image.y()));
        }
        projected.push_back(std::move(converted));
    }
    projected = union_(projected);
    const SegmentSupportBounds bounds(receiver);
    projected = bounds.on_plane_inside(projected, a, b, d);
    if (projected.empty())
        return demand;
    const double gradient = std::hypot(a, b);
    if (gradient < 1e-12) {
        Polygons column = projected;
        for (size_t i = layers.size(); i-- > 0;) {
            if (throw_on_cancel)
                throw_on_cancel();
            if (layers[i]->print_z > d + EPSILON)
                continue;
            column = diff(column, to_polygons(layers[i]->lslices));
            if (column.empty())
                break;
            demand[i] = column;
        }
        return demand;
    }
    const double angle = std::atan2(b, a);
    for (Polygon &polygon : projected)
        polygon.rotate(-angle);
    const BoundingBox box = get_extents(projected);
    Polygons column;
    for (size_t i = layers.size(); i-- > 0;) {
        if (throw_on_cancel)
            throw_on_cancel();
        const coord_t low = bounded_strip_coordinate((layers[i]->print_z - d) / gradient, box.min.x(), box.max.x());
        const coord_t high = i + 1 < layers.size() ?
            bounded_strip_coordinate((layers[i + 1]->print_z - d) / gradient, box.min.x(), box.max.x()) : box.max.x();
        const Polygons material = to_polygons(layers[i]->lslices);
        if (high > low) {
            Polygon strip;
            strip.points = {Point(low, box.min.y()), Point(high, box.min.y()),
                Point(high, box.max.y()), Point(low, box.max.y())};
            Polygons band = intersection(projected, Polygons{strip});
            for (Polygon &polygon : band)
                polygon.rotate(angle);
            polygons_append(column, diff(band, material));
        }
        if (!column.empty()) {
            column = diff(union_(column), material);
            demand[i] = column;
        }
    }
    return demand;
}
}

namespace Slic3r {
void generate_section_support_family(const SectionSupportFamily &family,
    const std::function<void()> &throw_on_cancel)
{
    if (family.empty())
        return;
    const auto check_cancel = [&] {
        if (family.front()->print()->canceled())
            throw CanceledException();
        if (throw_on_cancel)
            throw_on_cancel();
    };
    for (PrintObject *object : family) {
        check_cancel();
        if (object->has_support() && is_tree(object->config().support_type.value))
            throw SlicingError(_u8L("Plane sections with inherited support load currently require normal supports."));
        object->slice();
        object->make_perimeters();
        object->prepare_infill();
        object->clear_segment_support_demand();
        object->invalidate_step(posSupportMaterial);
    }
    for (PrintObject *child : family) {
        check_cancel();
        if (!child->segment()->is_root() && !child->segment()->touches_base_plane() && child->has_support())
            throw SlicingError(_u8L("Support below a section's floating base is not supported."));
        child->slicing_parameters();
        child->generate_support_material(false);
        if (child->segment()->is_root())
            continue;
        const double window = 2. * std::max(0.01, child->config().layer_height.value);
        Polygons support, body;
        if (!child->support_layers().empty()) {
            double lowest = std::numeric_limits<double>::max();
            for (const SupportLayer *layer : child->support_layers())
                lowest = std::min(lowest, layer->print_z);
            for (const SupportLayer *layer : child->support_layers())
                if (layer->print_z <= lowest + window)
                    polygons_append(support, to_polygons(layer->support_islands));
        }
        if (!child->layers().empty()) {
            const double lowest = child->layers().front()->print_z;
            for (const Layer *layer : child->layers()) {
                if (layer->print_z > lowest + window)
                    break;
                polygons_append(body, to_polygons(layer->lslices));
            }
        }
        support = union_(support);
        body = union_(body);
        for (PrintObject *receiver : family) {
            check_cancel();
            if (receiver->segment_index() >= child->segment_index() || !is_auto(receiver->config().support_type.value))
                continue;
            if (!support.empty())
                receiver->add_segment_support_demand(project_section_support_demand(*child, *receiver, support, check_cancel));
            if (!body.empty())
                receiver->add_segment_body_demand(project_section_support_demand(*child, *receiver, body, check_cancel));
        }
    }
}
}
